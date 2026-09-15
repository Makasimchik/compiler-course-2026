#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class AffineTripCountPass
    : public PassWrapper<AffineTripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "affine-trip-count"; }
  StringRef getDescription() const final {
    return "Annotates affine.for with trip_count attribute";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(moduleOp.getContext());

    moduleOp.walk([&](affine::AffineForOp forOp) {
      std::optional<uint64_t> tripCount = affine::getConstantTripCount(forOp);

      if (tripCount.has_value()) {
        IntegerAttr attrValue = builder.getI64IntegerAttr(tripCount.value());
        forOp->setAttr("trip_count", attrValue);
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(AffineTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(AffineTripCountPass)

mlir::PassPluginLibraryInfo getAffineTripCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "AffineTripCountPass", "1.0",
          []() { mlir::PassRegistration<AffineTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getAffineTripCountPassPluginInfo();
}
