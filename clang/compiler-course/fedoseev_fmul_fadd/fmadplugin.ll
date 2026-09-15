#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct FMADecompose : public PassInfoMixin<FMADecompose> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;
    SmallVector<IntrinsicInst *, 8> Calls;

    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *II = dyn_cast<IntrinsicInst>(&I))
          if (II->getIntrinsicID() == Intrinsic::fmuladd)
            Calls.push_back(II);

    for (IntrinsicInst *Call : Calls) {
      Value *A = Call->getOperand(0);
      Value *B = Call->getOperand(1);
      Value *C = Call->getOperand(2);

      IRBuilder<> Builder(Call);
      FastMathFlags FMF = Call->getFastMathFlags();
      Builder.setFastMathFlags(FMF);

      Value *Mul = Builder.CreateFMul(A, B, "fmul");
      Value *Add = Builder.CreateFAdd(Mul, C, "fadd");

      if (auto *I = dyn_cast<Instruction>(Mul))
        I->setFastMathFlags(FMF);
      if (auto *I = dyn_cast<Instruction>(Add))
        I->setFastMathFlags(FMF);

      Call->replaceAllUsesWith(Add);
      Call->eraseFromParent();
      Changed = true;
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "FMADecompose", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "decompose-fmuladd") {
                    FPM.addPass(FMADecompose());
                    return true;
                  }
                  return false;
                });
          }};
}