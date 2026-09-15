#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

using namespace mlir;

namespace {
static int getMaxDepth(Operation *op) {
  int maxDepth = 0;
  for (Region &region : op->getRegions()) {
    for (Block &block : region) {
      for (Operation &nested : block) {

        bool isRec = isa<scf::ForOp, scf::IfOp, scf::WhileOp,
                         affine::AffineForOp, affine::AffineIfOp>(&nested);

        int childDepth = getMaxDepth(&nested);
        int addition = isRec ? childDepth + 1 : childDepth;
        maxDepth = std::max(maxDepth, addition);
      }
    }
  }
  return maxDepth;
}

class LuzanEMaxDepthPass
    : public PassWrapper<LuzanEMaxDepthPass, OperationPass<func::FuncOp>> {

  StringRef getArgument() const final { return "luzanemaxdepth"; }
  StringRef getDescription() const final {
    return "A pass that counts the max depth of function blocks and Attaches "
           "the result as an attribute for the function operation";
  }

  void runOnOperation() override {
    func::FuncOp funcOp = getOperation();
    int depth = getMaxDepth(funcOp.getOperation());
    funcOp->setAttr(
        "max_block_depth",
        IntegerAttr::get(IntegerType::get(funcOp.getContext(), 64), depth));
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(LuzanEMaxDepthPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(LuzanEMaxDepthPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "luzanemaxdepth", "42.0",
          []() { mlir::PassRegistration<LuzanEMaxDepthPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
