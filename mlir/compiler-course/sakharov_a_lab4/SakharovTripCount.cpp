#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include <cstdint>
#include <optional>

using namespace mlir;

namespace {

class SakharovTripCountPass
    : public PassWrapper<SakharovTripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "sakharov-trip-count"; }

  StringRef getDescription() const final {
    return "Annotates affine.for loops with a constant trip_count attribute";
  }

  void runOnOperation() override {
    Builder builder(&getContext());

    getOperation().walk([&](affine::AffineForOp forOp) {
      if (forOp->hasAttr("trip_count"))
        return;

      std::optional<uint64_t> tripCount = affine::getConstantTripCount(forOp);
      if (!tripCount)
        return;

      forOp->setAttr("trip_count", builder.getI64IntegerAttr(
                                       static_cast<int64_t>(*tripCount)));
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(SakharovTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(SakharovTripCountPass)

mlir::PassPluginLibraryInfo getSakharovTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "SakharovTripCount", "1.0",
          []() { mlir::PassRegistration<SakharovTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getSakharovTripCountPassPluginInfo();
}
