#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
class AffineTripCountPass
    : public PassWrapper<AffineTripCountPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AffineTripCountPass)

  StringRef getArgument() const final { return "affine-annotate-trip-count"; }
  StringRef getDescription() const final {
    return "Annotate affine.for loops with trip_count attribute";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    OpBuilder builder(&getContext());

    module.walk([&](affine::AffineForOp forOp) {
      auto tripCount = affine::getConstantTripCount(forOp);
      if (tripCount.has_value()) {
        forOp->setAttr("trip_count",
                       builder.getI64IntegerAttr(tripCount.value()));
      }
    });
  }
};
} // namespace

mlir::PassPluginLibraryInfo getAffineTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "AffineTripCountPass", "1.0",
          []() { mlir::PassRegistration<AffineTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getAffineTripCountPassPluginInfo();
}