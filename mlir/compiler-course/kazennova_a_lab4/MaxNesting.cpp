#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class MaxNestingDepthPass
    : public PassWrapper<MaxNestingDepthPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "max-nesting-depth"; }
  StringRef getDescription() const final {
    return "Compute max nesting depth of scf/affine operations and attach as "
           "attribute";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](func::FuncOp func) {
      int maxDepth = 0;
      std::function<void(Operation *, int)> walkOp = [&](Operation *op,
                                                         int depth) {
        if (isa<scf::ForOp, scf::IfOp, scf::WhileOp, scf::ParallelOp,
                affine::AffineForOp, affine::AffineIfOp>(op)) {
          depth++;
          if (depth > maxDepth)
            maxDepth = depth;
        }
        for (Region &region : op->getRegions()) {
          for (Block &block : region) {
            for (Operation &nestedOp : block) {
              walkOp(&nestedOp, depth);
            }
          }
        }
      };
      walkOp(func, 0);

      auto depthAttr =
          StringAttr::get(func.getContext(), std::to_string(maxDepth));
      func->setAttr("max_nesting_depth", depthAttr);
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MaxNestingDepthPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MaxNestingDepthPass)

static mlir::PassRegistration<MaxNestingDepthPass> pass;

mlir::PassPluginLibraryInfo getMaxNestingDepthPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaxNestingDepthPass", "1.0",
          []() { mlir::PassRegistration<MaxNestingDepthPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getMaxNestingDepthPluginInfo();
}