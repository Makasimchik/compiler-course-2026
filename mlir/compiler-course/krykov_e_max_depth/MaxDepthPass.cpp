#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

static bool isNestingOp(Operation *op) {
  return isa<scf::IfOp, scf::ForOp, scf::WhileOp, scf::ParallelOp,
             affine::AffineIfOp, affine::AffineForOp, affine::AffineParallelOp>(
      op);
}

static int computeMaxDepth(Operation *op, int currentDepth) {
  int maxDepth = currentDepth;

  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {
        int childDepth = currentDepth;
        if (isNestingOp(&nested)) {
          childDepth = currentDepth + 1;
        }
        int result = computeMaxDepth(&nested, childDepth);
        if (result > maxDepth)
          maxDepth = result;
      }
    }
  }

  return maxDepth;
}

class MaxNestingDepthPass
    : public PassWrapper<MaxNestingDepthPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "max-nesting-depth"; }
  StringRef getDescription() const final {
    return "Computes the maximum control-flow nesting depth inside each "
           "func.func and attaches it as an attribute 'max_nesting_depth'";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(moduleOp.getContext());
    SmallVector<std::pair<func::FuncOp, int>, 8> results;

    moduleOp.walk([&](func::FuncOp funcOp) {
      int depth = computeMaxDepth(funcOp, 0);
      results.emplace_back(funcOp, depth);
    });

    for (auto &[funcOp, depth] : results) {
      funcOp->setAttr("max_nesting_depth", builder.getI32IntegerAttr(depth));
      llvm::outs() << "Function '" << funcOp.getName()
                   << "': max nesting depth = " << depth << '\n';
    }
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MaxNestingDepthPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MaxNestingDepthPass)

mlir::PassPluginLibraryInfo getMaxNestingDepthPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaxNestingDepthPass", "1.0",
          []() { mlir::PassRegistration<MaxNestingDepthPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getMaxNestingDepthPassPluginInfo();
}