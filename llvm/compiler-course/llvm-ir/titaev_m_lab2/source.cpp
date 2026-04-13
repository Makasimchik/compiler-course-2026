#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct TitaevMRemainderExpansion
    : public PassInfoMixin<TitaevMRemainderExpansion> {

  Value *transformRem(Instruction *Inst) {
    IRBuilder<> Builder(Inst);
    Value *OpL = Inst->getOperand(0);
    Value *OpR = Inst->getOperand(1);
    unsigned OpCode = Inst->getOpcode();

    if (OpCode == Instruction::SRem) {
      Value *DivS = Builder.CreateSDiv(OpL, OpR, "titaev.s.div");
      Value *MulS = Builder.CreateMul(DivS, OpR, "titaev.s.mul");
      return Builder.CreateSub(OpL, MulS, "titaev.s.res");
    }

    if (OpCode == Instruction::URem) {
      Value *DivU = Builder.CreateUDiv(OpL, OpR, "titaev.u.div");
      Value *MulU = Builder.CreateMul(DivU, OpR, "titaev.u.mul");
      return Builder.CreateSub(OpL, MulU, "titaev.u.res");
    }

    if (OpCode == Instruction::FRem) {
      Value *DivF = Builder.CreateFDiv(OpL, OpR, "titaev.f.div");
      // Форматирование для clang-format (разрыв длинной строки)
      Value *Trunc = Builder.CreateUnaryIntrinsic(Intrinsic::trunc, DivF,
                                                  nullptr, "titaev.f.trunc");
      Value *MulF = Builder.CreateFMul(Trunc, OpR, "titaev.f.mul");
      return Builder.CreateFSub(OpL, MulF, "titaev.f.res");
    }

    return nullptr;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool IsChanged = false;
    SmallVector<Instruction *, 32> WorkList;

    for (auto &BB : F) {
      for (auto &I : BB) {
        unsigned Op = I.getOpcode();
        if (Op == Instruction::SRem || Op == Instruction::URem ||
            Op == Instruction::FRem) {
          WorkList.push_back(&I);
        }
      }
    }

    for (Instruction *I : WorkList) {
      if (Value *Replacement = transformRem(I)) {
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
  return {LLVM_PLUGIN_API_VERSION, "TitaevMRemainderExpansionPlugin", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "titaev-m-expand-rem") {
                    FPM.addPass(TitaevMRemainderExpansion());
                    return true;
                  }
                  return false;
                });
          }};
}