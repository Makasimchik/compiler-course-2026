#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace mlir;
using namespace mlir::affine;

namespace {

class AnnotateTripCountPass
    : public PassWrapper<AnnotateTripCountPass, OperationPass<mlir::ModuleOp>> {
public:
  StringRef getArgument() const final { return "annotate-trip-count"; }
  StringRef getDescription() const final {
    return "Annotate affine.for loops with trip_count attribute";
  }

  void runOnOperation() override {
    mlir::ModuleOp moduleOp = getOperation();

    moduleOp.walk([&](AffineForOp forOp) {
      if (!forOp.hasConstantLowerBound() || !forOp.hasConstantUpperBound())
        return;

      int64_t lb = forOp.getConstantLowerBound();
      int64_t ub = forOp.getConstantUpperBound();
      int64_t step = forOp.getStep().getSExtValue();

      if (step == 0)
        return;

      uint64_t tripCount = 0;
      if (step > 0 && ub > lb) {
        tripCount = (ub - lb + step - 1) / step;
      } else if (step < 0 && lb > ub) {
        step = -step;
        tripCount = (lb - ub + step - 1) / step;
      } else {
        return;
      }

      IntegerAttr attr =
          IntegerAttr::get(IntegerType::get(forOp.getContext(), 64), tripCount);
      forOp->setAttr("trip_count", attr);
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(AnnotateTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(AnnotateTripCountPass)

mlir::PassPluginLibraryInfo getAnnotateTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "AnnotateTripCountPass", "1.0",
          []() { mlir::PassRegistration<AnnotateTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getAnnotateTripCountPassPluginInfo();
}
