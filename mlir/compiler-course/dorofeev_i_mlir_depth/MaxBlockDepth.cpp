#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include <algorithm>

using namespace mlir;

namespace {

struct MaxBlockDepthPass
    : public PassWrapper<MaxBlockDepthPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MaxBlockDepthPass)

  StringRef getArgument() const final { return "max-block-depth"; }
  StringRef getDescription() const final {
    return "Calculates max block depth and attaches it to func.";
  }

  void runOnOperation() override {
    func::FuncOp func = getOperation();

    int maxDepth = 0;
    if (!func.isExternal()) {
      maxDepth = calculateMaxDepth(func.getBody(), 0);
    }

    OpBuilder builder(func.getContext());
    func->setAttr("max_block_depth", builder.getI32IntegerAttr(maxDepth));
  }

private:
  int calculateMaxDepth(Region &region, int currentDepth) {
    int maxD = currentDepth;

    for (Block &block : region.getBlocks()) {
      for (Operation &op : block) {
        for (Region &nestedRegion : op.getRegions()) {
          maxD =
              std::max(maxD, calculateMaxDepth(nestedRegion, currentDepth + 1));
        }
      }
    }
    return maxD;
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaxBlockDepthPass", "v0.1",
          []() { PassRegistration<MaxBlockDepthPass>(); }};
}