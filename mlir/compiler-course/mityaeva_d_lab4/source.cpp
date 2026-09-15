// source.cpp
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
class MaxBlockDepthPass
    : public PassWrapper<MaxBlockDepthPass, OperationPass<func::FuncOp>> {
public:
  StringRef getArgument() const final { return "mityaeva_d_lab4"; }
  StringRef getDescription() const final {
    return "Computes max depth of blocks (scf/affine if, for, while) and "
           "attaches the result as an attribute to func.func.";
  }

  void runOnOperation() override {
    func::FuncOp func = getOperation();
    int maxDepth = computeMaxDepth(func, 0);

    // Attach the computed depth as a 64‑bit integer attribute.
    auto attr = IntegerAttr::get(IntegerType::get(&getContext(), 64), maxDepth);
    func->setAttr("max_block_depth", attr);
  }

private:
  /// Recursively traverses the regions of `op` and computes the maximum
  /// depth of block‑creating operations. `currentDepth` is the depth
  /// inherited from the parent context.
  int computeMaxDepth(Operation *op, int currentDepth) {
    int maxDepth = currentDepth;
    for (Region &region : op->getRegions()) {
      for (Block &block : region) {
        for (Operation &innerOp : block) {
          int newDepth = currentDepth;
          // Any of the following operations introduces a new block depth.
          if (isa<scf::ForOp, scf::WhileOp, scf::IfOp, affine::AffineForOp,
                  affine::AffineIfOp, affine::AffineParallelOp>(innerOp)) {
            newDepth = currentDepth + 1;
          }
          int depth = computeMaxDepth(&innerOp, newDepth);
          maxDepth = std::max(maxDepth, depth);
        }
      }
    }
    return maxDepth;
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MaxBlockDepthPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MaxBlockDepthPass)

mlir::PassPluginLibraryInfo getPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaxBlockDepthPass", "1.0",
          []() { mlir::PassRegistration<MaxBlockDepthPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getPassPluginInfo();
}