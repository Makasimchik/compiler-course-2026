#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
class NikolaevDChangeIncDec : public ModulePass {
public:
  static char ID;
  NikolaevDChangeIncDec() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  bool isIncDec(unsigned Opcode) const {
    return Opcode == X86::INC32r || Opcode == X86::INC64r ||
           Opcode == X86::DEC32r || Opcode == X86::DEC64r;
  }

  int getStep(unsigned Opcode) const {
    if (Opcode == X86::INC32r || Opcode == X86::INC64r)
      return 1;
    if (Opcode == X86::DEC32r || Opcode == X86::DEC64r)
      return -1;
    return 0;
  }

  bool runOnModule(Module &M) override {
    bool Changed = false;

    auto &MMIWrapper = getAnalysis<MachineModuleInfoWrapperPass>();
    MachineModuleInfo &MMI = MMIWrapper.getMMI();

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;

      MachineFunction *MF = MMI.getMachineFunction(F);
      if (!MF)
        continue;

      const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();

      for (auto &MBB : *MF) {
        auto I = MBB.begin();
        while (I != MBB.end()) {
          unsigned Opcode = I->getOpcode();

          if (isIncDec(Opcode)) {
            Register Reg = I->getOperand(0).getReg();
            bool is64Bit = (Opcode == X86::INC64r || Opcode == X86::DEC64r);
            int Count = getStep(Opcode);

            auto NextI = std::next(I);
            while (NextI != MBB.end() && NextI->getOpcode() == Opcode &&
                   NextI->getOperand(0).getReg() == Reg) {
              Count += getStep(Opcode);
              auto ToDelete = NextI;
              NextI = std::next(NextI);
              ToDelete->eraseFromParent();
              Changed = true;
            }

            unsigned NewOpcode;
            if (Count > 0) {
              NewOpcode = is64Bit ? X86::ADD64ri8 : X86::ADD32ri8;
            } else {
              NewOpcode = is64Bit ? X86::SUB64ri8 : X86::SUB32ri8;
              Count = -Count;
            }

            BuildMI(MBB, I, I->getDebugLoc(), TII->get(NewOpcode), Reg)
                .addReg(Reg)
                .addImm(Count);

            auto ToDelete = I;
            I = NextI;
            ToDelete->eraseFromParent();
            Changed = true;
          } else {
            ++I;
          }
        }
      }
    }
    return Changed;
  }
};
} // namespace

char NikolaevDChangeIncDec::ID = 0;
static RegisterPass<NikolaevDChangeIncDec> X("nikolaev-d-change-inc-dec-module",
                                             "change pass", false, false);