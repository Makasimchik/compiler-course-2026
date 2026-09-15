#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;
using namespace mlir::affine;

namespace {
class TripCountAnnotationPass
    : public PassWrapper<TripCountAnnotationPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "TripCountAnnotationPass_MLIR"; }
  StringRef getDescription() const final {
    return "Annotate affine.for loops with an attribute \"trip_count\" that "
           "represents amount of iteration. If trip_count is unknown then no "
           "attribute is attached";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    MLIRContext *context = &getContext();
    OpBuilder builder(context);

    moduleOp.walk([&](AffineForOp forOp) {
      auto tripCount = getConstantTripCount(forOp);

      if (tripCount.has_value()) {
        forOp->setAttr("trip_count",
                       builder.getI64IntegerAttr(tripCount.value()));
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TripCountAnnotationPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TripCountAnnotationPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TripCountAnnotationPass", "1.0",
          []() { mlir::PassRegistration<TripCountAnnotationPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
