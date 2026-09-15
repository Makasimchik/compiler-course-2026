#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include <vector>

using namespace llvm;

namespace {

struct OpMeta {
  int Delta;
  unsigned AddOpc;
  unsigned SubOpc;
  unsigned RegWidth;
};

class ashihmin_d_lab3 : public MachineFunctionPass {
public:
  static char ID;
  ashihmin_d_lab3() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Ashihmin D. INC/DEC Folder";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    bool IsModified = false;

    for (auto &MBB : MF) {
      auto MII = MBB.begin();
      while (MII != MBB.end()) {
        OpMeta Meta;
        if (!resolveOpMeta(MII->getOpcode(), Meta)) {
          ++MII;
          continue;
        }

        Register TargetReg = MII->getOperand(0).getReg();
        DebugLoc DL = MII->getDebugLoc();

        std::vector<MachineInstr *> Batch;
        int TotalDelta = 0;

        auto ScanIt = MII;
        while (ScanIt != MBB.end()) {
          OpMeta Current;
          if (resolveOpMeta(ScanIt->getOpcode(), Current) &&
              ScanIt->getOperand(0).getReg() == TargetReg &&
              Current.RegWidth == Meta.RegWidth) {

            TotalDelta += Current.Delta;
            Batch.push_back(&*ScanIt);
            ++ScanIt;
          } else {
            break;
          }
        }

        if (!Batch.empty()) {
          if (TotalDelta != 0) {
            unsigned NewOpc = (TotalDelta > 0) ? Meta.AddOpc : Meta.SubOpc;
            uint64_t AbsoluteVal = std::abs(TotalDelta);

            BuildMI(MBB, MII, DL, TII->get(NewOpc), TargetReg)
                .addReg(TargetReg)
                .addImm(AbsoluteVal)
                .addReg(X86::EFLAGS, RegState::Define | RegState::Implicit);
          }

          for (auto *Inst : Batch) {
            Inst->eraseFromParent();
          }
          IsModified = true;
          MII = ScanIt;
        }
      }
    }
    return IsModified;
  }

private:
  bool resolveOpMeta(unsigned Opc, OpMeta &P) const {
    switch (Opc) {
    case X86::INC8r:
      P = {1, X86::ADD8ri, X86::SUB8ri, 8};
      return true;
    case X86::DEC8r:
      P = {-1, X86::ADD8ri, X86::SUB8ri, 8};
      return true;
    case X86::INC16r:
      P = {1, X86::ADD16ri, X86::SUB16ri, 16};
      return true;
    case X86::DEC16r:
      P = {-1, X86::ADD16ri, X86::SUB16ri, 16};
      return true;
    case X86::INC32r:
      P = {1, X86::ADD32ri, X86::SUB32ri, 32};
      return true;
    case X86::DEC32r:
      P = {-1, X86::ADD32ri, X86::SUB32ri, 32};
      return true;
    case X86::INC64r:
      P = {1, X86::ADD64ri32, X86::SUB64ri32, 64};
      return true;
    case X86::DEC64r:
      P = {-1, X86::ADD64ri32, X86::SUB64ri32, 64};
      return true;
    default:
      return false;
    }
  }
};

char ashihmin_d_lab3::ID = 0;
} // namespace

static RegisterPass<ashihmin_d_lab3> X("ashihmin_d_lab3",
                                       "ashihmin_d_lab3 pass", false, false);