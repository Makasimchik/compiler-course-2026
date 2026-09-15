#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/StringMap.h"

using namespace mlir;

namespace {

class CallCounterPass
    : public PassWrapper<CallCounterPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "call-counter"; }
  StringRef getDescription() const final {
    return "Counts how many times each function is called";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    llvm::StringMap<int64_t> callCounts;

    module.walk([&](func::CallOp callOp) {
      StringRef funcName = callOp.getCallee();
      callCounts[funcName]++;
    });

    module.walk([&](func::FuncOp func) {
      StringRef funcName = func.getName();
      int64_t count = callCounts[funcName];
      auto callCountAttr =
          IntegerAttr::get(IntegerType::get(func.getContext(), 64), count);
      func->setAttr("call_count", callCountAttr);
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(CallCounterPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(CallCounterPass)

mlir::PassPluginLibraryInfo getCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "CallCounterPass", "1.0",
          []() { mlir::PassRegistration<CallCounterPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getCallCounterPassPluginInfo();
}
