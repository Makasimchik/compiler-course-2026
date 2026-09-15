#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class TripCountPass
    : public PassWrapper<TripCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "levonychev_MLIR"; }
  StringRef getDescription() const final { return "TripCountPass"; }

  void runOnOperation() override {

    ModuleOp f = getOperation();

    f.walk([&](affine::AffineForOp op) {
      if (op.hasConstantBounds()) {
        int64_t lowerBound = op.getConstantLowerBound();
        int64_t upperBound = op.getConstantUpperBound();
        int64_t step = op.getStep().getSExtValue();
        int64_t tripCount = 0;
        if (upperBound > lowerBound) {
          tripCount = (upperBound - lowerBound + step - 1) / step;
        }
        MLIRContext *context = op.getContext();
        auto attribute = IntegerAttr::get(IndexType::get(context), tripCount);
        op->setAttr("trip_count", attribute);
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TripCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TripCountPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "LevonychevTripCountPass", "1.0",
          []() { mlir::PassRegistration<TripCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
