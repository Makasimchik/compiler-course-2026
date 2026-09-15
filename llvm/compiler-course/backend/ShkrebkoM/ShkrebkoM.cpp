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
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override {
    return "Insert NULL Checks Before Memory Dereference";
  }
};

char NullCheckPass::ID = 0;

bool isMemoryAccess(const MachineInstr &MI) {
  return MI.mayLoad() || MI.mayStore();
}

Register getBaseRegister(const MachineInstr &MI) {
  const MCInstrDesc &Desc = MI.getDesc();
  int MemOpOffset = X86II::getMemoryOperandNo(Desc.TSFlags);
  if (MemOpOffset < 0)
    return Register();

  MemOpOffset += X86II::getOperandBias(Desc);
  const MachineOperand &BaseOp = MI.getOperand(MemOpOffset + X86::AddrBaseReg);
  if (!BaseOp.isReg() || !BaseOp.getReg().isValid())
    return Register();

  Register BaseReg = BaseOp.getReg();
  if (BaseReg == X86::RSP || BaseReg == X86::RBP || BaseReg == X86::ESP ||
      BaseReg == X86::EBP)
    return Register();

  const MachineOperand &IndexOp =
      MI.getOperand(MemOpOffset + X86::AddrIndexReg);
  if (IndexOp.isReg() && IndexOp.getReg() != X86::NoRegister)
    return Register();

  return BaseReg;
}

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *TII = ST.getInstrInfo();
  bool Changed = false;

  SmallVector<MachineInstr *, 16> Worklist;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (isMemoryAccess(MI) && getBaseRegister(MI))
        Worklist.push_back(&MI);
    }
  }

  for (MachineInstr *MI : Worklist) {
    MachineBasicBlock *CurMBB = MI->getParent();
    DebugLoc DL = MI->getDebugLoc();
    Register BaseReg = getBaseRegister(*MI);
    if (!BaseReg)
      continue;

    MachineBasicBlock *TrapMBB = MF.CreateMachineBasicBlock();
    MF.push_back(TrapMBB);
    BuildMI(TrapMBB, DL, TII->get(X86::TRAP));

    MachineBasicBlock *ContMBB = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MachineFunction::iterator(CurMBB)), ContMBB);

    ContMBB->splice(ContMBB->begin(), CurMBB, MI->getIterator(), CurMBB->end());
    ContMBB->transferSuccessors(CurMBB);
    while (!CurMBB->succ_empty())
      CurMBB->removeSuccessor(CurMBB->succ_begin());

    CurMBB->addSuccessor(TrapMBB);
    CurMBB->addSuccessor(ContMBB);

    BuildMI(*CurMBB, CurMBB->end(), DL, TII->get(X86::CMP64ri8))
        .addReg(BaseReg)
        .addImm(0);
    BuildMI(*CurMBB, CurMBB->end(), DL, TII->get(X86::JCC_1))
        .addMBB(TrapMBB)
        .addImm(X86::COND_E);
    BuildMI(*CurMBB, CurMBB->end(), DL, TII->get(X86::JMP_1)).addMBB(ContMBB);

    Changed = true;
  }
  return Changed;
}

} // namespace

static RegisterPass<NullCheckPass>
    X("shkrebko-pass", "Insert NULL checks before memory accesses", false,
      false);