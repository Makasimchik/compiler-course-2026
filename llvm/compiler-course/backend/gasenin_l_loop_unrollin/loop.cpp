#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include <algorithm>

using namespace llvm;

namespace {

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    auto &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();

    bool Changed = false;
    SmallVector<MachineLoop *, 8> Loops;

    for (MachineLoop *L : MLI)
      collectLoops(L, Loops);

    for (MachineLoop *L : Loops)
      Changed |= unrollLoop(L, MF, MLI);

    return Changed;
  }

  void collectLoops(MachineLoop *L, SmallVectorImpl<MachineLoop *> &Loops) {
    for (MachineLoop *SubLoop : *L)
      collectLoops(SubLoop, Loops);
    Loops.push_back(L);
  }

  bool hasUniquePreheader(MachineLoop *L) {
    MachineBasicBlock *Header = L->getHeader();
    if (!Header)
      return false;

    MachineBasicBlock *Preheader = nullptr;

    for (MachineBasicBlock *Pred : Header->predecessors()) {
      if (L->contains(Pred))
        continue;

      if (Preheader)
        return false;

      Preheader = Pred;
    }

    return Preheader != nullptr;
  }

  MachineBasicBlock *getUniqueExitingBlock(MachineLoop *L) {
    MachineBasicBlock *Exiting = nullptr;

    for (MachineBasicBlock *MBB : L->blocks()) {
      bool HasOutsideSucc = false;

      for (MachineBasicBlock *Succ : MBB->successors()) {
        if (!L->contains(Succ)) {
          HasOutsideSucc = true;
          break;
        }
      }

      if (!HasOutsideSucc)
        continue;

      if (Exiting)
        return nullptr;

      Exiting = MBB;
    }

    return Exiting;
  }

  bool unrollLoop(MachineLoop *L, MachineFunction &MF, MachineLoopInfo &MLI) {
    constexpr int MaxTripCount = 1024;
    constexpr int MaxUnrollFactor = 4;

    MachineBasicBlock *Latch = L->getLoopLatch();
    MachineBasicBlock *Exiting = getUniqueExitingBlock(L);

    if (!hasUniquePreheader(L) || !Latch || !Exiting || Exiting != Latch)
      return false;

    int TripCount = getTripCount(Latch);
    if (TripCount <= 1 || TripCount > MaxTripCount)
      return false;

    int Factor = getUnrollFactor(TripCount, MaxUnrollFactor);
    if (Factor <= 1)
      return false;

    SmallVector<MachineInstr *, 16> Body;

    for (MachineBasicBlock *MBB : L->blocks()) {
      if (MLI.getLoopFor(MBB) != L)
        continue;

      for (MachineInstr &MI : *MBB) {
        if (MI.isBranch() || MI.isTerminator() || MI.isDebugInstr())
          continue;

        Body.push_back(&MI);
      }
    }

    if (Body.empty())
      return false;

    MachineBasicBlock::iterator InsertPt = Latch->getFirstTerminator();

    bool HasSubLoops = L->begin() != L->end();
    int Copies = HasSubLoops ? Factor : Factor - 1;

    for (int I = 0; I < Copies; ++I) {
      for (MachineInstr *MI : Body) {
        MachineInstr *Cloned = MF.CloneMachineInstr(MI);
        Latch->insert(InsertPt, Cloned);
      }
    }

    updateInductionStep(Latch, Factor);
    return true;
  }

  int getUnrollFactor(int TripCount, int MaxFactor) {
    for (int Factor = std::min(MaxFactor, TripCount); Factor > 1; --Factor) {
      if (TripCount % Factor == 0)
        return Factor;
    }

    return 1;
  }

  int getTripCount(MachineBasicBlock *Latch) {
    for (MachineInstr &MI : *Latch) {
      if (MI.getOpcode() != X86::CMP32ri8 && MI.getOpcode() != X86::CMP32ri)
        continue;

      for (MachineOperand &Op : MI.operands()) {
        if (Op.isImm())
          return static_cast<int>(Op.getImm());
      }
    }

    return -1;
  }

  void updateInductionStep(MachineBasicBlock *Latch, int Factor) {
    for (MachineInstr &MI : *Latch) {
      if (MI.getOpcode() != X86::ADD32ri8)
        continue;

      for (MachineOperand &Op : MI.operands()) {
        if (Op.isImm() && Op.getImm() == 1)
          Op.setImm(Factor);
      }
    }
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
  }
};

} // namespace

char ExamplePass::ID = 0;

static RegisterPass<ExamplePass> X("example-x86", "simple loop unroll pass",
                                   false, false);
