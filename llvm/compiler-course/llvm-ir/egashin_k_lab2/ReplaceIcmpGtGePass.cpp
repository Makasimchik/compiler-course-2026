#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <optional>

using namespace llvm;

namespace {

std::optional<ICmpInst::Predicate>
getReplacementPredicate(ICmpInst::Predicate Predicate) {
  switch (Predicate) {
  case ICmpInst::ICMP_SGT:
    return ICmpInst::ICMP_SLE;
  case ICmpInst::ICMP_SGE:
    return ICmpInst::ICMP_SLT;
  case ICmpInst::ICMP_UGT:
    return ICmpInst::ICMP_ULE;
  case ICmpInst::ICMP_UGE:
    return ICmpInst::ICMP_ULT;
  default:
    return std::nullopt;
  }
}

struct ReplaceIcmpGtGePass : PassInfoMixin<ReplaceIcmpGtGePass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;

    for (BasicBlock &BB : F) {
      for (Instruction &I : make_early_inc_range(BB)) {
        auto *Cmp = dyn_cast<ICmpInst>(&I);
        if (!Cmp)
          continue;

        const CmpPredicate OriginalPredicate = Cmp->getCmpPredicate();
        std::optional<ICmpInst::Predicate> NewPredicate =
            getReplacementPredicate(OriginalPredicate);
        if (!NewPredicate)
          continue;

        IRBuilder<> Builder(Cmp);
        auto *OppositeCmp = cast<ICmpInst>(
            Builder.CreateICmp(*NewPredicate, Cmp->getOperand(0),
                               Cmp->getOperand(1), Cmp->getName() + ".rev"));
        OppositeCmp->setSameSign(Cmp->hasSameSign());
        Value *NegatedCmp =
            Builder.CreateNot(OppositeCmp, Cmp->getName() + ".not");

        Cmp->replaceAllUsesWith(NegatedCmp);
        Cmp->eraseFromParent();
        Changed = true;
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ReplaceIcmpGtGePass", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "replace-icmp-gt-ge") {
                    FPM.addPass(ReplaceIcmpGtGePass());
                    return true;
                  }
                  return false;
                });
          }};
}
