#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
class SpichekLab3Pass : public MachineFunctionPass {
public:
  static char ID;
  SpichekLab3Pass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "Spichek Lab 3: Replace INC/DEC sequences with ADD/SUB";
  }
};

char SpichekLab3Pass::ID = 0;

bool isIncDec(unsigned Opcode) {
  return Opcode == X86::INC32r || Opcode == X86::INC64r ||
         Opcode == X86::DEC32r || Opcode == X86::DEC64r;
}

bool isInc(unsigned Opcode) {
  return Opcode == X86::INC32r || Opcode == X86::INC64r;
}

bool is32Bit(unsigned Opcode) {
  return Opcode == X86::INC32r || Opcode == X86::DEC32r;
}

bool SpichekLab3Pass::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

  for (auto &MBB : MF) {
    auto I = MBB.begin();
    while (I != MBB.end()) {
      if (!isIncDec(I->getOpcode())) {
        ++I;
        continue;
      }

      Register TargetReg = I->getOperand(0).getReg();
      bool Is32 = is32Bit(I->getOpcode());
      int Delta = 0;
      auto StartI = I;

      while (I != MBB.end() && isIncDec(I->getOpcode()) &&
             I->getOperand(0).getReg() == TargetReg &&
             is32Bit(I->getOpcode()) == Is32) {

        Delta += isInc(I->getOpcode()) ? 1 : -1;
        ++I;
      }

      DebugLoc DL = StartI->getDebugLoc();

      I = MBB.erase(StartI, I);

      if (Delta > 0) {
        unsigned AddOpc = Is32 ? X86::ADD32ri : X86::ADD64ri32;
        BuildMI(MBB, I, DL, TII->get(AddOpc), TargetReg)
            .addReg(TargetReg)
            .addImm(Delta);
      } else if (Delta < 0) {
        unsigned SubOpc = Is32 ? X86::SUB32ri : X86::SUB64ri32;
        BuildMI(MBB, I, DL, TII->get(SubOpc), TargetReg)
            .addReg(TargetReg)
            .addImm(-Delta);
      } else {
        unsigned AddOpc = Is32 ? X86::ADD32ri : X86::ADD64ri32;
        BuildMI(MBB, I, DL, TII->get(AddOpc), TargetReg)
            .addReg(TargetReg)
            .addImm(0);
      }

      Changed = true;
    }
  }
  return Changed;
}
} // namespace

static RegisterPass<SpichekLab3Pass>
    X("spichek-lab-3", "Replace INC/DEC with ADD/SUB pass", false, false);