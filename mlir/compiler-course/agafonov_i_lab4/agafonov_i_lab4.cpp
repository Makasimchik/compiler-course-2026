#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class AddTripCountPass
    : public PassWrapper<AddTripCountPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AddTripCountPass)

  StringRef getArgument() const final { return "add-trip-count"; }
  StringRef getDescription() const final {
    return "Annotate affine.for loops with trip_count attribute";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<affine::AffineDialect>();
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(moduleOp.getContext());

    moduleOp.walk([&](affine::AffineForOp forOp) {
      if (forOp.hasConstantBounds()) {
        int64_t lb = forOp.getConstantLowerBound();
        int64_t ub = forOp.getConstantUpperBound();
        int64_t step = forOp.getStep().getSExtValue();
        int64_t tripCount = 0;
        if (ub > lb) {
          tripCount = (ub - lb + step - 1) / step;
        }

        forOp->setAttr("trip_count", builder.getI64IntegerAttr(tripCount));
      }
    });
  }
};
} // namespace

mlir::PassPluginLibraryInfo getAddTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "AddTripCountPass", "1.0",
          []() { mlir::PassRegistration<AddTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getAddTripCountPassPluginInfo();
}