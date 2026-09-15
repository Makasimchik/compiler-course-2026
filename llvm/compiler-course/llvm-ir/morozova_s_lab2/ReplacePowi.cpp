#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct ReplacePowi : public PassInfoMixin<ReplacePowi> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    bool Changed = false;

    for (BasicBlock &BB : F) {
      SmallVector<CallInst *, 4> PowiCalls;

      for (Instruction &I : BB) {
        if (auto *Call = dyn_cast<CallInst>(&I)) {
          Function *Callee = Call->getCalledFunction();
          if (Callee && Callee->getName().contains("llvm.powi")) {
            PowiCalls.push_back(Call);
          }
        }
      }

      for (CallInst *Call : PowiCalls) {
        if (Call->arg_size() < 2)
          continue;

        Value *PowerArg = Call->getArgOperand(1);
        auto *ConstPower = dyn_cast<ConstantInt>(PowerArg);
        if (!ConstPower)
          continue;

        int64_t Power = ConstPower->getSExtValue();
        if (Power < 0 || Power > 4)
          continue;

        Value *Base = Call->getArgOperand(0);
        IRBuilder<> Builder(Call);
        Value *NewResult = nullptr;

        if (Power == 0) {
          NewResult = ConstantFP::get(Base->getType(), 1.0);
        } else if (Power == 1) {
          NewResult = Base;
        } else if (Power == 2) {
          NewResult = Builder.CreateFMul(Base, Base);
        } else if (Power == 3) {
          Value *Sq = Builder.CreateFMul(Base, Base);
          NewResult = Builder.CreateFMul(Sq, Base);
        } else if (Power == 4) {
          Value *Sq = Builder.CreateFMul(Base, Base);
          NewResult = Builder.CreateFMul(Sq, Sq);
        }

        if (NewResult) {
          Call->replaceAllUsesWith(NewResult);
          Call->eraseFromParent();
          Changed = true;
        }
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ReplacePowi", "v0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "replace-powi") {
                    FPM.addPass(ReplacePowi());
                    return true;
                  }
                  return false;
                });
          }};
}
