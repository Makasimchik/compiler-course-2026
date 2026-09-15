#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace {

struct LoopTripInfo {
  affine::AffineForOp loop;
  int64_t tripCount;
};

class ShkrebkoTripCountPass
    : public PassWrapper<ShkrebkoTripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "shkrebko-trip-count"; }

  StringRef getDescription() const final {
    return "Annotate affine.for loops with constant 'trip_count' attribute";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    Builder builder(module.getContext());

    SmallVector<affine::AffineForOp> candidateLoops;
    module.walk([&](affine::AffineForOp forOp) {
      if (!forOp->hasAttr("trip_count"))
        candidateLoops.push_back(forOp);
    });

    SmallVector<LoopTripInfo> computed;
    for (affine::AffineForOp forOp : candidateLoops) {
      std::optional<int64_t> tc = computeTripCountDifferent(forOp);
      if (tc.has_value()) {
        computed.push_back({forOp, tc.value()});
      }
    }

    for (const auto &info : computed) {
      info.loop->setAttr("trip_count",
                         builder.getI64IntegerAttr(info.tripCount));
    }
  }

private:
  std::optional<int64_t>
  computeTripCountDifferent(affine::AffineForOp forOp) const {
    int64_t step = forOp.getStepAsInt();
    if (step <= 0)
      return std::nullopt;

    if (!forOp.hasConstantLowerBound() || !forOp.hasConstantUpperBound())
      return std::nullopt;

    int64_t lb = forOp.getConstantLowerBound();
    int64_t ub = forOp.getConstantUpperBound();

    if (lb >= ub)
      return 0;

    int64_t distance = ub - lb;
    int64_t iterations = ceilDiv(distance, step);
    return iterations;
  }

  static int64_t ceilDiv(int64_t numerator, int64_t denominator) {
    return (numerator + denominator - 1) / denominator;
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ShkrebkoTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ShkrebkoTripCountPass)

mlir::PassPluginLibraryInfo getShkrebkoTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ShkrebkoTripCountPass", "1.0",
          []() { mlir::PassRegistration<ShkrebkoTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getShkrebkoTripCountPassPluginInfo();
}