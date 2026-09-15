#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class SavvaMulDivToShiftPass final
    : public llvm::PassInfoMixin<SavvaMulDivToShiftPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {

    bool Modified = false;

    for (auto &BB : F) {
      for (auto &I : llvm::make_early_inc_range(BB)) {

        auto *BinOp = llvm::dyn_cast<llvm::BinaryOperator>(&I);
        if (!BinOp)
          continue;

        unsigned Opcode = BinOp->getOpcode();
        if (Opcode != llvm::Instruction::Mul &&
            Opcode != llvm::Instruction::SDiv &&
            Opcode != llvm::Instruction::UDiv)
          continue;

        llvm::ConstantInt *ConstOp = nullptr;
        llvm::Value *VarOp = nullptr;

        // Умножение: константа может быть слева или справа
        if (Opcode == llvm::Instruction::Mul) {
          if (auto *C =
                  llvm::dyn_cast<llvm::ConstantInt>(BinOp->getOperand(1))) {
            ConstOp = C;
            VarOp = BinOp->getOperand(0);
          } else if (auto *C = llvm::dyn_cast<llvm::ConstantInt>(
                         BinOp->getOperand(0))) {
            ConstOp = C;
            VarOp = BinOp->getOperand(1);
          }
        }
        // Деление: константа только справа
        else {
          ConstOp = llvm::dyn_cast<llvm::ConstantInt>(BinOp->getOperand(1));
          VarOp = BinOp->getOperand(0);
        }

        if (!ConstOp || !VarOp)
          continue;

        const auto &AP = ConstOp->getValue();
        if (!AP.isPowerOf2() || !AP.isStrictlyPositive())
          continue;

        unsigned ShiftBits = AP.logBase2();

        llvm::IRBuilder<> Builder(BinOp);
        auto *ShiftAmount = llvm::ConstantInt::get(BinOp->getType(), ShiftBits);
        llvm::Value *Result = nullptr;

        switch (Opcode) {
        case llvm::Instruction::Mul:
          Result = Builder.CreateShl(VarOp, ShiftAmount, "savva.shl");
          break;
        case llvm::Instruction::UDiv:
          Result = Builder.CreateLShr(VarOp, ShiftAmount, "savva.lshr");
          break;
        case llvm::Instruction::SDiv:
          Result =
              createSignedShift(Builder, VarOp, ShiftAmount, BinOp->getType());
          break;
        }

        // Result всегда валиден после успешного switch
        BinOp->replaceAllUsesWith(Result);
        BinOp->eraseFromParent();
        Modified = true;
      }
    }

    return Modified ? llvm::PreservedAnalyses::none()
                    : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }

private:
  llvm::Value *createSignedShift(llvm::IRBuilder<> &Builder, llvm::Value *LHS,
                                 llvm::Value *Shift, llvm::Type *Ty) {
    unsigned BitWidth = Ty->getIntegerBitWidth();
    unsigned ShiftVal = llvm::cast<llvm::ConstantInt>(Shift)->getZExtValue();

    // Безопасное создание маски через APInt (поддерживает i128, i256 и т.д.)
    llvm::APInt MaskVal = llvm::APInt::getLowBitsSet(BitWidth, ShiftVal);
    auto *CorrectionMask = llvm::ConstantInt::get(Ty, MaskVal);

    auto *Zero = llvm::ConstantInt::get(Ty, 0);
    auto *IsNegative = Builder.CreateICmpSLT(LHS, Zero, "savva.isneg");
    auto *Correction =
        Builder.CreateSelect(IsNegative, CorrectionMask, Zero, "savva.corr");
    auto *Adjusted = Builder.CreateAdd(LHS, Correction, "savva.adjusted");

    return Builder.CreateAShr(Adjusted, Shift, "savva.ashr");
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "SavvaMulDivToShiftPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "savva-mul-div-to-shift-pass") {
                    FPM.addPass(SavvaMulDivToShiftPass{});
                    return true;
                  }
                  return false;
                });
          }};
}
