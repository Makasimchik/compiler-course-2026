#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/StringMap.h"

using namespace mlir;

namespace {
class FunctionCallCounterPass
    : public PassWrapper<FunctionCallCounterPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FunctionCallCounterPass)

  StringRef getArgument() const final { return "count-function-calls"; }
  StringRef getDescription() const final {
    return "Counts how many times each function is called and adds it as an "
           "attribute";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    llvm::StringMap<int64_t> callCounts;

    module.walk([&](func::CallOp callOp) {
      StringRef callee = callOp.getCallee();
      callCounts[callee]++;
    });
    module.walk([&](func::FuncOp funcOp) {
      StringRef funcName = funcOp.getName();
      int64_t count = callCounts.lookup(funcName);

      OpBuilder builder(funcOp.getContext());
      funcOp->setAttr("kutergin_call_count", builder.getI64IntegerAttr(count));
    });
  }
};
} // namespace

namespace mlir {
namespace kutergin_lab4 {
void registerFunctionCallCounterPass() {
  PassRegistration<FunctionCallCounterPass>();
}
} // namespace kutergin_lab4
} // namespace mlir

extern "C" LLVM_ATTRIBUTE_WEAK ::mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "FunctionCallCounterPlugin", "1.0",
          []() { ::mlir::kutergin_lab4::registerFunctionCallCounterPass(); }};
}