#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct ResuctShiftsPass : public PassInfoMixin<ResuctShiftsPass> {

  PreservedAnalyses run(Function &Func, FunctionAnalysisManager &) {
    bool IsModified = false;

    // Собираем инструкции в вектор, чтобы избежать проблем с итераторами при
    // удалении
    SmallVector<BinaryOperator *, 16> WorkList;
    for (BasicBlock &Block : Func) {
      for (Instruction &Inst : Block) {
        if (auto *BinOp = dyn_cast<BinaryOperator>(&Inst)) {
          WorkList.push_back(BinOp);
        }
      }
    }

    for (BinaryOperator *TargetOp : WorkList) {
      Value *ValOperand = TargetOp->getOperand(0);
      Value *ConstOperand = TargetOp->getOperand(1);
      ConstantInt *ConstInt = dyn_cast<ConstantInt>(ConstOperand);

      // Обработка коммутативности (например, 8 * x превращается в x * 8 для
      // удобства)
      if (!ConstInt && TargetOp->getOpcode() == Instruction::Mul) {
        ConstInt = dyn_cast<ConstantInt>(ValOperand);
        std::swap(ValOperand, ConstOperand);
      }

      if (!ConstInt || !ConstInt->getValue().isPowerOf2()) {
        continue;
      }

      IRBuilder<> Builder(TargetOp);
      Value *Replacement = nullptr;
      unsigned ShiftAmount = ConstInt->getValue().logBase2();

      switch (TargetOp->getOpcode()) {
      case Instruction::Mul: {
        Replacement = Builder.CreateShl(ValOperand, ShiftAmount,
                                        TargetOp->getName() + ".shl");
        break;
      }

      case Instruction::UDiv: {
        if (ConstInt->getValue().isStrictlyPositive()) {
          Replacement = Builder.CreateLShr(ValOperand, ShiftAmount,
                                           TargetOp->getName() + ".lshr");
        }
        break;
      }

      case Instruction::SDiv: {
        Type *OpType = ValOperand->getType();

        APInt MaskValue =
            APInt::getLowBitsSet(OpType->getIntegerBitWidth(), ShiftAmount);
        Value *BiasConst = ConstantInt::get(OpType, MaskValue);
        Value *ZeroConst = ConstantInt::get(OpType, 0);

        Value *IsNegative =
            Builder.CreateICmpSLT(ValOperand, ZeroConst, "sign_check");
        Value *Bias =
            Builder.CreateSelect(IsNegative, BiasConst, ZeroConst, "bias_val");

        Value *AdjustedVal = Builder.CreateAdd(ValOperand, Bias, "adjusted");
        Replacement = Builder.CreateAShr(AdjustedVal, ShiftAmount,
                                         TargetOp->getName() + ".ashr");
        break;
      }

      default:
        break;
      }

      if (Replacement) {
        TargetOp->replaceAllUsesWith(Replacement);
        TargetOp->eraseFromParent();
        IsModified = true;
      }
    }

    return IsModified ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ResuctShifts", "1.0", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  // Имя пасса для вызова через команду opt
                  if (Name == "resuct-shifts") {
                    FPM.addPass(ResuctShiftsPass());
                    return true;
                  }
                  return false;
                });
          }};
}
