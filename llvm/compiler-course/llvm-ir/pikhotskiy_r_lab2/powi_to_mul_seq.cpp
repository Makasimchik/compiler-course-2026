#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

namespace {

llvm::Constant *buildOneConstant(llvm::Type *Ty) {
  if (!Ty)
    return nullptr;

  llvm::Type *ScalarTy = Ty->getScalarType();
  llvm::Constant *OneScalar = llvm::ConstantFP::get(ScalarTy, 1.0);

  if (Ty->isVectorTy()) {
    auto *VecTy = llvm::cast<llvm::VectorType>(Ty);
    return llvm::ConstantVector::getSplat(VecTy->getElementCount(), OneScalar);
  }

  return OneScalar;
}

llvm::Value *expandPowi(llvm::IRBuilder<> &Builder,
                        llvm::Instruction *InsertBefore, llvm::Value *Base,
                        int64_t Exp) {
  if (!InsertBefore || !Base)
    return nullptr;

  Builder.SetInsertPoint(InsertBefore);

  switch (Exp) {
  case 0:
    return buildOneConstant(Base->getType());
  case 1:
    return Base;
  case 2:
    return Builder.CreateFMul(Base, Base, "powi.mul2");
  case 3: {
    llvm::Value *Square = Builder.CreateFMul(Base, Base, "powi.mul3.sq");
    return Builder.CreateFMul(Square, Base, "powi.mul3");
  }
  case 4: {
    llvm::Value *Square = Builder.CreateFMul(Base, Base, "powi.mul4.sq");
    return Builder.CreateFMul(Square, Square, "powi.mul4");
  }
  default:
    return nullptr;
  }
}

struct PikhotskiyPowiPass : llvm::PassInfoMixin<PikhotskiyPowiPass> {
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
    bool Changed = false;

    for (llvm::BasicBlock &BB : F) {
      for (llvm::Instruction &I : llvm::make_early_inc_range(BB)) {
        auto *Powi = llvm::dyn_cast<llvm::IntrinsicInst>(&I);
        if (!Powi || Powi->getIntrinsicID() != llvm::Intrinsic::powi)
          continue;

        auto *ConstExp =
            llvm::dyn_cast<llvm::ConstantInt>(Powi->getArgOperand(1));
        if (!ConstExp)
          continue;

        int64_t Exp = ConstExp->getSExtValue();
        if (Exp < 0 || Exp > 4)
          continue;

        llvm::IRBuilder<> Builder(Powi);
        Builder.setFastMathFlags(Powi->getFastMathFlags());

        llvm::Value *NewValue =
            expandPowi(Builder, Powi, Powi->getArgOperand(0), Exp);
        if (!NewValue)
          continue;

        Powi->replaceAllUsesWith(NewValue);
        Powi->eraseFromParent();
        Changed = true;
      }
    }

    if (!Changed)
      return llvm::PreservedAnalyses::all();

    llvm::PreservedAnalyses PA;
    PA.preserveSet<llvm::CFGAnalyses>();
    return PA;
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PikhotskiyPowiPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (Name == "pikhotskiy-powi-to-mul") {
                    FPM.addPass(PikhotskiyPowiPass{});
                    return true;
                  }
                  return false;
                });
          }};
}
