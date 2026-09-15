#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  NullCheckPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool isInterestingMemInstr(const MachineInstr &MI) {
    return (MI.mayLoad() || MI.mayStore()) && !MI.isCall() && !MI.isBranch();
  }

  Register getBaseReg(const MachineInstr &MI) {
    const MCInstrDesc &Desc = MI.getDesc();
    int MemOpNo = X86II::getMemoryOperandNo(Desc.TSFlags);
    if (MemOpNo < 0)
      return Register();
    const MachineOperand &BaseOp = MI.getOperand(MemOpNo);
    if (!BaseOp.isReg() || !BaseOp.getReg().isPhysical())
      return Register();
    return BaseOp.getReg();
  }
};

char NullCheckPass::ID = 0;

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *TII = ST.getInstrInfo();
  bool Changed = false;

  SmallVector<MachineInstr *, 16> WorkList;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (isInterestingMemInstr(MI) && getBaseReg(MI))
        WorkList.push_back(&MI);
    }
  }

  for (MachineInstr *MI : WorkList) {
    MachineBasicBlock *CurrMBB = MI->getParent();
    DebugLoc DL = MI->getDebugLoc();
    Register BaseReg = getBaseReg(*MI);
    if (!BaseReg)
      continue;

    MachineBasicBlock *NullMBB = MF.CreateMachineBasicBlock();
    MachineBasicBlock *ContinueMBB = MF.CreateMachineBasicBlock();

    MF.insert(std::next(MachineFunction::iterator(CurrMBB)), ContinueMBB);
    MF.insert(std::next(MachineFunction::iterator(CurrMBB)), NullMBB);

    BuildMI(NullMBB, DL, TII->get(X86::RET64));

    ContinueMBB->splice(ContinueMBB->end(), CurrMBB, MI->getIterator(),
                        CurrMBB->end());

    ContinueMBB->transferSuccessors(CurrMBB);
    CurrMBB->addSuccessor(ContinueMBB);
    CurrMBB->addSuccessor(NullMBB);

    BuildMI(CurrMBB, DL, TII->get(X86::TEST64rr))
        .addReg(BaseReg)
        .addReg(BaseReg);
    BuildMI(CurrMBB, DL, TII->get(X86::JCC_1))
        .addMBB(NullMBB)
        .addImm(X86::COND_E);
    BuildMI(CurrMBB, DL, TII->get(X86::JMP_1)).addMBB(ContinueMBB);

    Changed = true;
  }

  return Changed;
}

} // namespace

static RegisterPass<NullCheckPass>
    X("null-check-x86", "Insert null pointer checks", false, false);