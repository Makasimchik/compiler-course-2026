#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
struct NullCheckBackendPass : public MachineFunctionPass {
  static char ID;
  NullCheckBackendPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const X86Subtarget &STI = MF.getSubtarget<X86Subtarget>();
    const X86InstrInfo *TII = STI.getInstrInfo();

    for (auto MBB = MF.begin(); MBB != MF.end(); ++MBB) {
      for (auto MI = MBB->begin(); MI != MBB->end(); ++MI) {
        if (MI->mayLoad() || MI->mayStore()) {
          Register BaseReg;
          for (unsigned i = 0; i < MI->getNumOperands(); ++i) {
            MachineOperand &MO = MI->getOperand(i);
            if (MO.isReg() && MO.isUse() && MO.getReg().isVirtual()) {
              BaseReg = MO.getReg();
              break;
            }
          }

          if (BaseReg) {
            MachineBasicBlock *TrapMBB = MF.CreateMachineBasicBlock();
            MF.push_back(TrapMBB);
            BuildMI(*TrapMBB, TrapMBB->end(), MI->getDebugLoc(),
                    TII->get(X86::TRAP));

            MachineBasicBlock *FallThroughMBB = MF.CreateMachineBasicBlock();
            MF.insert(std::next(MBB), FallThroughMBB);
            FallThroughMBB->splice(FallThroughMBB->begin(), &*MBB, MI,
                                   MBB->end());
            FallThroughMBB->transferSuccessors(&*MBB);

            MBB->addSuccessor(TrapMBB);
            MBB->addSuccessor(FallThroughMBB);

            BuildMI(*MBB, MBB->end(), MI->getDebugLoc(),
                    TII->get(X86::TEST64rr))
                .addReg(BaseReg)
                .addReg(BaseReg);
            BuildMI(*MBB, MBB->end(), MI->getDebugLoc(), TII->get(X86::JCC_1))
                .addMBB(TrapMBB)
                .addImm(X86::COND_E);

            Changed = true;
            MBB = FallThroughMBB->getIterator();
            break;
          }
        }
      }
    }
    return Changed;
  }
};
} // namespace

char NullCheckBackendPass::ID = 0;
static RegisterPass<NullCheckBackendPass>
    X("null-check-backend", "Insert NULL checks before dereference");