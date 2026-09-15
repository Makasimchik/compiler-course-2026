#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

namespace {

bool isFmulAdd(const llvm::Instruction &inst) {
  const auto *intr = llvm::dyn_cast<llvm::IntrinsicInst>(&inst);
  if (!intr)
    return false;

  return intr->getIntrinsicID() == llvm::Intrinsic::fmuladd;
}

llvm::Value *expandFmulAdd(llvm::IntrinsicInst &intr) {
  llvm::IRBuilder<> builder(&intr);

  auto *a = intr.getArgOperand(0);
  auto *b = intr.getArgOperand(1);
  auto *c = intr.getArgOperand(2);

  builder.setFastMathFlags(intr.getFastMathFlags());

  llvm::Value *mul = builder.CreateFMul(a, b, "mul_part");
  llvm::Value *addm = builder.CreateFAdd(mul, c, "add_part");

  return addm;
}

struct ExpandFmulAddPass : llvm::PassInfoMixin<ExpandFmulAddPass> {

  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {

    bool changed = false;

    for (auto &block : func) {
      for (auto &inst : llvm::make_early_inc_range(block)) {

        if (!isFmulAdd(inst))
          continue;

        auto &intr = llvm::cast<llvm::IntrinsicInst>(inst);

        llvm::Value *newValue = expandFmulAdd(intr);

        intr.replaceAllUsesWith(newValue);
        intr.eraseFromParent();

        changed = true;
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
  return {LLVM_PLUGIN_API_VERSION, "ExpandFmulAddPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "expandfma") {
                    FPM.addPass(ExpandFmulAddPass{});
                    return true;
                  }
                  return false;
                });
          }};
}
