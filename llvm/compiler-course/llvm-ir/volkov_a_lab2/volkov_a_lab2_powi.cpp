#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

namespace {
struct VolkovPowiPass : llvm::PassInfoMixin<VolkovPowiPass> {
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
    bool isModified = false;
    std::vector<llvm::IntrinsicInst *> worklist;

    // собираем интринсики заранее, чтобы безопасно модифицировать базовые блоки
    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *intrin = llvm::dyn_cast<llvm::IntrinsicInst>(&I)) {
          if (intrin->getIntrinsicID() == llvm::Intrinsic::powi) {
            worklist.push_back(intrin);
          }
        }
      }
    }

    for (auto *powiInst : worklist) {
      auto *constExp =
          llvm::dyn_cast<llvm::ConstantInt>(powiInst->getOperand(1));
      if (!constExp)
        continue;

      int64_t expVal = constExp->getSExtValue();
      if (expVal < 0 || expVal > 4)
        continue;

      llvm::Value *baseVal = powiInst->getOperand(0);
      llvm::IRBuilder<> irb(powiInst);
      llvm::Value *newInst = nullptr;

      // разворачиваем степени в умножения
      if (expVal == 0) {
        llvm::Type *bType = baseVal->getType();
        llvm::Value *fpOne = llvm::ConstantFP::get(bType->getScalarType(), 1.0);

        if (bType->isVectorTy()) {
          fpOne = llvm::ConstantVector::getSplat(
              llvm::cast<llvm::VectorType>(bType)->getElementCount(),
              llvm::cast<llvm::Constant>(fpOne));
        }
        newInst = fpOne;
      } else if (expVal == 1) {
        newInst = baseVal;
      } else if (expVal == 2) {
        newInst = irb.CreateFMul(baseVal, baseVal);
      } else if (expVal == 3) {
        auto *sqr = irb.CreateFMul(baseVal, baseVal);
        newInst = irb.CreateFMul(sqr, baseVal);
      } else if (expVal == 4) {
        auto *sqr = irb.CreateFMul(baseVal, baseVal);
        newInst = irb.CreateFMul(sqr, sqr);
      }

      if (newInst) {
        powiInst->replaceAllUsesWith(newInst);
        powiInst->eraseFromParent();
        isModified = true;
      }
    }

    return isModified ? llvm::PreservedAnalyses::none()
                      : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "VolkovPowiPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (Name == "volkov-powi-opt") {
                    FPM.addPass(VolkovPowiPass{});
                    return true;
                  }
                  return false;
                });
          }};
}
