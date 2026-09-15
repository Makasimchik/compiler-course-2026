#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/APInt.h"

using namespace mlir;

namespace {
class AnnotateAffineTripCountPass
    : public PassWrapper<AnnotateAffineTripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "annotate-affine-trip-count"; }
  StringRef getDescription() const final {
    return "Attach a trip_count attribute to affine.for loops with known "
           "iteration counts";
  }

  void runOnOperation() override {
    MLIRContext *context = &getContext();
    auto tripCountType = IntegerType::get(context, 64);

    getOperation().walk([&](affine::AffineForOp forOp) {
      std::optional<uint64_t> tripCount = affine::getConstantTripCount(forOp);
      if (!tripCount) {
        forOp->removeAttr("trip_count");
        return;
      }

      forOp->setAttr("trip_count",
                     IntegerAttr::get(tripCountType, APInt(64, *tripCount)));
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(AnnotateAffineTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(AnnotateAffineTripCountPass)

mlir::PassPluginLibraryInfo getAnnotateAffineTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "AnnotateAffineTripCountPass", "1.0",
          []() { mlir::PassRegistration<AnnotateAffineTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getAnnotateAffineTripCountPassPluginInfo();
}
