#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class CallCounterPass
    : public PassWrapper<CallCounterPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "func-call-counter"; }
  StringRef getDescription() const final {
    return "Counts how many times each function is called and attaches the "
           "count as an attribute.";
  }

  void runOnOperation() override {
    ModuleOp mod_op = getOperation();
    OpBuilder builder(mod_op.getContext());

    llvm::StringMap<unsigned> count_map;

    // рекурсивный проход по вызовам
    mod_op.walk([&](func::CallOp call_op) {
      StringRef callee_nm = call_op.getCallee();
      count_map[callee_nm]++;
    });

    // рекурсивный проход по определениям + добавление аттрибута
    mod_op.walk([&](func::FuncOp func_op) {
      StringRef func_nm = func_op.getName();

      unsigned count = count_map[func_nm];

      func_op->setAttr("call_count", builder.getI32IntegerAttr(count));
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(CallCounterPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(CallCounterPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "CallCounterPass", "1.0",
          []() { mlir::PassRegistration<CallCounterPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}