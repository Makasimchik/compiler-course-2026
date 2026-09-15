#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
class IncDecToAddSubPass : public MachineFunctionPass {
public:
  static char ID;
  IncDecToAddSubPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto MII = MBB.begin(), MIE = MBB.end(); MII != MIE;) {
        MachineInstr &MI = *MII;
        unsigned Opcode = MI.getOpcode();

        if (Opcode == X86::INC32r || Opcode == X86::DEC32r) {
          Register Reg = MI.getOperand(0).getReg();
          int TotalDelta = (Opcode == X86::INC32r) ? 1 : -1;

          auto NextI = std::next(MII);
          std::vector<MachineInstr *> ToDelete;
          ToDelete.push_back(&MI);

          while (NextI != MIE) {
            unsigned NextOp = NextI->getOpcode();
            if ((NextOp == X86::INC32r || NextOp == X86::DEC32r) &&
                NextI->getOperand(0).getReg() == Reg) {
              TotalDelta += (NextOp == X86::INC32r) ? 1 : -1;
              ToDelete.push_back(&*NextI);
              NextI = std::next(NextI);
            } else {
              break;
            }
          }

          if (TotalDelta != 0) {
            DebugLoc DL = MI.getDebugLoc();
            unsigned NewOp = (TotalDelta > 0) ? X86::ADD32ri8 : X86::SUB32ri8;
            int Val = std::abs(TotalDelta);

            BuildMI(MBB, *ToDelete.back(), DL, TII->get(NewOp), Reg)
                .addReg(Reg)
                .addImm(Val);
          }

          for (auto *Inst : ToDelete) {
            Inst->eraseFromParent();
          }
          MII = NextI;
          Changed = true;
        } else {
          ++MII;
        }
      }
    }
    return Changed;
  }

  StringRef getPassName() const override { return "X86 IncDec to AddSub Pass"; }
};
} // namespace

char IncDecToAddSubPass::ID = 0;
static RegisterPass<IncDecToAddSubPass>
    X("x86-incdec-to-addsub", "Combine INC/DEC into ADD/SUB", false, false);

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "IncDecToAddSub", LLVM_VERSION_STRING,
          nullptr};
}
