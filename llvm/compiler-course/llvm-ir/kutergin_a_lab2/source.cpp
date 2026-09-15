#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct PowIToMulPass : public PassInfoMixin<PowIToMulPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    bool Changed = false;

    for (auto &BB : F) {
      for (auto IT = BB.begin(), E = BB.end(); IT != E;) {
        Instruction &I = *IT++;

        auto *CI = dyn_cast<CallInst>(&I);
        if (!CI)
          continue;

        Function *Callee = CI->getCalledFunction();
        if (!Callee || !Callee->isIntrinsic())
          continue;

        if (Callee->getIntrinsicID() != Intrinsic::powi)
          continue;

        Value *Base = CI->getArgOperand(0);
        ConstantInt *ExpCI = dyn_cast<ConstantInt>(CI->getArgOperand(1));

        if (!ExpCI)
          continue;
        int64_t Exp = ExpCI->getSExtValue();
        if (Exp < 0 || Exp > 4)
          continue;

        IRBuilder<> Builder(CI);
        Value *Result = nullptr;

        if (Exp == 0) {
          Result = ConstantFP::get(Base->getType(), 1.0);
        } else if (Exp == 1) {
          Result = Base;
        } else if (Exp == 2) {
          Result = Builder.CreateFMul(Base, Base, "powi.sq");
        } else if (Exp == 3) {
          Value *Sq = Builder.CreateFMul(Base, Base, "powi.sq");
          Result = Builder.CreateFMul(Sq, Base, "powi.cub");
        } else if (Exp == 4) {
          Value *Sq = Builder.CreateFMul(Base, Base, "powi.sq");
          Result = Builder.CreateFMul(Sq, Sq, "powi.quad");
        }

        if (Result) {
          CI->replaceAllUsesWith(Result);
          CI->eraseFromParent();
          Changed = true;
        }
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // end anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PowIToMul", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "powi-to-mul") {
                    FPM.addPass(PowIToMulPass());
                    return true;
                  }
                  return false;
                });
          }};
}