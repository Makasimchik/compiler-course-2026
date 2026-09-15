#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
class ChyokotovAReplacePass : public ModulePass {
public:
  static char ID;
  ChyokotovAReplacePass() : ModulePass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override { return "replace pass"; }
  bool runOnModule(Module &M) override;
};

char ChyokotovAReplacePass::ID = 0;

bool isAllowedOpcode(unsigned Opcode) {
  return (Opcode == X86::INC32r || Opcode == X86::DEC32r ||
          Opcode == X86::INC64r || Opcode == X86::DEC64r);
}

bool ChyokotovAReplacePass::runOnModule(Module &M) {
  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  bool Changed = false;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;

    const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();

    for (MachineBasicBlock &MBB : *MF) {
      for (auto MI = MBB.begin(); MI != MBB.end();) {
        MachineInstr &Inst = *MI;
        unsigned Opcode = Inst.getOpcode();

        if (isAllowedOpcode(Opcode)) {
          unsigned Reg = Inst.getOperand(0).getReg();
          bool Is32Bit = (Opcode == X86::INC32r || Opcode == X86::DEC32r);
          int Sum = (Opcode == X86::INC32r || Opcode == X86::INC64r) ? 1 : -1;

          auto NextMI = std::next(MI);
          while (NextMI != MBB.end()) {
            MachineInstr &NextInst = *NextMI;
            unsigned NextOpcode = NextInst.getOpcode();

            if (isAllowedOpcode(NextOpcode) &&
                NextInst.getOperand(0).getReg() == Reg) {

              if (NextOpcode == X86::INC32r || NextOpcode == X86::INC64r) {
                Sum += 1;
              } else {
                Sum -= 1;
              }
              NextMI = std::next(NextMI);
            } else {
              break;
            }
          }

          auto TempMI = std::next(MI);
          while (TempMI != NextMI) {
            TempMI = MBB.erase(TempMI);
          }

          if (Sum == 0) {
            MI = MBB.erase(MI);
            continue;
          } else if (Sum > 0) {
            if (Is32Bit) {
              BuildMI(MBB, MI, Inst.getDebugLoc(), TII->get(X86::ADD32ri), Reg)
                  .addReg(Reg)
                  .addImm(Sum);
            } else {
              BuildMI(MBB, MI, Inst.getDebugLoc(), TII->get(X86::ADD64ri32),
                      Reg)
                  .addReg(Reg)
                  .addImm(Sum);
            }
          } else {
            if (Is32Bit) {
              BuildMI(MBB, MI, Inst.getDebugLoc(), TII->get(X86::SUB32ri), Reg)
                  .addReg(Reg)
                  .addImm(-Sum);
            } else {
              BuildMI(MBB, MI, Inst.getDebugLoc(), TII->get(X86::SUB64ri32),
                      Reg)
                  .addReg(Reg)
                  .addImm(-Sum);
            }
          }

          MI = MBB.erase(MI);

          continue;
        }
        ++MI;
      }
      Changed = true;
    }
  }

  return Changed;
}
} // namespace

static RegisterPass<ChyokotovAReplacePass>
    X("chyokotov_a_incanddec_replace-x86", "replace pass", false, false);