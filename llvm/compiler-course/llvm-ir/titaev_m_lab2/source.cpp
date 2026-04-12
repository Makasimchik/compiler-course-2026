#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct RemainderExpansionPass : public PassInfoMixin<RemainderExpansionPass> {

  Value *expandRemOp(Instruction *I) {
    IRBuilder<> B(I);
    Value *ValA = I->getOperand(0);
    Value *ValB = I->getOperand(1);
    auto OpCode = I->getOpcode();

    if (OpCode == Instruction::SRem) {
      Value *DivS = B.CreateSDiv(ValA, ValB, "rem.sdiv");
      Value *MulS = B.CreateMul(DivS, ValB, "rem.smul");
      return B.CreateSub(ValA, MulS, "rem.sres");
    }

    if (OpCode == Instruction::URem) {
      Value *DivU = B.CreateUDiv(ValA, ValB, "rem.udiv");
      Value *MulU = B.CreateMul(DivU, ValB, "rem.umul");
      return B.CreateSub(ValA, MulU, "rem.ures");
    }

    if (OpCode == Instruction::FRem) {
      Value *DivF = B.CreateFDiv(ValA, ValB, "rem.fdiv");
      // Форматирование согласно требованиям clang-format
      Value *Trunc =
          B.CreateUnaryIntrinsic(Intrinsic::trunc, DivF, nullptr, "rem.ftrunc");
      Value *MulF = B.CreateFMul(Trunc, ValB, "rem.fmul");
      return B.CreateFSub(ValA, MulF, "rem.fres");
    }

    return nullptr;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;
    SmallVector<Instruction *, 32> InstList;

    for (auto &BB : F) {
      for (auto &I : BB) {
        unsigned Op = I.getOpcode();
        // Форматирование согласно требованиям clang-format
        if (Op == Instruction::SRem || Op == Instruction::URem ||
            Op == Instruction::FRem) {
          InstList.push_back(&I);
        }
      }
    }

    for (Instruction *I : InstList) {
      if (Value *NewInst = expandRemOp(I)) {
        I->replaceAllUsesWith(NewInst);
        I->eraseFromParent();
        Changed = true;
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
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