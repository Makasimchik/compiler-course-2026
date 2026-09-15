#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

// Вспомогательная функция для замены инструкции на сдвиг влево (умножение)
void replaceWithLeftShift(llvm::BinaryOperator *oldInst,
                          llvm::Value *valToShift,
                          llvm::ConstantInt *powerOf2Const,
                          llvm::IRBuilder<> &builder) {
  uint64_t shiftAmt = powerOf2Const->getValue().logBase2();
  llvm::Value *shiftConst =
      builder.getIntN(powerOf2Const->getBitWidth(), shiftAmt);

  llvm::Value *newShift =
      builder.CreateShl(valToShift, shiftConst, "leftShift");

  oldInst->replaceAllUsesWith(newShift);
  oldInst->eraseFromParent();
}

// Вспомогательная функция для замены инструкции на сдвиг вправо (деление)
void replaceWithRightShift(llvm::BinaryOperator *oldInst,
                           llvm::Value *valToShift,
                           llvm::ConstantInt *powerOf2Const,
                           llvm::IRBuilder<> &builder, bool isArithmetic) {
  uint64_t shiftAmt = powerOf2Const->getValue().logBase2();
  llvm::Type *ty = valToShift->getType();
  llvm::Value *shiftConst = builder.getIntN(ty->getIntegerBitWidth(), shiftAmt);

  llvm::Value *newShift = nullptr;

  if (isArithmetic) {
    // Если деление точное, нам не нужны лишние вычисления
    if (oldInst->isExact()) {
      newShift = builder.CreateAShr(valToShift, shiftConst, "arithShiftRight");
    } else {
      // Генерируем смещение для отрицательных чисел во время выполнения
      // bias = (1 << shiftAmt) - 1
      uint64_t biasVal = (1ULL << shiftAmt) - 1;
      llvm::Value *biasConst =
          builder.getIntN(ty->getIntegerBitWidth(), biasVal);
      llvm::Value *zeroConst = builder.getIntN(ty->getIntegerBitWidth(), 0);

      // Проверяем: valToShift < 0 ?
      llvm::Value *isNegative =
          builder.CreateICmpSLT(valToShift, zeroConst, "isNeg");

      // Выбираем: если < 0, то bias, иначе 0
      llvm::Value *bias =
          builder.CreateSelect(isNegative, biasConst, zeroConst, "bias");

      // valToShift + bias
      llvm::Value *adjustedVal =
          builder.CreateAdd(valToShift, bias, "adjustedVal");

      // Наконец, делаем сдвиг
      newShift = builder.CreateAShr(adjustedVal, shiftConst, "arithShiftRight");
    }
  } else {
    // Беззнаковый сдвиг (UDiv)
    newShift = builder.CreateLShr(valToShift, shiftConst, "logicalShiftRight");
  }

  oldInst->replaceAllUsesWith(newShift);
  oldInst->eraseFromParent();
}

bool createShift(llvm::BinaryOperator *bOper) {
  llvm::Value *leftOperand = bOper->getOperand(0);
  llvm::Value *rightOperand = bOper->getOperand(1);

  llvm::IRBuilder<> builder(bOper);

  switch (bOper->getOpcode()) {
  case llvm::Instruction::Mul: {
    auto *c1 = llvm::dyn_cast<llvm::ConstantInt>(leftOperand);
    auto *c2 = llvm::dyn_cast<llvm::ConstantInt>(rightOperand);

    if (c1 && c1->getValue().isPowerOf2()) {
      replaceWithLeftShift(bOper, rightOperand, c1, builder);
      return true;
    } else if (c2 && c2->getValue().isPowerOf2()) {
      replaceWithLeftShift(bOper, leftOperand, c2, builder);
      return true;
    }
    break;
  }

  case llvm::Instruction::SDiv:
  case llvm::Instruction::UDiv: {
    auto *divider = llvm::dyn_cast<llvm::ConstantInt>(rightOperand);

    if (divider && divider->getValue().isPowerOf2()) {
      bool isArithmetic = (bOper->getOpcode() == llvm::Instruction::SDiv);
      replaceWithRightShift(bOper, leftOperand, divider, builder, isArithmetic);
      return true;
    }
    break;
  }
  }

  return false;
}

struct replaceWithShiftPass : llvm::PassInfoMixin<replaceWithShiftPass> {
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {
    bool changed = false;

    for (auto &baseBlock : func) {
      for (auto instruction = baseBlock.begin(), end = baseBlock.end();
           instruction != end;) {
        llvm::Instruction &I = *instruction++;

        if (auto *bOper = llvm::dyn_cast<llvm::BinaryOperator>(&I)) {
          if (createShift(bOper)) {
            changed = true;
          }
        }
      }
    }
    return changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "replaceWithShiftPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "replaceWithShiftPass") {
                    FPM.addPass(replaceWithShiftPass{});
                    return true;
                  }
                  return false;
                });
          }};
}