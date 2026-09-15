#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

bool isControlFlowOp(Operation *op) {
  return isa<scf::ForOp, scf::IfOp, scf::WhileOp, scf::IndexSwitchOp,
             scf::ForallOp, affine::AffineForOp, affine::AffineIfOp,
             affine::AffineParallelOp>(op);
}

int computeMaxDepth(Operation *op) {
  int childMax = 0;
  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &child : block) {
        childMax = std::max(childMax, computeMaxDepth(&child));
      }
    }
  }
  return (isControlFlowOp(op) ? 1 : 0) + childMax;
}

class MaxBlockDepthPass
    : public PassWrapper<MaxBlockDepthPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "gasenin_l_max_blocks_MLIR"; }
  StringRef getDescription() const final {
    return "Annotates every func.func with its maximum control-flow nesting "
           "depth (scf + affine dialects) as 'max_block_depth' attribute.";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    MLIRContext *ctx = moduleOp.getContext();
    Builder builder(ctx);

    moduleOp.walk([&](func::FuncOp funcOp) {
      int maxDepth = 0;

      for (Region &region : funcOp->getRegions()) {
        for (Block &block : region) {
          for (Operation &op : block) {
            maxDepth = std::max(maxDepth, computeMaxDepth(&op));
          }
        }
      }

      funcOp->setAttr("max_block_depth", builder.getI64IntegerAttr(maxDepth));

      llvm::outs() << "Function '" << funcOp.getName()
                   << "' max block depth: " << maxDepth << '\n';
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MaxBlockDepthPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MaxBlockDepthPass)

mlir::PassPluginLibraryInfo getMaxBlockDepthPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaxBlockDepthPass", "1.0",
          []() { mlir::PassRegistration<MaxBlockDepthPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getMaxBlockDepthPassPluginInfo();
}
