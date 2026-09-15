#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace mlir;

namespace {
class AnnotateTripCountPass
    : public PassWrapper<AnnotateTripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "nikitin_a_lab4_MLIR"; }
  StringRef getDescription() const final {
    return "Annotate affine.for loops with trip_count attribute representing "
           "the number of iterations";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    moduleOp.walk([&](affine::AffineForOp forOp) {
      // Check if bounds are constant
      if (!forOp.hasConstantLowerBound() || !forOp.hasConstantUpperBound()) {
        return;
      }

      int64_t lowerBound = forOp.getConstantLowerBound();
      int64_t upperBound = forOp.getConstantUpperBound();
      int64_t step = forOp.getStepAsInt();

      // Calculate trip count
      int64_t range = upperBound - lowerBound;
      int64_t tripCount;

      if (range <= 0) {
        tripCount = 0;
      } else {
        tripCount = (range + step - 1) / step;
      }

      // Add attribute to the operation
      forOp->setAttr("trip_count",
                     IntegerAttr::get(IntegerType::get(forOp.getContext(), 64),
                                      tripCount));
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(AnnotateTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(AnnotateTripCountPass)

mlir::PassPluginLibraryInfo getAnnotateTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "nikitin_a_lab4_MLIR", "1.0",
          []() { mlir::PassRegistration<AnnotateTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getAnnotateTripCountPassPluginInfo();
}