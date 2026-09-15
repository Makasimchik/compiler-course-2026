#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

#include <cstdint>
#include <optional>

using namespace mlir;

namespace {

class TelnovTripCountPass
    : public PassWrapper<TelnovTripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "telnov-trip-count"; }

  StringRef getDescription() const final {
    return "Attach trip_count attribute to affine.for loops with known bounds";
  }

  void runOnOperation() override {
    Builder builder(getOperation().getContext());

    getOperation().walk([&](affine::AffineForOp loop) {
      if (loop->hasAttr("trip_count"))
        return;

      std::optional<int64_t> count = calculateTripCount(loop);
      if (!count.has_value())
        return;

      loop->setAttr("trip_count", builder.getI64IntegerAttr(*count));
    });
  }

private:
  std::optional<int64_t> calculateTripCount(affine::AffineForOp loop) const {
    if (!loop.getLowerBoundOperands().empty() ||
        !loop.getUpperBoundOperands().empty())
      return std::nullopt;

    std::optional<int64_t> lower = loop.getConstantLowerBound();
    std::optional<int64_t> upper = loop.getConstantUpperBound();

    if (!lower.has_value() || !upper.has_value())
      return std::nullopt;

    int64_t step = loop.getStep().getSExtValue();
    if (step <= 0)
      return std::nullopt;

    int64_t distance = *upper - *lower;
    if (distance <= 0)
      return 0;

    int64_t iterations = distance / step;
    if (distance % step != 0)
      ++iterations;

    return iterations;
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TelnovTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TelnovTripCountPass)

mlir::PassPluginLibraryInfo getTelnovTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TelnovTripCountPass", "1.0",
          []() { mlir::PassRegistration<TelnovTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTelnovTripCountPassPluginInfo();
}