#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class IncDecFoldPass : public MachineFunctionPass {
public:
  static char ID;
  IncDecFoldPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end();) {
        MachineInstr &MI = *I;

        int Delta = getDelta(MI);
        if (Delta == 0) {
          ++I;
          continue;
        }

        Register Reg = MI.getOperand(0).getReg();
        unsigned OrigOpc = MI.getOpcode();

        auto Start = I;
        auto J = std::next(I);

        while (J != MBB.end()) {
          int D = getDelta(*J);
          if (D == 0)
            break;

          if (J->getOperand(0).getReg() != Reg)
            break;

          if (!sameWidth(OrigOpc, J->getOpcode()))
            break;

          Delta += D;
          ++J;
        }

        if (Delta == 0) {
          auto EraseIt = Start;
          while (EraseIt != J) {
            EraseIt = MBB.erase(EraseIt);
          }

          I = J;
          Changed = true;
          continue;
        }
        unsigned NewOpc = getNewOpcode(OrigOpc, Delta);

        auto MIB = BuildMI(MBB, Start, MI.getDebugLoc(), TII->get(NewOpc));
        MIB.addReg(Reg, RegState::Define);
        MIB.addReg(Reg);
        MIB.addImm(std::abs(Delta));

        auto EraseIt = Start;
        while (EraseIt != J) {
          EraseIt = MBB.erase(EraseIt);
        }

        I = J;
        Changed = true;
      }
    }

    return Changed;
  }

private:
  int getDelta(const MachineInstr &MI) {
    switch (MI.getOpcode()) {
    case X86::INC8r:
    case X86::INC16r:
    case X86::INC32r:
    case X86::INC64r:
      return 1;

    case X86::DEC8r:
    case X86::DEC16r:
    case X86::DEC32r:
    case X86::DEC64r:
      return -1;

    default:
      return 0;
    }
  }

  bool sameWidth(unsigned A, unsigned B) { return getWidth(A) == getWidth(B); }

  unsigned getWidth(unsigned Opc) {
    switch (Opc) {
    case X86::INC8r:
    case X86::DEC8r:
      return 8;

    case X86::INC16r:
    case X86::DEC16r:
      return 16;

    case X86::INC32r:
    case X86::DEC32r:
      return 32;

    case X86::INC64r:
    case X86::DEC64r:
      return 64;

    default:
      return 0;
    }
  }

  unsigned getNewOpcode(unsigned OrigOpc, int Delta) {
    bool IsAdd = Delta > 0;

    switch (getWidth(OrigOpc)) {
    case 8:
      return IsAdd ? X86::ADD8ri : X86::SUB8ri;
    case 16:
      return IsAdd ? X86::ADD16ri : X86::SUB16ri;
    case 32:
      return IsAdd ? X86::ADD32ri : X86::SUB32ri;
    case 64:
      return IsAdd ? X86::ADD64ri32 : X86::SUB64ri32;
    default:
      return 0;
    }
  }
};

char IncDecFoldPass::ID = 0;

} // namespace

static RegisterPass<IncDecFoldPass> X("kiselev-inc-dec-pass", "INC/DEC folding",
                                      false, false);