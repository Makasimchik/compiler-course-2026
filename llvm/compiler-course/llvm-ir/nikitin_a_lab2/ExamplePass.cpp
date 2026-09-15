#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct ICmpSwapPass : PassInfoMixin<ICmpSwapPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    for (BasicBlock &BB : F) {
      std::vector<Instruction *> ToReplace;

      for (Instruction &I : BB) {
        auto *ICmp = dyn_cast<ICmpInst>(&I);
        if (!ICmp)
          continue;

        CmpInst::Predicate Pred = ICmp->getPredicate();
        CmpInst::Predicate NewPred;

        if (Pred == CmpInst::ICMP_SGT) {
          NewPred = CmpInst::ICMP_SLE;
        } else if (Pred == CmpInst::ICMP_SGE) {
          NewPred = CmpInst::ICMP_SLT;
        } else {
          continue;
        }

        // Создаём новую инструкцию icmp перед старой
        ICmpInst *NewICmp =
            new ICmpInst(ICmp->getIterator(), NewPred, ICmp->getOperand(0),
                         ICmp->getOperand(1), ICmp->getName());

        // Создаём инверсию (xor true = not) после новой icmp
        BinaryOperator *Not =
            BinaryOperator::CreateNot(NewICmp, ICmp->getName() + ".not");
        Not->insertAfter(NewICmp);

        // Заменяем старый ICmp на Not
        ICmp->replaceAllUsesWith(Not);
        ToReplace.push_back(ICmp);
      }

      for (Instruction *I : ToReplace) {
        I->eraseFromParent();
      }
    }
    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ICmpSwapPass", "0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (Name == "icmp-swap") {
                    FPM.addPass(ICmpSwapPass{});
                    return true;
                  }
                  return false;
                });
          }};
}