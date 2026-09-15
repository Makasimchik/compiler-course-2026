#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct AshihminDDecRemFPInt : public PassInfoMixin<AshihminDDecRemFPInt> {

  Value *processRemainder(Instruction *Inst) {
    IRBuilder<> B(Inst);
    Value *OpA = Inst->getOperand(0);
    Value *OpB = Inst->getOperand(1);
    unsigned OpCode = Inst->getOpcode();

    if (OpCode == Instruction::FRem) {
      Value *FDiv = B.CreateFDiv(OpA, OpB, "frem.div");
      Value *Trunc =
          B.CreateUnaryIntrinsic(Intrinsic::trunc, FDiv, nullptr, "frem.trunc");
      Value *FMul = B.CreateFMul(Trunc, OpB, "frem.mul");
      return B.CreateFSub(OpA, FMul, "frem.res");
    }

    if (OpCode == Instruction::SRem) {
      Value *SDiv = B.CreateSDiv(OpA, OpB, "srem.div");
      Value *SMul = B.CreateMul(SDiv, OpB, "srem.mul");
      return B.CreateSub(OpA, SMul, "srem.res");
    }

    if (OpCode == Instruction::URem) {
      Value *UDiv = B.CreateUDiv(OpA, OpB, "urem.div");
      Value *UMul = B.CreateMul(UDiv, OpB, "urem.mul");
      return B.CreateSub(OpA, UMul, "urem.res");
    }

    return nullptr;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool IsChanged = false;
    SmallVector<Instruction *, 32> ToProcess;

    for (auto &BB : F) {
      for (auto &I : BB) {
        unsigned Op = I.getOpcode();
        if (Op == Instruction::FRem || Op == Instruction::SRem ||
            Op == Instruction::URem) {
          ToProcess.push_back(&I);
        }
      }
    }

    for (Instruction *I : ToProcess) {
      Value *Replacement = processRemainder(I);
      if (Replacement) {
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
  return {LLVM_PLUGIN_API_VERSION, "AshihminDDecRemFPIntPlugin", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "ashihmin_d_decompose_remainder_fp_int") {
                    FPM.addPass(AshihminDDecRemFPInt());
                    return true;
                  }
                  return false;
                });
          }};
}