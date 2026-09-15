#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "loop-unroll-x86"

namespace {

class LoopUnrollPass : public MachineFunctionPass {
public:
  static char ID;
  LoopUnrollPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  int getEstimatedTripCount(MachineBasicBlock &MBB, MachineInstr *BackEdge);
};

char LoopUnrollPass::ID = 0;

int LoopUnrollPass::getEstimatedTripCount(MachineBasicBlock &MBB,
                                          MachineInstr *BackEdge) {
  int TripCount = -1;

  for (MachineInstr &MI : MBB) {
    if (&MI == BackEdge)
      continue;

    if (MI.getOpcode() == X86::CMP32ri || MI.getOpcode() == X86::CMP32ri8 ||
        MI.getOpcode() == X86::CMP64ri32 || MI.getOpcode() == X86::CMP64ri8) {
      for (MachineOperand &Op : MI.operands()) {
        if (Op.isImm() && Op.getImm() >= 0) {
          TripCount = Op.getImm() + 1; // +1 because index from 0
          break;
        }
      }
    }
  }

  return TripCount;
}

bool LoopUnrollPass::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const int MaxUnrollFactor = 5;

  for (MachineBasicBlock &MBB : MF) {
    MachineInstr *BackEdge = nullptr;

    for (MachineInstr &MI : MBB) {
      if (!MI.isBranch())
        continue;

      for (MachineOperand &Op : MI.operands()) {
        if (Op.isMBB() && Op.getMBB() == &MBB) {
          BackEdge = &MI;
          break;
        }
      }

      if (BackEdge)
        break;
    }

    if (!BackEdge)
      continue;

    SmallVector<MachineInstr *, 16> Body;
    SmallVector<MachineInstr *, 4> CmpInstructions;

    for (MachineInstr &MI : MBB) {
      if (&MI == BackEdge)
        continue;

      if (MI.getOpcode() == X86::CMP32ri || MI.getOpcode() == X86::CMP32ri8 ||
          MI.getOpcode() == X86::CMP64ri32 || MI.getOpcode() == X86::CMP64ri8) {
        CmpInstructions.push_back(&MI);
        continue;
      }

      Body.push_back(&MI);
    }

    if (Body.empty() && CmpInstructions.empty()) {
      BackEdge->eraseFromParent();
      Changed = true;
      continue;
    }

    int EstimatedTrip = getEstimatedTripCount(MBB, BackEdge);
    int UnrollFactor;
    bool RemoveBackEdge;

    if (EstimatedTrip > 0 && EstimatedTrip <= MaxUnrollFactor) {
      UnrollFactor = EstimatedTrip;
      RemoveBackEdge = true;
    } else {
      UnrollFactor = MaxUnrollFactor;
      RemoveBackEdge = false;
    }

    if (UnrollFactor <= 1) {
      if (RemoveBackEdge) {
        BackEdge->eraseFromParent();
        Changed = true;
      }
      continue;
    }

    auto InsertPt = BackEdge->getIterator();
    MachineFunction &Func = *MBB.getParent();

    for (int i = 1; i < UnrollFactor; ++i) {
      for (MachineInstr *MI : Body) {
        MachineInstr *Clone = Func.CloneMachineInstr(MI);

        for (MachineOperand &Op : Clone->operands()) {
          if (Op.isReg() && Op.isUse() && Op.isKill())
            Op.setIsKill(false);
        }

        MBB.insert(InsertPt, Clone);
      }
    }

    if (RemoveBackEdge) {
      BackEdge->eraseFromParent();
    }

    Changed = true;
  }

  return Changed;
}

} // namespace

static RegisterPass<LoopUnrollPass> X("loop-unroll-x86", "Loop Unroll Pass",
                                      false, false);