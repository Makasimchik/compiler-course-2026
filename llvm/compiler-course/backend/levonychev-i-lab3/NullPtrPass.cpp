#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

namespace {
class CheckNullPtrPass : public MachineFunctionPass {
public:
  static char ID;
  CheckNullPtrPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

    for (auto &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
        if (MI->mayLoad() || MI->mayStore()) {
          int MemOpNo = X86II::getMemoryOperandNo(MI->getDesc().TSFlags);
          if (MemOpNo == -1)
            continue;

          const MachineOperand &BaseRegOp =
              MI->getOperand(MemOpNo + X86::AddrBaseReg);

          if (!BaseRegOp.isReg() || !BaseRegOp.getReg().isValid())
            continue;

          Register PtrReg = BaseRegOp.getReg();

          if (PtrReg == X86::RSP || PtrReg == X86::RBP || PtrReg == X86::RIP)
            continue;

          if (MI != MBB.begin()) {
            auto PrevMI = std::prev(MI);
            if (PrevMI->getOpcode() == X86::CALL64pcrel32 &&
                PrevMI->getNumOperands() > 0 &&
                PrevMI->getOperand(0).isSymbol() &&
                StringRef(PrevMI->getOperand(0).getSymbolName()) ==
                    "check_null") {
              continue;
            }
          }

          DebugLoc DL = MI->getDebugLoc();
          BuildMI(MBB, MI, DL, TII->get(X86::MOV64rr), X86::RDI).addReg(PtrReg);
          auto MIB = BuildMI(MBB, MI, DL, TII->get(X86::CALL64pcrel32))
                         .addExternalSymbol("check_null");

          MIB.addReg(X86::RDI, RegState::Implicit);

          Changed = true;
        }
      }
    }
    return Changed;
  }
};

char CheckNullPtrPass::ID = 0;
} // namespace

static RegisterPass<CheckNullPtrPass> X("levonychev-nullptr-x86",
                                        "Call check_null", false, false);