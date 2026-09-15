#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCInstrDesc.h"

using namespace llvm;

namespace {

class NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  NullCheckPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  Register getDerefBaseReg(const MachineInstr &MI, const X86Subtarget &STI);
};

char NullCheckPass::ID = 0;

Register NullCheckPass::getDerefBaseReg(const MachineInstr &MI,
                                        const X86Subtarget &STI) {
  const MCInstrDesc &Desc = MI.getDesc();

  if (!Desc.mayLoad() && !Desc.mayStore())
    return Register();

  int MemOpNo = X86II::getMemoryOperandNo(Desc.TSFlags);
  if (MemOpNo < 0)
    return Register();

  unsigned MemOpOffset = (unsigned)MemOpNo + X86II::getOperandBias(Desc);
  if (MemOpOffset >= MI.getNumOperands())
    return Register();

  const MachineOperand &BaseOp = MI.getOperand(MemOpOffset + X86::AddrBaseReg);
  if (!BaseOp.isReg())
    return Register();

  Register BaseReg = BaseOp.getReg();
  if (!BaseReg.isValid())
    return Register();

  const TargetRegisterInfo &TRI = *STI.getRegisterInfo();
  if (TRI.isSubRegisterEq(X86::RSP, BaseReg) ||
      TRI.isSubRegisterEq(X86::RBP, BaseReg) ||
      TRI.isSubRegisterEq(X86::RIP, BaseReg))
    return Register();

  return BaseReg;
}

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &STI = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *TII = STI.getInstrInfo();
  const TargetRegisterInfo &TRI = *STI.getRegisterInfo();
  bool Changed = false;

  SmallVector<std::pair<MachineInstr *, Register>, 16> ToInsert;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (Register Base = getDerefBaseReg(MI, STI))
        ToInsert.push_back({&MI, Base});

  if (ToInsert.empty())
    return false;

  MachineBasicBlock *TrapBB = MF.CreateMachineBasicBlock();
  MF.push_back(TrapBB);
  BuildMI(*TrapBB, TrapBB->end(), DebugLoc(), TII->get(X86::TRAP));

  for (auto &[MI, BaseReg] : ToInsert) {
    MachineBasicBlock *OrigBB = MI->getParent();
    DebugLoc DL = MI->getDebugLoc();

    MachineBasicBlock *ContBB = OrigBB->splitAt(*MI, /*UpdateLiveIns=*/true);
    unsigned RegSize =
        TRI.getRegSizeInBits(*TRI.getMinimalPhysRegClass(BaseReg));
    unsigned TestOp = (RegSize == 32) ? X86::TEST32rr : X86::TEST64rr;

    BuildMI(*OrigBB, OrigBB->end(), DL, TII->get(TestOp))
        .addReg(BaseReg)
        .addReg(BaseReg);

    BuildMI(*OrigBB, OrigBB->end(), DL, TII->get(X86::JCC_1))
        .addMBB(TrapBB)
        .addImm(X86::COND_E);

    OrigBB->addSuccessor(TrapBB);
    OrigBB->addSuccessor(ContBB);
    Changed = true;
  }

  return Changed;
}

} // namespace

static RegisterPass<NullCheckPass>
    X("null-check-x86", "Insert NULL pointer checks before dereferences", false,
      false);