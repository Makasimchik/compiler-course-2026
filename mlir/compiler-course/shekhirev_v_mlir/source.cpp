#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/Debug.h"

using namespace mlir;

#define DEBUG_TYPE "shekhirev-max-depth"

namespace {

bool isControlFlowConstruct(Operation *op) {
  return isa<scf::ForOp, scf::IfOp, scf::WhileOp, scf::IndexSwitchOp,
             scf::ForallOp, affine::AffineForOp, affine::AffineIfOp,
             affine::AffineParallelOp>(op);
}

uint64_t calculateNestingDepth(Operation *op) {
  uint64_t maxInnerDepth = 0;

  for (Region &region : op->getRegions()) {
    for (Block &block : region.getBlocks()) {
      for (Operation &childOp : block.getOperations()) {
        maxInnerDepth =
            std::max(maxInnerDepth, calculateNestingDepth(&childOp));
      }
    }
  }

  uint64_t currentWeight = isControlFlowConstruct(op) ? 1 : 0;
  return currentWeight + maxInnerDepth;
}

struct DepthAnalyzerPass
    : public PassWrapper<DepthAnalyzerPass, OperationPass<ModuleOp>> {

  StringRef getArgument() const final { return "shekhirev_v_max_depth_MLIR"; }
  StringRef getDescription() const final {
    return "Analyzes functions to find max control-flow depth and adds it as "
           "an attribute.";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    OpBuilder builder(module.getContext());

    for (auto func : module.getOps<func::FuncOp>()) {
      uint64_t maxFuncDepth = 0;

      func.walk([&](Operation *op) {
        maxFuncDepth = std::max(maxFuncDepth, calculateNestingDepth(op));
      });

      func->setAttr("max_block_depth", builder.getI64IntegerAttr(maxFuncDepth));

      LLVM_DEBUG(llvm::dbgs() << "Function '" << func.getName()
                              << "' max block depth: " << maxFuncDepth << '\n');
    }
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(DepthAnalyzerPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(DepthAnalyzerPass)

mlir::PassPluginLibraryInfo getDepthAnalyzerPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "DepthAnalyzerPass", "1.0",
          []() { mlir::PassRegistration<DepthAnalyzerPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getDepthAnalyzerPassPluginInfo();
}