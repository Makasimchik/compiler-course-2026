#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"

using namespace llvm;

namespace {
class LoopUnrollPass : public MachineFunctionPass {
public:
  static char ID;
  LoopUnrollPass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    auto &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    bool Changed = false;
    for (MachineLoop *L : MLI)
      Changed |= unrollRec(L);
    return Changed;
  }

private:
  bool unrollRec(MachineLoop *L) {
    bool Changed = false;
    for (MachineLoop *Sub : *L)
      Changed |= unrollRec(Sub);
    return Changed | tryUnroll(L);
  }

  bool tryUnroll(MachineLoop *L) {
    auto *Header = L->getHeader();
    auto *Latch = L->getLoopLatch();
    auto *Pre = L->getLoopPreheader();
    if (!Header || !Latch || !Pre || Header != Latch)
      return false;

    MachineInstr *Branch = nullptr;
    MachineInstr *Step = nullptr;
    for (auto I = Header->rbegin(), E = Header->rend(); I != E; ++I) {
      if (I->isDebugInstr())
        continue;
      if (!Branch) {
        if (I->getOpcode() != X86::JCC_1 || !I->getOperand(0).isMBB() ||
            I->getOperand(0).getMBB() != Header)
          return false;
        Branch = &*I;
      } else {
        Step = &*I;
        break;
      }
    }
    if (!Branch || !Step || Step->getOpcode() != X86::SUB32rr ||
        !Step->getOperand(0).isReg() || !Step->getOperand(1).isReg())
      return false;

    Register CntReg = Step->getOperand(0).getReg();
    int64_t Trips = -1;
    for (auto I = Pre->rbegin(), E = Pre->rend(); I != E; ++I) {
      if (I->getOpcode() == X86::MOV32ri && I->getOperand(0).isReg() &&
          I->getOperand(0).getReg() == CntReg && I->getOperand(1).isImm()) {
        Trips = I->getOperand(1).getImm();
        break;
      }
    }
    if (Trips < 1 || Trips > 5)
      return false;

    SmallVector<MachineInstr *, 8> Body;
    for (MachineInstr &MI : *Header) {
      if (&MI == Step)
        break;
      if (!MI.isDebugInstr() && !MI.isTerminator())
        Body.push_back(&MI);
    }

    auto InsertPt = Step->getIterator();
    for (int64_t I = 1; I < Trips; ++I)
      for (MachineInstr *MI : Body)
        Header->insert(InsertPt, Header->getParent()->CloneMachineInstr(MI));

    Step->eraseFromParent();
    Branch->eraseFromParent();
    if (Header->isSuccessor(Header))
      Header->removeSuccessor(Header);
    return true;
  }
};

char LoopUnrollPass::ID = 0;
} // namespace

static RegisterPass<LoopUnrollPass> X("kondakov-loop-unroll-x86",
                                      "Loop unrolling", false, false);
