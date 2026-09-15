#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
int computeMaxDepth(Region &region, int currentDepth) {
  int maxDepth = currentDepth;
  for (Operation &op : region.getOps()) {
    int newDepth = currentDepth;
    if (isa<scf::IfOp, scf::ForOp, scf::WhileOp, affine::AffineForOp,
            affine::AffineIfOp>(&op)) {
      newDepth = currentDepth + 1;
      if (newDepth > maxDepth)
        maxDepth = newDepth;
    }
    for (Region &subRegion : op.getRegions()) {
      int subDepth = computeMaxDepth(subRegion, newDepth);
      if (subDepth > maxDepth)
        maxDepth = subDepth;
    }
  }
  return maxDepth;
}

class FedoseevPass4
    : public PassWrapper<FedoseevPass4, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "FedoseevPass4"; }
  StringRef getDescription() const final {
    return "Attach max depth of scf/affine if/for/while blocks to each "
           "function";
  }
  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    for (func::FuncOp funcOp : moduleOp.getOps<func::FuncOp>()) {
      int maxDepth = computeMaxDepth(funcOp.getBody(), 0);
      IntegerAttr depthAttr = IntegerAttr::get(
          IntegerType::get(moduleOp.getContext(), 32), maxDepth);
      funcOp->setAttr("max_depth", depthAttr);
    }
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(FedoseevPass4)
MLIR_DEFINE_EXPLICIT_TYPE_ID(FedoseevPass4)

mlir::PassPluginLibraryInfo getFedoseevPass4PluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "FedoseevPass4", "1.0",
          []() { mlir::PassRegistration<FedoseevPass4>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFedoseevPass4PluginInfo();
}