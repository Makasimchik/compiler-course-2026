#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct ICmpInvertPass : PassInfoMixin<ICmpInvertPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;

    for (auto &BB : F) {
      for (auto II = BB.begin(), IE = BB.end(); II != IE;) {
        Instruction &I = *II++;

        if (auto *ICI = dyn_cast<ICmpInst>(&I)) {
          CmpInst::Predicate OldPred = ICI->getPredicate();
          CmpInst::Predicate NewPred;
          bool NeedsInversion = false;

          switch (OldPred) {
          case CmpInst::ICMP_SGT:
            NewPred = CmpInst::ICMP_SLE;
            NeedsInversion = true;
            break;
          case CmpInst::ICMP_UGT:
            NewPred = CmpInst::ICMP_ULE;
            NeedsInversion = true;
            break;
          case CmpInst::ICMP_SGE:
            NewPred = CmpInst::ICMP_SLT;
            NeedsInversion = true;
            break;
          case CmpInst::ICMP_UGE:
            NewPred = CmpInst::ICMP_ULT;
            NeedsInversion = true;
            break;
          default:
            break;
          }

          if (NeedsInversion) {
            IRBuilder<> Builder(ICI);
            Value *NewCmp =
                Builder.CreateICmp(NewPred, ICI->getOperand(0),
                                   ICI->getOperand(1), ICI->getName() + ".rev");
            Value *NotCmp = Builder.CreateNot(NewCmp, ICI->getName() + ".not");

            ICI->replaceAllUsesWith(NotCmp);
            ICI->eraseFromParent();
            Changed = true;
          }
        }
      }
    }
    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ICmpInvertPass", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "icmp-invert") {
                    FPM.addPass(ICmpInvertPass{});
                    return true;
                  }
                  return false;
                });
          }};
}