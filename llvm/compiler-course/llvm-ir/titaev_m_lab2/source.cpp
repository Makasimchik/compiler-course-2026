#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct RemainderExpansionPass : public PassInfoMixin<RemainderExpansionPass> {

  Value *processRemainder(Instruction *I) {
    IRBuilder<> Builder(I);
    Value *OpA = I->getOperand(0);
    Value *OpB = I->getOperand(1);
    auto OpCode = I->getOpcode();

    if (OpCode == Instruction::SRem) {
      Value *SDiv = Builder.CreateSDiv(OpA, OpB, "rem.sdiv");
      Value *SMul = Builder.CreateMul(SDiv, OpB, "rem.smul");
      return Builder.CreateSub(OpA, SMul, "rem.sres");
    }

    if (OpCode == Instruction::URem) {
      Value *UDiv = Builder.CreateUDiv(OpA, OpB, "rem.udiv");
      Value *UMul = Builder.CreateMul(UDiv, OpB, "rem.umul");
      return Builder.CreateSub(OpA, UMul, "rem.ures");
    }

    if (OpCode == Instruction::FRem) {
      Value *FDiv = Builder.CreateFDiv(OpA, OpB, "rem.fdiv");
      // Разбиваем строку для соблюдения clang-format
      Value *Trunc = Builder.CreateUnaryIntrinsic(Intrinsic::trunc, FDiv,
                                                  nullptr, "rem.ftrunc");
      Value *FMul = Builder.CreateFMul(Trunc, OpB, "rem.fmul");
      return Builder.CreateFSub(OpA, FMul, "rem.fres");
    }

    return nullptr;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool IsChanged = false;
    SmallVector<Instruction *, 32> ToProcess;

    for (auto &BB : F) {
      for (auto &I : BB) {
        unsigned Op = I.getOpcode();
        // Разбиваем строку для соблюдения clang-format
        if (Op == Instruction::SRem || Op == Instruction::URem ||
            Op == Instruction::FRem) {
          ToProcess.push_back(&I);
        }
      }
    }

    for (Instruction *I : ToProcess) {
      if (Value *Replacement = processRemainder(I)) {
        I->replaceAllUsesWith(Replacement);
        I->eraseFromParent();
        IsChanged = true;
      }
    }

    return IsChanged ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "RemainderExpansionPlugin", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "expand-rem") {
                    FPM.addPass(RemainderExpansionPass());
                    return true;
                  }
                  return false;
                });
          }};
}