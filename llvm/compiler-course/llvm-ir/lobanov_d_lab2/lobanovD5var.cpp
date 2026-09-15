#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct MulDivToShiftPass : PassInfoMixin<MulDivToShiftPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool changed = false;

    for (BasicBlock &BB : F) {
      for (Instruction &I : make_early_inc_range(BB)) {
        auto *BinOp = dyn_cast<BinaryOperator>(&I);
        if (!BinOp)
          continue;

        unsigned Opcode = BinOp->getOpcode();
        if (Opcode != Instruction::Mul && Opcode != Instruction::UDiv &&
            Opcode != Instruction::SDiv)
          continue;

        Value *LHS = BinOp->getOperand(0);
        Value *RHS = BinOp->getOperand(1);
        ConstantInt *LHSConst = dyn_cast<ConstantInt>(LHS);
        ConstantInt *RHSConst = dyn_cast<ConstantInt>(RHS);

        if (!LHSConst && !RHSConst)
          continue;

        IRBuilder<> Builder(BinOp);

        if (LHSConst && LHSConst->getValue().isPowerOf2()) {
          unsigned Shift = LHSConst->getValue().logBase2();
          if (Opcode == Instruction::Mul) {
            Value *NewInst = Builder.CreateShl(RHS, Shift);
            BinOp->replaceAllUsesWith(NewInst);
            BinOp->eraseFromParent();
            changed = true;
            continue;
          }
        }

        if (RHSConst && RHSConst->getValue().isPowerOf2()) {
          unsigned Shift = RHSConst->getValue().logBase2();
          Value *NewInst = nullptr;

          if (Opcode == Instruction::Mul) {
            NewInst = Builder.CreateShl(LHS, Shift);
          } else if (Opcode == Instruction::UDiv) {
            NewInst = Builder.CreateLShr(LHS, Shift);
          } else if (Opcode == Instruction::SDiv) {
            if (BinOp->isExact()) {
              NewInst = Builder.CreateAShr(LHS, Shift);
            } else {
              uint64_t MaskVal = (1ULL << Shift) - 1;
              Constant *MaskConst = ConstantInt::get(BinOp->getType(), MaskVal);
              Constant *ZeroConst = ConstantInt::get(BinOp->getType(), 0);
              Value *IsNeg = Builder.CreateICmpSLT(LHS, ZeroConst);
              Value *AdjustedLHS = Builder.CreateAdd(LHS, MaskConst);
              Value *Selected = Builder.CreateSelect(IsNeg, AdjustedLHS, LHS);
              NewInst = Builder.CreateAShr(Selected, Shift);
            }
          }

          if (NewInst) {
            BinOp->replaceAllUsesWith(NewInst);
            BinOp->eraseFromParent();
            changed = true;
          }
        }
      }
    }

    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MulDivToShiftPass", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (name == "mul-div-to-shift") {
                    FPM.addPass(MulDivToShiftPass{});
                    return true;
                  }
                  return false;
                });
          }};
}