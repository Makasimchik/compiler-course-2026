#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct RemainderDecPass : public PassInfoMixin<RemainderDecPass> {

  Value *expandRem(Instruction *I) {
    IRBuilder<> Builder(I);
    Value *Left = I->getOperand(0);
    Value *Right = I->getOperand(1);
    auto Code = I->getOpcode();

    if (Code == Instruction::SRem) {
      Value *DivS = Builder.CreateSDiv(Left, Right, "s.div");
      Value *MulS = Builder.CreateMul(DivS, Right, "s.mul");
      return Builder.CreateSub(Left, MulS, "s.rem.res");
    }

    if (Code == Instruction::URem) {
      Value *DivU = Builder.CreateUDiv(Left, Right, "u.div");
      Value *MulU = Builder.CreateMul(DivU, Right, "u.mul");
      return Builder.CreateSub(Left, MulU, "u.rem.res");
    }

    if (Code == Instruction::FRem) {
      Value *DivF = Builder.CreateFDiv(Left, Right, "f.div");
      // Используем тот же подход с intrinsic, но с другими именами
      Value *TruncF = Builder.CreateUnaryIntrinsic(Intrinsic::trunc, DivF, nullptr, "f.trunc");
      Value *MulF = Builder.CreateFMul(TruncF, Right, "f.mul");
      return Builder.CreateFSub(Left, MulF, "f.rem.res");
    }

    return nullptr;
  }

  PreservedAnalyses run(Function &Func, FunctionAnalysisManager &) {
    bool MadeChange = false;
    // Сохранили SmallVector, как у друга
    SmallVector<Instruction *, 32> WorkList;

    // Проход по всем инструкциям функции
    for (auto &Block : Func) {
      for (auto &Inst : Block) {
        unsigned Op = Inst.getOpcode();
        if (Op == Instruction::SRem || Op == Instruction::URem || Op == Instruction::FRem) {
          WorkList.push_back(&Inst);
        }
      }
    }

    for (Instruction *Inst : WorkList) {
      if (Value *NewVal = expandRem(Inst)) {
        Inst->replaceAllUsesWith(NewVal);
        Inst->eraseFromParent();
        MadeChange = true;
      }
    }

    return MadeChange ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "RemainderDecPlugin", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "rem-decompose-pass") {
                    FPM.addPass(RemainderDecPass());
                    return true;
                  }
                  return false;
                });
          }};
}