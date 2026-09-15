#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;

namespace {
class SavvaDariyaInsertCheckPass : public MachineFunctionPass {
public:
  static char ID;
  SavvaDariyaInsertCheckPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  bool isSystemReg(Register R) const {
    return R == X86::RSP || R == X86::RBP || R == X86::ESP || R == X86::EBP;
  }

  Register getBaseAddrReg(const MachineInstr &MI) const {
    const MCInstrDesc &Desc = MI.getDesc();
    int MemOp = X86II::getMemoryOperandNo(Desc.TSFlags);
    if (MemOp < 0)
      return Register();
    MemOp += X86II::getOperandBias(Desc);
    const MachineOperand &Base = MI.getOperand(MemOp + X86::AddrBaseReg);
    return (Base.isReg() && Base.getReg().isValid()) ? Base.getReg()
                                                     : Register();
  }
};
} // namespace

char SavvaDariyaInsertCheckPass::ID = 0;

bool SavvaDariyaInsertCheckPass::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();

  MachineBasicBlock *CommonTrapBB = MF.CreateMachineBasicBlock();
  MF.push_back(CommonTrapBB);
  BuildMI(CommonTrapBB, DebugLoc(), TII->get(X86::TRAP));

  SmallVector<MachineInstr *, 16> Targets;
  for (auto &MBB : MF) {
    if (&MBB == CommonTrapBB)
      continue;
    for (auto &MI : MBB) {
      if (MI.mayLoad() || MI.mayStore()) {
        Register R = getBaseAddrReg(MI);
        if (R && !isSystemReg(R))
          Targets.push_back(&MI);
      }
    }
  }

  for (MachineInstr *MI : Targets) {
    MachineBasicBlock *OrigBB = MI->getParent();
    DebugLoc DL = MI->getDebugLoc();
    Register Ptr = getBaseAddrReg(*MI);

    MachineBasicBlock *ContBB = MF.CreateMachineBasicBlock();
    MF.insert(std::next(OrigBB->getIterator()), ContBB);

    ContBB->splice(ContBB->end(), OrigBB, MI->getIterator(), OrigBB->end());

    ContBB->transferSuccessorsAndUpdatePHIs(OrigBB);

    BuildMI(OrigBB, DL, TII->get(X86::TEST64rr)).addReg(Ptr).addReg(Ptr);

    BuildMI(OrigBB, DL, TII->get(X86::JCC_1))
        .addMBB(CommonTrapBB)
        .addImm(X86::COND_E);

    OrigBB->addSuccessor(CommonTrapBB);
    OrigBB->addSuccessor(ContBB);

    Changed = true;
  }

  return Changed;
}

static RegisterPass<SavvaDariyaInsertCheckPass>
    X("savvadariya-insert-check",
      "Savva Dariya: inserting a null pointer check", false, false);