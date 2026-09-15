#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class KhruevAIncReplacePass : public MachineFunctionPass {
public:
  static char ID;
  KhruevAIncReplacePass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "Inc/Dec Fusion Optimizer"; }

private:
  bool getInstructionDelta(unsigned Opcode, int &OutDelta, bool &OutIs64Bit) {
    switch (Opcode) {
    case X86::INC32r:
      OutDelta = 1;
      OutIs64Bit = false;
      return true;
    case X86::DEC32r:
      OutDelta = -1;
      OutIs64Bit = false;
      return true;
    case X86::INC64r:
      OutDelta = 1;
      OutIs64Bit = true;
      return true;
    case X86::DEC64r:
      OutDelta = -1;
      OutIs64Bit = true;
      return true;
    default:
      return false;
    }
  }
};

char KhruevAIncReplacePass::ID = 0;

bool KhruevAIncReplacePass::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  bool IsModified = false;

  for (MachineBasicBlock &MBB : MF) {
    auto Iter = MBB.begin();

    while (Iter != MBB.end()) {
      int CurrentDelta = 0;
      bool Is64BitMode = false;

      if (!getInstructionDelta(Iter->getOpcode(), CurrentDelta, Is64BitMode)) {
        ++Iter;
        continue;
      }

      SmallVector<MachineInstr *, 8> InstrChain;
      MachineInstr &StartMI = *Iter;

      Register OriginalSrcReg = StartMI.getOperand(1).getReg();
      Register CurrentDstReg = StartMI.getOperand(0).getReg();

      int AccumulatedOffset = CurrentDelta;
      InstrChain.push_back(&StartMI);

      auto LookAheadIter = std::next(Iter);

      while (LookAheadIter != MBB.end()) {
        int NextDelta = 0;
        bool NextIs64Bit = false;

        if (!getInstructionDelta(LookAheadIter->getOpcode(), NextDelta,
                                 NextIs64Bit) ||
            NextIs64Bit != Is64BitMode) {
          break;
        }

        if (LookAheadIter->getOperand(1).getReg() != CurrentDstReg) {
          break;
        }

        AccumulatedOffset += NextDelta;
        CurrentDstReg = LookAheadIter->getOperand(0).getReg();
        InstrChain.push_back(&*LookAheadIter);
        ++LookAheadIter;
      }

      MachineInstr *LastMI = InstrChain.back();
      DebugLoc DLoc = LastMI->getDebugLoc();

      if (AccumulatedOffset > 0) {
        unsigned AddOpcode = Is64BitMode ? X86::ADD64ri32 : X86::ADD32ri;
        BuildMI(MBB, LastMI, DLoc, TII->get(AddOpcode), CurrentDstReg)
            .addReg(OriginalSrcReg)
            .addImm(AccumulatedOffset);
      } else if (AccumulatedOffset < 0) {
        unsigned SubOpcode = Is64BitMode ? X86::SUB64ri32 : X86::SUB32ri;
        BuildMI(MBB, LastMI, DLoc, TII->get(SubOpcode), CurrentDstReg)
            .addReg(OriginalSrcReg)
            .addImm(-AccumulatedOffset);
      } else {
        if (OriginalSrcReg != CurrentDstReg) {
          BuildMI(MBB, LastMI, DLoc, TII->get(TargetOpcode::COPY),
                  CurrentDstReg)
              .addReg(OriginalSrcReg);
        }
      }

      for (MachineInstr *MI : InstrChain) {
        MI->eraseFromParent();
      }

      IsModified = true;
      Iter = LookAheadIter;
    }
  }

  return IsModified;
}

} // namespace

static RegisterPass<KhruevAIncReplacePass> X("khruev_a_incdec_replace-x86",
                                             "replace pass", false, false);