#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "sannikov-nesting-depth"

using namespace mlir;

namespace {

struct SannikovNestingDepthPass
    : public PassWrapper<SannikovNestingDepthPass, OperationPass<ModuleOp>> {

  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(SannikovNestingDepthPass)

  StringRef getArgument() const final { return "sannikov-nesting-depth"; }

  StringRef getDescription() const final {
    return "Computes max nesting depth of scf/affine block ops "
           "and attaches the result as a function attribute";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<scf::SCFDialect, affine::AffineDialect>();
  }

  bool isNestingOp(Operation *op) const {
    return isa<scf::ForOp, scf::IfOp, scf::WhileOp, scf::ForallOp,
               affine::AffineForOp, affine::AffineIfOp,
               affine::AffineParallelOp>(op);
  }

  int depthOf(Operation *op) const {
    int level = 0;
    Operation *oper = op->getParentOp();
    while (oper && !isa<func::FuncOp>(oper)) {
      if (isNestingOp(oper))
        level++;
      oper = oper->getParentOp();
    }
    return level + 1;
  }

  int computeMaxDepth(func::FuncOp funcOp) const {
    int maxLevel = 0;
    funcOp.walk([&](Operation *op) -> WalkResult {
      if (isNestingOp(op))
        maxLevel = std::max(maxLevel, depthOf(op));
      return WalkResult::advance();
    });
    return maxLevel;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    OpBuilder builder(module.getContext());
    module.walk([&](func::FuncOp funcOp) {
      int maxLevel = computeMaxDepth(funcOp);

      funcOp->setAttr("sannikov_nesting_depth",
                      builder.getI64IntegerAttr(maxLevel));

      LLVM_DEBUG(llvm::dbgs() << "[sannikov-nesting-depth] " << funcOp.getName()
                              << " - " << maxLevel << "\n");
    });
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "SannikovNestingDepthPass", "v0.1",
          []() { PassRegistration<SannikovNestingDepthPass>(); }};
}