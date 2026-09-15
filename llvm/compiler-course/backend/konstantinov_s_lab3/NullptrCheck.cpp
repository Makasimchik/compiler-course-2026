#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
class InsertNullCheckPass : public llvm::MachineFunctionPass {
public:
  static char ID;
  InsertNullCheckPass() : llvm::MachineFunctionPass(ID) {}

  bool runOnMachineFunction(llvm::MachineFunction &MF) override;

private:
  llvm::Register extractBaseRegister(llvm::MachineInstr &MI) const;
  bool shouldCheckRegister(llvm::Register Reg) const;
  void insertCheck(llvm::MachineBasicBlock &MBB,
                   llvm::MachineBasicBlock::iterator InsertPt,
                   llvm::DebugLoc DL, const llvm::TargetInstrInfo *TII,
                   llvm::Register Reg) const;
};

char InsertNullCheckPass::ID = 0;

llvm::Register
InsertNullCheckPass::extractBaseRegister(llvm::MachineInstr &MI) const {
  const llvm::MCInstrDesc &Desc = MI.getDesc();

  int MemOp = X86II::getMemoryOperandNo(Desc.TSFlags);
  if (MemOp == -1)
    return llvm::Register();

  MemOp += X86II::getOperandBias(Desc);

  const llvm::MachineOperand &Base = MI.getOperand(MemOp + X86::AddrBaseReg);

  if (!Base.isReg())
    return llvm::Register();

  return Base.getReg();
}

bool InsertNullCheckPass::shouldCheckRegister(llvm::Register Reg) const {
  if (!Reg.isValid())
    return false;

  return Reg != X86::RSP && Reg != X86::RBP;
}

void InsertNullCheckPass::insertCheck(
    llvm::MachineBasicBlock &MBB, llvm::MachineBasicBlock::iterator InsertPt,
    llvm::DebugLoc DL, const llvm::TargetInstrInfo *TII,
    llvm::Register Reg) const {

  llvm::BuildMI(MBB, InsertPt, DL, TII->get(llvm::TargetOpcode::COPY), X86::RDI)
      .addReg(Reg);

  llvm::BuildMI(MBB, InsertPt, DL, TII->get(X86::CALL64pcrel32))
      .addExternalSymbol("check_null");
}

bool InsertNullCheckPass::runOnMachineFunction(llvm::MachineFunction &MF) {
  bool Modified = false;

  const llvm::TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

  for (llvm::MachineBasicBlock &MBB : MF) {
    for (auto It = MBB.begin(); It != MBB.end(); ++It) {
      llvm::MachineInstr &MI = *It;

      if (!MI.mayLoad() && !MI.mayStore())
        continue;

      llvm::Register BaseReg = extractBaseRegister(MI);

      if (!shouldCheckRegister(BaseReg))
        continue;

      insertCheck(MBB, It, MI.getDebugLoc(), TII, BaseReg);

      Modified = true;
    }
  }

  return Modified;
}
} // namespace

static RegisterPass<InsertNullCheckPass> X("nullptrcheckpass",
                                           "description pass", false, false);
