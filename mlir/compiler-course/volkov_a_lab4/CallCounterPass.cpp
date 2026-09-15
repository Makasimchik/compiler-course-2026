#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

struct VolkovCallCounterPass
    : public PassWrapper<VolkovCallCounterPass, OperationPass<ModuleOp>> {

  StringRef getArgument() const final { return "volkov-call-counter"; }

  StringRef getDescription() const final {
    return "Counts how many times each function is called by other functions "
           "in the module and attaches the result as a 'call_count' attribute.";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    llvm::StringMap<int32_t> callFrequency;
    module.walk(
        [&](func::CallOp callOp) { callFrequency[callOp.getCallee()]++; });

    module.walk([&](func::FuncOp funcOp) {
      int32_t count = callFrequency.lookup(funcOp.getName());
      auto i32 = IntegerType::get(funcOp.getContext(), 32);
      funcOp->setAttr("call_count", IntegerAttr::get(i32, count));
    });

    size_t totalOps = 0;
    module.walk([&](Operation *) { ++totalOps; });
    llvm::outs() << "Total operations in module: " << totalOps << '\n';
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(VolkovCallCounterPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(VolkovCallCounterPass)

mlir::PassPluginLibraryInfo getVolkovCallCounterPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "VolkovCallCounterPass", "v1.0",
          []() { mlir::PassRegistration<VolkovCallCounterPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getVolkovCallCounterPluginInfo();
}