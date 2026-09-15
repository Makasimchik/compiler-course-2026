#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

static void traverse(Operation *op, int depth, int &maxDepth) {
  if (isa<scf::ForOp, scf::ForallOp, scf::IfOp, scf::WhileOp,
          scf::IndexSwitchOp, affine::AffineForOp, affine::AffineIfOp,
          affine::AffineParallelOp>(op))
    depth++;

  if (depth > maxDepth)
    maxDepth = depth;

  for (Region &region : op->getRegions())
    for (Block &block : region)
      for (Operation &child : block)
        traverse(&child, depth, maxDepth);
}

namespace {

struct MaxBlockDepthPass
    : public PassWrapper<MaxBlockDepthPass, OperationPass<func::FuncOp>> {

  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MaxBlockDepthPass)

  StringRef getArgument() const final { return "konstantinov_s_lab4_MLIR"; }
  StringRef getDescription() const final {
    return "Counts maximum control-flow nesting depth inside a function "
           "and saves the result as attribute 'max_block_depth'.";
  }

  void runOnOperation() override {
    func::FuncOp funcOp = getOperation();
    Builder builder(funcOp.getContext());

    int maxDepth = 0;
    traverse(funcOp.getOperation(), 0, maxDepth);

    funcOp->setAttr("max_block_depth", builder.getI64IntegerAttr(maxDepth));
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MaxBlockDepthPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MaxBlockDepthPass)

mlir::PassPluginLibraryInfo getMaxBlockDepthPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "BlockDepthCountPass", "1.0",
          []() { mlir::PassRegistration<MaxBlockDepthPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getMaxBlockDepthPassPluginInfo();
}