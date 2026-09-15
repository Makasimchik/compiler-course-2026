#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

struct NikolaevDDecomposeFremPass
    : llvm::PassInfoMixin<NikolaevDDecomposeFremPass> {
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {
    bool changed = false;

    for (auto &BB : func) {
      for (auto it = BB.begin(); it != BB.end();) {
        llvm::Instruction &I = *it++;
        unsigned opcode = I.getOpcode();

        if (opcode == llvm::Instruction::SRem ||
            opcode == llvm::Instruction::URem ||
            opcode == llvm::Instruction::FRem) {

          llvm::IRBuilder<> builder(&I);
          llvm::Value *a = I.getOperand(0);
          llvm::Value *b = I.getOperand(1);
          llvm::Value *replacement = nullptr;

          if (opcode == llvm::Instruction::SRem) {
            llvm::Value *div = builder.CreateSDiv(a, b, "sdiv_tmp");
            llvm::Value *mul = builder.CreateMul(div, b, "mul_tmp");
            replacement = builder.CreateSub(a, mul, "sub_tmp");
          } else if (opcode == llvm::Instruction::URem) {
            llvm::Value *div = builder.CreateUDiv(a, b, "udiv_tmp");
            llvm::Value *mul = builder.CreateMul(div, b, "mul_tmp");
            replacement = builder.CreateSub(a, mul, "sub_tmp");
          } else if (opcode == llvm::Instruction::FRem) {
            llvm::Value *fdiv = builder.CreateFDiv(a, b, "fdiv_tmp");

            llvm::Module *M = func.getParent();
            llvm::Function *truncFunc = llvm::Intrinsic::getDeclaration(
                M, llvm::Intrinsic::trunc, {fdiv->getType()});

            llvm::Value *q_trunc =
                builder.CreateCall(truncFunc, {fdiv}, "trunc_tmp");
            llvm::Value *fmul = builder.CreateFMul(q_trunc, b, "fmul_tmp");
            replacement = builder.CreateFSub(a, fmul, "fsub_tmp");
          }

          if (replacement) {
            I.replaceAllUsesWith(replacement);
            I.eraseFromParent();
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
  return {LLVM_PLUGIN_API_VERSION, "NikolaevDDecomposeFremPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "decompose-rem") {
                    FPM.addPass(NikolaevDDecomposeFremPass{});
                    return true;
                  }
                  return false;
                });
          }};
}