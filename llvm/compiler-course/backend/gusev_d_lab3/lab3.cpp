#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

#include <cstdlib>
#include <iterator>

using namespace llvm;

namespace {
class GusevDLab3Pass : public MachineFunctionPass {
public:
  static char ID;

  GusevDLab3Pass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        int Delta = getIncDecDelta(*It);
        if (Delta == 0 || !isSupportedRegisterIncDec(*It)) {
          ++It;
          continue;
        }

        MachineInstr &First = *It;
        Register DstReg = First.getOperand(0).getReg();
        Register SrcReg = First.getOperand(1).getReg();
        auto End = std::next(It);

        while (End != MBB.end()) {
          int NextDelta = getIncDecDelta(*End);
          if (NextDelta == 0 || !isSupportedRegisterIncDec(*End) ||
              End->getOperand(0).getReg() != DstReg ||
              End->getOperand(1).getReg() != SrcReg) {
            break;
          }

          Delta += NextDelta;
          ++End;
        }

        MachineBasicBlock::iterator InsertPos = First.getIterator();
        if (Delta != 0) {
          unsigned AbsDelta = std::abs(Delta);
          unsigned NewOpcode =
              getAddSubOpcode(First.getOpcode(), Delta, AbsDelta <= 127);
          BuildMI(MBB, InsertPos, First.getDebugLoc(), TII->get(NewOpcode),
                  DstReg)
              .addReg(SrcReg)
              .addImm(AbsDelta);
        }

        It = MBB.erase(It, End);
        Changed = true;
      }
    }

    return Changed;
  }

private:
  static int getIncDecDelta(const MachineInstr &MI) {
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

  static bool isSupportedRegisterIncDec(const MachineInstr &MI) {
    return MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
           MI.getOperand(1).isReg();
  }

  static unsigned getAddSubOpcode(unsigned IncDecOpcode, int Delta,
                                  bool FitsInSignedByte) {
    bool UseAdd = Delta > 0;
    switch (IncDecOpcode) {
    case X86::INC8r:
    case X86::DEC8r:
      return UseAdd ? X86::ADD8ri : X86::SUB8ri;
    case X86::INC16r:
    case X86::DEC16r:
      if (FitsInSignedByte)
        return UseAdd ? X86::ADD16ri8 : X86::SUB16ri8;
      return UseAdd ? X86::ADD16ri : X86::SUB16ri;
    case X86::INC32r:
    case X86::DEC32r:
      if (FitsInSignedByte)
        return UseAdd ? X86::ADD32ri8 : X86::SUB32ri8;
      return UseAdd ? X86::ADD32ri : X86::SUB32ri;
    case X86::INC64r:
    case X86::DEC64r:
      if (FitsInSignedByte)
        return UseAdd ? X86::ADD64ri8 : X86::SUB64ri8;
      return UseAdd ? X86::ADD64ri32 : X86::SUB64ri32;
    default:
      llvm_unreachable("unsupported inc/dec opcode");
    }
  }
};
} // namespace

char GusevDLab3Pass::ID = 0;

static RegisterPass<GusevDLab3Pass>
    X("gusev-d-lab3", "Replace X86 inc/dec with add/sub and fold sequences",
      false, false);
