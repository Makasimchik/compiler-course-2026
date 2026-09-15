#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"

using namespace llvm;

namespace {
class NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  NullCheckPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool isInterestingLoad(const MachineInstr &MI) {
    return MI.mayLoad() && !MI.isCall() && !MI.isBranch();
  }

  Register getBaseReg(const MachineInstr &MI) {
    const MCInstrDesc &desc = MI.getDesc();
    int memOpStart = X86II::getMemoryOperandNo(desc.TSFlags);
    if (memOpStart < 0)
      return Register();

    const MachineOperand &baseOp = MI.getOperand(memOpStart);
    if (!baseOp.isReg())
      return Register();

    return baseOp.getReg();
  }

  MachineBasicBlock *getOrCreateTrapBlock(MachineFunction &MF,
                                          const X86InstrInfo *TII) {
    if (TrapBlock)
      return TrapBlock;

    TrapBlock = MF.CreateMachineBasicBlock();
    MF.push_back(TrapBlock);
    BuildMI(TrapBlock, DebugLoc(), TII->get(X86::TRAP));

    return TrapBlock;
  }

private:
  MachineBasicBlock *TrapBlock = nullptr;
};

char NullCheckPass::ID = 0;

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &subtarget = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *TII = subtarget.getInstrInfo();
  bool changed = false;

  SmallVector<MachineInstr *, 16> workList;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (isInterestingLoad(MI) && getBaseReg(MI))
        workList.push_back(&MI);
    }
  }

  for (MachineInstr *MI : workList) {
    MachineBasicBlock *currMBB = MI->getParent();
    DebugLoc DL = MI->getDebugLoc();

    Register baseReg = getBaseReg(*MI);
    if (!baseReg)
      continue;
    MachineBasicBlock *trapBlock = getOrCreateTrapBlock(MF, TII);
    MachineBasicBlock *continueBlock = MF.CreateMachineBasicBlock();
    MF.insert(std::next(MachineFunction::iterator(currMBB)), continueBlock);

    continueBlock->splice(continueBlock->begin(), currMBB, MI->getIterator(),
                          currMBB->end());

    continueBlock->transferSuccessors(currMBB);
    while (!currMBB->succ_empty())
      currMBB->removeSuccessor(currMBB->succ_begin());
    currMBB->addSuccessor(continueBlock);
    currMBB->addSuccessor(trapBlock);

    BuildMI(*currMBB, currMBB->end(), DL, TII->get(X86::TEST64rr))
        .addReg(baseReg)
        .addReg(baseReg);
    BuildMI(*currMBB, currMBB->end(), DL, TII->get(X86::JCC_1))
        .addMBB(trapBlock)
        .addImm(X86::COND_E);
    BuildMI(*currMBB, currMBB->end(), DL, TII->get(X86::JMP_1))
        .addMBB(continueBlock);
    changed = true;
  }

  return changed;
}
} // namespace

static RegisterPass<NullCheckPass> X("null-check", "Null pointer check pass",
                                     false, false);
