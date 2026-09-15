#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include <optional>

using namespace llvm;

namespace {

bool isStackOrFrameRegister(Register Reg) {
  switch (Reg.id()) {
  case X86::RSP:
  case X86::ESP:
  case X86::RBP:
  case X86::EBP:
    return true;
  default:
    return false;
  }
}

std::optional<unsigned> getMemoryOperandStart(const MachineInstr &MI) {
  const MCInstrDesc &Desc = MI.getDesc();
  int MemOpStart = X86II::getMemoryOperandNo(Desc.TSFlags);
  if (MemOpStart < 0)
    return std::nullopt;

  return static_cast<unsigned>(MemOpStart + X86II::getOperandBias(Desc));
}

std::optional<Register> getCheckedPointerRegister(const MachineInstr &MI) {
  if (MI.isCall() || MI.isBranch() || MI.isInlineAsm())
    return std::nullopt;

  if (!MI.mayLoad() && !MI.mayStore())
    return std::nullopt;

  std::optional<unsigned> MemOpStart = getMemoryOperandStart(MI);
  if (!MemOpStart.has_value())
    return std::nullopt;

  const MachineOperand &BaseOp = MI.getOperand(*MemOpStart + X86::AddrBaseReg);
  if (!BaseOp.isReg())
    return std::nullopt;

  Register BaseReg = BaseOp.getReg();
  if (!BaseReg.isValid() || isStackOrFrameRegister(BaseReg))
    return std::nullopt;

  return BaseReg;
}

void dropRedefinedRegisters(const MachineInstr &MI,
                            const TargetRegisterInfo *TRI,
                            SmallSet<Register, 8> &CheckedRegs) {
  SmallVector<Register, 4> InvalidatedRegs;
  for (Register Reg : CheckedRegs) {
    if (MI.modifiesRegister(Reg, TRI))
      InvalidatedRegs.push_back(Reg);
  }

  for (Register Reg : InvalidatedRegs)
    CheckedRegs.erase(Reg);
}

class InsertNullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  InsertNullCheckPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
};

char InsertNullCheckPass::ID = 0;

bool InsertNullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &Subtarget = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *InstrInfo = Subtarget.getInstrInfo();
  const TargetRegisterInfo *RegisterInfo = Subtarget.getRegisterInfo();

  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    SmallSet<Register, 8> CheckedRegs;

    for (MachineBasicBlock::iterator MI = MBB.begin(); MI != MBB.end(); ++MI) {
      dropRedefinedRegisters(*MI, RegisterInfo, CheckedRegs);

      if (MI->isCall() || MI->isInlineAsm())
        CheckedRegs.clear();

      std::optional<Register> BaseReg = getCheckedPointerRegister(*MI);
      if (!BaseReg.has_value())
        continue;

      if (CheckedRegs.count(*BaseReg))
        continue;

      DebugLoc DL = MI->getDebugLoc();
      BuildMI(MBB, MI, DL, InstrInfo->get(TargetOpcode::COPY), X86::RDI)
          .addReg(*BaseReg);
      BuildMI(MBB, MI, DL, InstrInfo->get(X86::CALL64pcrel32))
          .addExternalSymbol("check_null");

      CheckedRegs.insert(*BaseReg);
      Changed = true;
    }
  }

  return Changed;
}

} // namespace

static RegisterPass<InsertNullCheckPass>
    X("insert-null-check-x86", "Insert null checks before memory dereference",
      false, false);
