#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
struct InsertNullCheck : public MachineFunctionPass {
  static char ID;

  InsertNullCheck() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Insert Null Pointer Check Pass";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Modified = false;
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

    for (MachineBasicBlock &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
        if (!MI->mayLoad() && !MI->mayStore())
          continue;

        const MCInstrDesc &Desc = MI->getDesc();
        int MemOpIdx = X86II::getMemoryOperandNo(Desc.TSFlags);

        if (MemOpIdx < 0)
          continue;

        MemOpIdx += X86II::getOperandBias(Desc);
        const MachineOperand &BaseOp =
            MI->getOperand(MemOpIdx + X86::AddrBaseReg);

        if (!BaseOp.isReg() || !BaseOp.getReg().isValid())
          continue;

        Register BaseReg = BaseOp.getReg();
        if (BaseReg == X86::RSP || BaseReg == X86::RBP)
          continue;

        DebugLoc DL = MI->getDebugLoc();

        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), X86::RDI)
            .addReg(BaseReg);

        BuildMI(MBB, MI, DL, TII->get(X86::CALL64pcrel32))
            .addExternalSymbol("__verify_pointer");

        Modified = true;
      }
    }

    return Modified;
  }
};
} // namespace

char InsertNullCheck::ID = 0;

static RegisterPass<InsertNullCheck>
    X("x86-insert-null-check",
      "Insert check for null pointers before memory access", false, false);