#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include <optional>
#include <vector>

using namespace llvm;

namespace {

struct IncDecInfo {
  int Delta;
  unsigned AddOpcode;
  unsigned SubOpcode;
};

class IncDecToAddSubPass : public MachineFunctionPass {
public:
  static char ID;
  IncDecToAddSubPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  std::optional<IncDecInfo> getIncDecInfo(unsigned Opc) const {
    switch (Opc) {
    case X86::INC8r:
      return IncDecInfo{1, X86::ADD8ri, X86::SUB8ri};
    case X86::DEC8r:
      return IncDecInfo{-1, X86::ADD8ri, X86::SUB8ri};

    case X86::INC16r:
      return IncDecInfo{1, X86::ADD16ri, X86::SUB16ri};
    case X86::DEC16r:
      return IncDecInfo{-1, X86::ADD16ri, X86::SUB16ri};

    case X86::INC32r:
      return IncDecInfo{1, X86::ADD32ri, X86::SUB32ri};
    case X86::DEC32r:
      return IncDecInfo{-1, X86::ADD32ri, X86::SUB32ri};

    case X86::INC64r:
      return IncDecInfo{1, X86::ADD64ri32, X86::SUB64ri32};
    case X86::DEC64r:
      return IncDecInfo{-1, X86::ADD64ri32, X86::SUB64ri32};

    default:
      return std::nullopt;
    }
  }
};

char IncDecToAddSubPass::ID = 0;

bool IncDecToAddSubPass::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  bool Changed = false;

  for (auto &MBB : MF) {
    for (auto It = MBB.begin(); It != MBB.end();) {
      auto Info = getIncDecInfo(It->getOpcode());

      if (!Info) {
        ++It;
        continue;
      }

      Register TargetReg = It->getOperand(0).getReg();
      int TotalDelta = Info->Delta;
      unsigned TargetAdd = Info->AddOpcode;
      unsigned TargetSub = Info->SubOpcode;

      std::vector<MachineInstr *> InstsToRemove;
      InstsToRemove.push_back(&*It);

      auto NextIt = std::next(It);

      while (NextIt != MBB.end()) {
        auto NextInfo = getIncDecInfo(NextIt->getOpcode());

        if (!NextInfo)
          break;

        if (NextIt->getOperand(0).getReg() != TargetReg ||
            NextInfo->AddOpcode != TargetAdd)
          break;

        TotalDelta += NextInfo->Delta;
        InstsToRemove.push_back(&*NextIt);
        ++NextIt;
      }

      if (TotalDelta != 0) {
        unsigned NewOpc = (TotalDelta > 0) ? TargetAdd : TargetSub;
        int AbsDelta = std::abs(TotalDelta);

        BuildMI(MBB, It, It->getDebugLoc(), TII->get(NewOpc), TargetReg)
            .addReg(TargetReg)
            .addImm(AbsDelta);
      }

      for (MachineInstr *MI : InstsToRemove) {
        MI->eraseFromParent();
      }

      Changed = true;
      It = NextIt;
    }
  }

  return Changed;
}

} // namespace

static RegisterPass<IncDecToAddSubPass>
    X("inc-dec-to-add-sub", "Replace INC/DEC sequences with single ADD/SUB",
      false, false);