#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

using namespace llvm;

namespace {
class LoopUnrollPass : public MachineFunctionPass {
  static constexpr int maxUnrollingIters = 5;
  const X86InstrInfo *TII = nullptr;

public:
  void FillLoops(MachineLoop *Loop, SmallVectorImpl<MachineLoop *> &Loops) {
    for (MachineLoop *SubLoop : *Loop)
      FillLoops(SubLoop, Loops);
    Loops.push_back(Loop);
  }

  bool hasSinglePreheaderBlock(MachineLoop *Loop) {
    auto *Hdr = Loop->getHeader();
    if (!Hdr)
      return false;

    MachineBasicBlock *Candidate = nullptr;

    for (auto *Pred : Hdr->predecessors()) {
      if (Loop->contains(Pred))
        continue;

      if (Candidate != nullptr)
        return false;

      Candidate = Pred;
    }

    return Candidate != nullptr;
  }

  MachineBasicBlock *findSingleExitBlock(MachineLoop *Loop) {
    MachineBasicBlock *Result = nullptr;

    for (auto *Block : Loop->blocks()) {
      bool exitsLoop =
          llvm::any_of(Block->successors(), [&](MachineBasicBlock *Succ) {
            return !Loop->contains(Succ);
          });

      if (!exitsLoop)
        continue;

      // если уже находили такой блок — значит он не единственный
      if (Result != nullptr)
        return nullptr;

      Result = Block;
    }

    return Result;
  }

  static char ID;
  LoopUnrollPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

  unsigned getTripCount(MachineLoop *Loop) {
    // сначала пробуем latch, потом header
    MachineBasicBlock *Latch = Loop->getLoopLatch();
    MachineBasicBlock *Header = Loop->getHeader();

    for (MachineBasicBlock *BB : {Latch, Header}) {
      if (!BB)
        continue;
      for (auto &MI : reverse(*BB)) {
        if (MI.getOpcode() == X86::CMP32ri || MI.getOpcode() == X86::CMP32ri8) {
          for (auto &MO : MI.operands()) {
            if (MO.isImm()) {
              return (unsigned)(MO.getImm() + 1);
            }
          }
        }
      }
    }
    return 0;
  }

  bool unrollLoop(MachineLoop *Loop, unsigned Count, MachineFunction &MF,
                  MachineLoopInfo &MLI) {
    if (Count <= 1)
      return false;
    MachineBasicBlock *Latch = Loop->getLoopLatch();
    MachineBasicBlock *Exiting = findSingleExitBlock(Loop);

    if (!hasSinglePreheaderBlock(Loop) || !Latch || !Exiting) {
      return false;
    }

    // ищем число итераций в одном блоке
    int itersInBlock = 1;
    for (int div = maxUnrollingIters; div > 0; div--) {
      if (Count % div == 0) {
        itersInBlock = div;
        break;
      }
    }
    if (itersInBlock == 1) {
      return false;
    }

    SmallVector<MachineInstr *, 16> LoopBody;

    // ищем инструкции которые будем копировать
    for (auto &MBB : Loop->blocks()) {
      for (auto &MI : *MBB) {
        if (MI.isBranch() || MI.isTerminator() || MI.isDebugInstr()) {
          continue;
        }
        if (MI.getOpcode() == X86::CMP32ri || MI.getOpcode() == X86::CMP32ri8 ||
            MI.getOpcode() == X86::INC32r) {
          continue;
        }

        LoopBody.push_back(&MI);
      }
    }

    if (LoopBody.empty()) {
      return false;
    }

    const auto &WhereToInsert =
        Exiting->getFirstTerminator(); // первая инструкция-терминатор

    int AmountOfCopies = itersInBlock - 1;

    Register CounterReg;
    for (auto &MI : *Latch) {
      if (MI.getOpcode() == X86::INC32r) {
        CounterReg = MI.getOperand(0).getReg();
        break;
      }
    }

    // копируем
    for (int i = 0; i < AmountOfCopies; i++) {
      // сначала инкремент счётчика
      BuildMI(*Exiting, WhereToInsert, DebugLoc(), TII->get(X86::INC32r),
              CounterReg)
          .addReg(CounterReg);

      // потом копия тела
      for (auto &MI : LoopBody) {
        MachineInstr *Clone = MF.CloneMachineInstr(MI);
        Exiting->insert(WhereToInsert, Clone);
      }
    }

    return true;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

char LoopUnrollPass::ID = 0;

bool LoopUnrollPass::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
  auto &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  bool Changed = false;
  SmallVector<MachineLoop *, 8> Loops;

  for (MachineLoop *Loop : MLI) {
    FillLoops(Loop, Loops);
  }

  for (MachineLoop *Loop : Loops) {

    unsigned TripCount = getTripCount(Loop);
    Changed |= unrollLoop(Loop, TripCount, MF, MLI);
  }

  return Changed;
}

} // namespace

static RegisterPass<LoopUnrollPass> X("loop-unroll-x86", "loop unrolling pass",
                                      false, false);
