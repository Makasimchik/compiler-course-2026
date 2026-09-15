#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

static unsigned MAX_INSTS = 15;
static unsigned MAX_DEPTH = 3;

namespace {

class InliningPass : public MachineFunctionPass {
public:
  static char ID;
  InliningPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (MI.isCall()) {
          if (canInline(MI)) {
            MI.eraseFromParent();
            Changed = true;
            break;
          }
        }
      }
    }
    return Changed;
  }

private:
  bool canInline(MachineInstr &Call) {
    for (unsigned i = 0; i < Call.getNumOperands(); i++) {
      MachineOperand &Op = Call.getOperand(i);
      if (Op.isGlobal()) {
        if (Function *F =
                const_cast<Function *>(dyn_cast<Function>(Op.getGlobal()))) {
          if (F && !F->isDeclaration()) {
            unsigned Size = countInstructions(F);
            if (Size <= MAX_INSTS) {
              if (isRecursive(F)) {
                return false;
              }
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  unsigned countInstructions(Function *F) {
    unsigned Count = 0;
    for (BasicBlock &BB : *F) {
      Count += BB.size();
    }
    return Count;
  }

  bool isRecursive(Function *F) {
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (auto *Call = dyn_cast<CallBase>(&I)) {
          if (Function *Callee = Call->getCalledFunction()) {
            if (Callee == F) {
              return true;
            }
          }
        }
      }
    }
    return false;
  }
};

char InliningPass::ID = 0;

} // namespace

RegisterPass<InliningPass> X("inlining", "Function Inlining Pass");
