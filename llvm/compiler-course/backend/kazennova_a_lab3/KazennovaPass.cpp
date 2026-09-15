#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  NullCheckPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

    for (MachineBasicBlock &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {

        if (!MI->mayLoad() && !MI->mayStore())
          continue;

        const MCInstrDesc &Desc = MI->getDesc();
        int MemOpNo = X86II::getMemoryOperandNo(Desc.TSFlags);
        if (MemOpNo < 0)
          continue;
        MemOpNo += X86II::getOperandBias(Desc);

        const MachineOperand &BaseOp =
            MI->getOperand(MemOpNo + X86::AddrBaseReg);
        if (!BaseOp.isReg())
          continue;
        Register BaseReg = BaseOp.getReg();
        if (!BaseReg.isValid())
          continue;

        if (BaseReg == X86::RSP || BaseReg == X86::RBP || BaseReg == X86::RIP)
          continue;

        bool AlreadyHasCheck = false;
        if (MI != MBB.begin()) {
          auto PrevMI = std::prev(MI);
          if (PrevMI->getOpcode() == X86::CALL64pcrel32) {
            for (const MachineOperand &MO : PrevMI->operands()) {
              if (MO.isSymbol() &&
                  StringRef(MO.getSymbolName()) == "verify_pointer") {
                AlreadyHasCheck = true;
                break;
              }
            }
          }
        }

        if (AlreadyHasCheck)
          continue;

        DebugLoc DL = MI->getDebugLoc();
        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), X86::RDI)
            .addReg(BaseReg);
        BuildMI(MBB, MI, DL, TII->get(X86::CALL64pcrel32))
            .addExternalSymbol("verify_pointer");

        Changed = true;
      }
    }
    return Changed;
  }
};

char NullCheckPass::ID = 0;

static RegisterPass<NullCheckPass>
    X("null-check", "Insert NULL check before pointer dereference", false,
      false);

} // namespace