#include "MCTargetDesc/X86BaseInfo.h"
#include "MCTargetDesc/X86MCTargetDesc.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/IR/DebugLoc.h"
#include <cstring>

using namespace llvm;

namespace {
class GutyanskyANullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  GutyanskyANullCheckPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &Func) override;

private:
  static void addCheck(MachineFunction &MF, MachineBasicBlock &BasicBlock,
                       MachineInstr &Insn, Register Reg);
  static bool shouldCheckRegister(Register Reg);
  static bool isCheckNullPair(const MachineInstr &CopyInsn,
                              const MachineInstr &CallInsn, Register Reg);
  static bool hasPrecedingCheckNull(MachineBasicBlock &BasicBlock,
                                    MachineInstr &Insn, Register Reg);
};

char GutyanskyANullCheckPass::ID = 0;

bool GutyanskyANullCheckPass::shouldCheckRegister(Register Reg) {
  switch (Reg) {
  case X86::ESP:
  case X86::EBP:
  case X86::RSP:
  case X86::RBP:
    return false;
  default:
    return true;
  }
}

bool GutyanskyANullCheckPass::isCheckNullPair(const MachineInstr &CopyInsn,
                                              const MachineInstr &CallInsn,
                                              Register Reg) {
  if (CopyInsn.getOpcode() != TargetOpcode::COPY)
    return false;

  if (!CopyInsn.getOperand(0).isReg() ||
      CopyInsn.getOperand(0).getReg() != X86::RDI)
    return false;

  if (!CopyInsn.getOperand(1).isReg())
    return false;

  Register RegToCheck = CopyInsn.getOperand(1).getReg();
  if (RegToCheck != Reg)
    return false;

  if (CallInsn.getOpcode() != X86::CALL64pcrel32)
    return false;

  const auto &SymbolName = CallInsn.getOperand(0);
  return SymbolName.isSymbol() &&
         std::strcmp(SymbolName.getSymbolName(), "check_null") == 0;
}

bool GutyanskyANullCheckPass::hasPrecedingCheckNull(
    MachineBasicBlock &BasicBlock, MachineInstr &Insn, Register Reg) {

  auto It = Insn.getIterator();

  if (It == BasicBlock.begin())
    return false;

  --It;
  const auto &CallInsn = *It;

  if (It == BasicBlock.begin())
    return false;

  --It;
  const auto &CopyInsn = *It;

  return isCheckNullPair(CopyInsn, CallInsn, Reg);
}

void GutyanskyANullCheckPass::addCheck(MachineFunction &MF,
                                       MachineBasicBlock &BasicBlock,
                                       MachineInstr &Insn, Register Reg) {
  const X86InstrInfo *InstrInfo =
      MF.getSubtarget<X86Subtarget>().getInstrInfo();
  DebugLoc DLoc = Insn.getDebugLoc();

  BuildMI(BasicBlock, Insn, DLoc, InstrInfo->get(TargetOpcode::COPY), X86::RDI)
      .addReg(Reg);
  BuildMI(BasicBlock, Insn, DLoc, InstrInfo->get(X86::CALL64pcrel32))
      .addExternalSymbol("check_null");
}

bool GutyanskyANullCheckPass::runOnMachineFunction(MachineFunction &Func) {
  bool HasChanges = false;

  for (auto &BasicBlock : Func) {
    for (auto &Insn : BasicBlock) {

      if (!Insn.mayLoadOrStore())
        continue;

      const auto &Desc = Insn.getDesc();

      int OpNo = X86II::getMemoryOperandNo(Desc.TSFlags);
      if (OpNo == -1)
        continue;

      OpNo += X86II::getOperandBias(Desc);

      const auto &Operand = Insn.getOperand(OpNo + X86::AddrBaseReg);
      if (!Operand.isReg())
        continue;

      if (!shouldCheckRegister(Operand.getReg()))
        continue;

      if (hasPrecedingCheckNull(BasicBlock, Insn, Operand.getReg()))
        continue;

      addCheck(Func, BasicBlock, Insn, Operand.getReg());

      HasChanges = true;
    }
  }

  return HasChanges;
}
} // namespace

static RegisterPass<GutyanskyANullCheckPass>
    X("gutyansky-a-null-check-x86", "Insert null pointer checks", false, false);
