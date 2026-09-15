#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <cmath>

namespace {

class DivToShiftPass : public llvm::PassInfoMixin<DivToShiftPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
    bool Changed = false;

    for (auto &BB : F) {
      for (auto &I : llvm::make_early_inc_range(BB)) {
        // Интересуют только умножение и деление
        auto *BinOp = llvm::dyn_cast<llvm::BinaryOperator>(&I);
        if (!BinOp)
          continue;

        unsigned Opcode = BinOp->getOpcode();
        if (Opcode != llvm::Instruction::Mul &&
            Opcode != llvm::Instruction::SDiv &&
            Opcode != llvm::Instruction::UDiv) {
          continue;
        }

        // Правый операнд должен быть константой
        auto *ConstOp = llvm::dyn_cast<llvm::ConstantInt>(BinOp->getOperand(1));
        if (!ConstOp)
          continue;

        uint64_t Val = ConstOp->getZExtValue();

        // Проверка: является ли число степенью двойки (Val > 0)
        if (Val == 0 || (Val & (Val - 1)) != 0)
          continue;

        // Вычисляем степень (log2)
        unsigned ShiftAmount = llvm::Log2_64(Val);
        llvm::IRBuilder<> Builder(BinOp);
        llvm::Value *LHS = BinOp->getOperand(0);
        llvm::Value *ShiftResult = nullptr;

        // Заменяем в зависимости от операции
        if (Opcode == llvm::Instruction::Mul) {
          // a * 2^k -> a << k
          ShiftResult = Builder.CreateShl(LHS, ShiftAmount, "mul2shl");
          Changed = true;
        } else if (Opcode == llvm::Instruction::UDiv) {
          // unsigned a / 2^k -> a >> k
          ShiftResult = Builder.CreateLShr(LHS, ShiftAmount, "udiv2shr");
          Changed = true;
        } else if (Opcode == llvm::Instruction::SDiv) {
          // signed a / 2^k -> требуется коррекция для отрицательных чисел
          // (a + (1<<k)-1) >> k для a<0, иначе a>>k
          llvm::Value *IsNeg = Builder.CreateICmpSLT(LHS, Builder.getInt64(0));
          llvm::Value *AddVal = Builder.CreateSub(
              Builder.getInt64(1LL << ShiftAmount), Builder.getInt64(1));
          llvm::Value *Adjusted = Builder.CreateAdd(LHS, AddVal);
          llvm::Value *ShiftedNeg = Builder.CreateAShr(Adjusted, ShiftAmount);
          llvm::Value *ShiftedPos = Builder.CreateAShr(LHS, ShiftAmount);
          ShiftResult =
              Builder.CreateSelect(IsNeg, ShiftedNeg, ShiftedPos, "sdiv2ashr");
          Changed = true;
        }

        if (ShiftResult) {
          BinOp->replaceAllUsesWith(ShiftResult);
          BinOp->eraseFromParent();
        }
      }
    }

    if (Changed) {
      return llvm::PreservedAnalyses::none();
    }
    return llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "rychkova_lab2", "v1.0",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (Name == "div2shift") {
                    FPM.addPass(DivToShiftPass{});
                    return true;
                  }
                  return false;
                });
          }};
}