#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

static std::optional<int64_t> getConstFromExpr(AffineExpr expr) {
  if (auto constExpr = dyn_cast<AffineConstantExpr>(expr)) {
    return constExpr.getValue();
  }
  return std::nullopt;
}

static std::optional<int64_t> calcTripCount(int64_t lower, int64_t upper,
                                            int64_t step) {
  if (step <= 0)
    return std::nullopt;
  if (upper <= lower)
    return 0;
  int64_t dist = upper - lower;
  return (dist + step - 1) / step;
}

class TripCountPass
    : public PassWrapper<TripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "trip_count"; }

  StringRef getDescription() const final {
    return "Add trip_count attribute to affine.for";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](affine::AffineForOp loop) {
      if (loop->hasAttr("trip_count"))
        return;

      if (!loop.getLowerBoundOperands().empty() ||
          !loop.getUpperBoundOperands().empty()) {
        return;
      }

      AffineExpr lowerExpr = loop.getLowerBoundMap().getResult(0);
      AffineExpr upperExpr = loop.getUpperBoundMap().getResult(0);

      auto lower = getConstFromExpr(lowerExpr);
      auto upper = getConstFromExpr(upperExpr);

      if (!lower.has_value() || !upper.has_value()) {
        return;
      }

      auto stepAttr = loop.getStepAttr();
      if (!stepAttr)
        return;
      int64_t step = stepAttr.getValue().getSExtValue();

      auto count = calcTripCount(*lower, *upper, step);
      if (!count.has_value())
        return;

      auto attr = IntegerAttr::get(IndexType::get(loop.getContext()), *count);
      loop->setAttr("trip_count", attr);
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TripCountPass)

mlir::PassPluginLibraryInfo getTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TripCountPass", "1.0",
          []() { mlir::PassRegistration<TripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTripCountPassPluginInfo();
}