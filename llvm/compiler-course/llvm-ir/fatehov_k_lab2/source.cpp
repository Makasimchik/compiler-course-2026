#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {
struct DecomposeRemPass : llvm::PassInfoMixin<DecomposeRemPass> {
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {
    bool changed = false;

    llvm::SmallVector<llvm::BinaryOperator *, 8> remInstructions;

    for (auto &block : func)
      for (auto &instr : block)
        if (auto *binOp = llvm::dyn_cast<llvm::BinaryOperator>(&instr))
          if (binOp->getOpcode() == llvm::Instruction::FRem ||
              binOp->getOpcode() == llvm::Instruction::SRem ||
              binOp->getOpcode() == llvm::Instruction::URem)
            remInstructions.push_back(binOp);

    for (auto *remInstr : remInstructions) {
      llvm::IRBuilder<> builder(remInstr);

      llvm::Value *a = remInstr->getOperand(0);
      llvm::Value *b = remInstr->getOperand(1);
      llvm::Value *div, *mul, *sub;

      switch (remInstr->getOpcode()) {
      case llvm::Instruction::FRem:
        div = builder.CreateFDiv(a, b, "fdiv_tmp");
        mul = builder.CreateIntrinsic(div->getType(), llvm::Intrinsic::trunc,
                                      {div}, nullptr, "trunc_tmp");
        mul = builder.CreateFMul(mul, b, "fmul_tmp");
        sub = builder.CreateFSub(a, mul, "frem_decomposed");
        break;

      case llvm::Instruction::SRem:
        div = builder.CreateSDiv(a, b, "sdiv_tmp");
        mul = builder.CreateMul(div, b, "mul_tmp");
        sub = builder.CreateSub(a, mul, "srem_decomposed");
        break;

      case llvm::Instruction::URem:
        div = builder.CreateUDiv(a, b, "udiv_tmp");
        mul = builder.CreateMul(div, b, "mul_tmp");
        sub = builder.CreateSub(a, mul, "urem_decomposed");
        break;

      default:
        continue;
      }

      remInstr->replaceAllUsesWith(sub);
      remInstr->eraseFromParent();
      changed = true;
    }

    return changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DecomposeRemPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "decompose-rem") {
                    FPM.addPass(DecomposeRemPass{});
                    return true;
                  }
                  return false;
                });
          }};
}