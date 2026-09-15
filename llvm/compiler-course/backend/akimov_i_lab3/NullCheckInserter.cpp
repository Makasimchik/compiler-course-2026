#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class NullCheckInserter : public MachineFunctionPass {
public:
  static char ID;
  NullCheckInserter() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool isDereference(const MachineInstr &MI, unsigned &PtrReg);
  void insertNullCheck(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                       unsigned PtrReg, const TargetInstrInfo *TII);
  void createAbortCall(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI);
};

char NullCheckInserter::ID = 0;

bool NullCheckInserter::isDereference(const MachineInstr &MI,
                                      unsigned &PtrReg) {
  if (!MI.mayLoad() && !MI.mayStore())
    return false;

  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && MO.isUse() && MO.getReg()) {
      PtrReg = MO.getReg();
      return true;
    }
  }
  return false;
}

void NullCheckInserter::createAbortCall(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI) {
  const TargetInstrInfo *TII = MBB.getParent()->getSubtarget().getInstrInfo();
  BuildMI(MBB, MI, DebugLoc(), TII->get(X86::CALL64pcrel32))
      .addExternalSymbol("abort");
}

void NullCheckInserter::insertNullCheck(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MI,
                                        unsigned PtrReg,
                                        const TargetInstrInfo *TII) {
  MachineFunction &MF = *MBB.getParent();

  MachineBasicBlock *FailBlock =
      MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(++MBB.getIterator(), FailBlock);

  BuildMI(MBB, MI, DebugLoc(), TII->get(X86::TEST64rr))
      .addReg(PtrReg)
      .addReg(PtrReg);

  BuildMI(MBB, MI, DebugLoc(), TII->get(X86::JCC_1))
      .addMBB(FailBlock)
      .addImm(X86::COND_E);

  createAbortCall(*FailBlock, FailBlock->end());
  BuildMI(*FailBlock, FailBlock->end(), DebugLoc(), TII->get(X86::RET));
}

bool NullCheckInserter::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

  for (auto &MBB : MF) {
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      unsigned PtrReg = 0;
      if (isDereference(*MI, PtrReg)) {
        insertNullCheck(MBB, MI, PtrReg, TII);
        Changed = true;
      }
    }
  }
  return Changed;
}

} // namespace

static RegisterPass<NullCheckInserter>
    X("null-check-inserter", "Insert NULL check before pointer dereference",
      false, false);
