#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

namespace {

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF)
      Changed |= processBlock(MBB);

    return Changed;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.setPreservesCFG();
  }

private:
  static constexpr int MaxAllowedIterations = 5;

  bool processBlock(MachineBasicBlock &MBB) const {
    bool Changed = false;

    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() != X86::ADD32ri8)
        continue;

      MachineOperand *ImmOp = findImmOperand(MI);
      if (!ImmOp || !ImmOp->isImm() || ImmOp->getImm() != 1)
        continue;

      Register Reg = getDefinedRegister(MI);
      if (!Reg)
        continue;

      int TripCount = findTripCountInBlock(MBB, MI, Reg);
      if (TripCount < 2 || TripCount > MaxAllowedIterations)
        continue;

      ImmOp->setImm(TripCount);
      Changed = true;
    }

    return Changed;
  }

  MachineOperand *findImmOperand(MachineInstr &MI) const {
    for (MachineOperand &Op : MI.operands()) {
      if (Op.isImm())
        return &Op;
    }
    return nullptr;
  }

  Register getDefinedRegister(MachineInstr &MI) const {
    for (MachineOperand &Op : MI.operands()) {
      if (Op.isReg() && Op.isDef())
        return Op.getReg();
    }
    return Register();
  }

  int findTripCountInBlock(MachineBasicBlock &MBB, MachineInstr &StartMI,
                           Register Reg) const {
    bool SeenStart = false;

    for (MachineInstr &MI : MBB) {
      if (&MI == &StartMI) {
        SeenStart = true;
        continue;
      }

      if (!SeenStart)
        continue;

      unsigned Opc = MI.getOpcode();
      if (Opc != X86::CMP32ri8 && Opc != X86::CMP32ri)
        continue;

      if (!usesRegister(MI, Reg))
        continue;

      for (const MachineOperand &Op : MI.operands()) {
        if (Op.isImm())
          return static_cast<int>(Op.getImm());
      }
    }

    return -1;
  }

  bool usesRegister(MachineInstr &MI, Register Reg) const {
    for (const MachineOperand &Op : MI.operands()) {
      if (Op.isReg() && Op.getReg() == Reg)
        return true;
    }
    return false;
  }
};

char ExamplePass::ID = 0;

} // namespace

static RegisterPass<ExamplePass> X("example-x86", "loop unrolling pass", false,
                                   false);