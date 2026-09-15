#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "x86-lab-loop-unroll"

using namespace llvm;

namespace {

class X86LabLoopUnrollPass : public MachineFunctionPass {
public:
  static char ID;

  X86LabLoopUnrollPass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    bool FunctionChanged = false;

    for (MachineLoop *TopLevelLoop : MLI) {
      FunctionChanged |= processLoopNestBottomUp(TopLevelLoop, MF);
    }

    return FunctionChanged;
  }

private:
  bool processLoopNestBottomUp(MachineLoop *Loop, MachineFunction &MF) {
    bool Changed = false;

    for (MachineLoop *InnerLoop : *Loop) {
      Changed |= processLoopNestBottomUp(InnerLoop, MF);
    }

    Changed |= applyUnrolling(Loop, MF);

    return Changed;
  }

  bool applyUnrolling(MachineLoop *Loop, MachineFunction &MF) {
    if (Loop->getNumBlocks() != 1) {
      return false;
    }

    MachineBasicBlock *LoopBlock = Loop->getHeader();
    if (!LoopBlock)
      return false;

    MachineRegisterInfo &MRI = MF.getRegInfo();

    const unsigned MaxUnrollFactor = 5;

    LLVM_DEBUG(dbgs() << "Разворачиваем цикл в функции: " << MF.getName()
                      << "\n");

    SmallVector<MachineInstr *, 16> BodyInstructions;
    for (MachineInstr &MI : *LoopBlock) {
      if (!MI.isTerminator() && !MI.isPHI()) {
        BodyInstructions.push_back(&MI);
      }
    }

    MachineBasicBlock::iterator InsertPoint = LoopBlock->getFirstTerminator();

    bool Unrolled = false;

    for (unsigned Iteration = 1; Iteration < MaxUnrollFactor; ++Iteration) {
      DenseMap<Register, Register> VRegMap;

      for (MachineInstr *MI : BodyInstructions) {
        for (const MachineOperand &MO : MI->operands()) {
          if (MO.isReg() && MO.isDef() && MO.getReg().isVirtual()) {
            Register OldReg = MO.getReg();
            Register NewReg = MRI.cloneVirtualRegister(OldReg);
            VRegMap[OldReg] = NewReg;
          }
        }
      }

      for (MachineInstr *MI : BodyInstructions) {
        MachineInstr *ClonedInstr = MF.CloneMachineInstr(MI);

        for (MachineOperand &MO : ClonedInstr->operands()) {
          if (MO.isReg() && MO.getReg().isVirtual()) {
            if (VRegMap.count(MO.getReg())) {
              MO.setReg(VRegMap[MO.getReg()]);
            }
          }
        }

        LoopBlock->insert(InsertPoint, ClonedInstr);
        Unrolled = true;
      }
    }

    return Unrolled;
  }
};

char X86LabLoopUnrollPass::ID = 0;

} // namespace

static RegisterPass<X86LabLoopUnrollPass>
    X("x86-lab-unroll", "X86 Loop Unrolling Lab Pass (Max 5 iters)", false,
      false);