#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
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
  StringRef getArgument() const final { return "AffineTripCount_MLIR"; }
  StringRef getDescription() const final { return "Description pass"; }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(moduleOp);

    moduleOp.walk([&](affine::AffineForOp op) {
      const auto &upperMap = op.getUpperBoundMap();
      const auto &lowerMap = op.getLowerBoundMap();
      if (upperMap.isSingleConstant() && lowerMap.isSingleConstant()) {
        int64_t upperBound = upperMap.getSingleConstantResult();
        int64_t lowerBound = lowerMap.getSingleConstantResult();
        int64_t step = op.getStep().getSExtValue();

        int64_t tripCount =
            std::max(0l, (upperBound - lowerBound + step - 1) / step);
        op->setAttr("trip_count", builder.getI64IntegerAttr(tripCount));
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(AffineTripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(AffineTripCountPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "AffineTripCountPass", "1.0",
          []() { mlir::PassRegistration<AffineTripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
