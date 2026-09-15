#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class callcountpass_kruglova
    : public PassWrapper<callcountpass_kruglova, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "func-call-counter"; }
  StringRef getDescription() const final {
    return "Counts calls for func.func and adds call_count attribute";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    OpBuilder builder(&getContext());

    llvm::StringMap<int> callCounts;

    // считаем вызовы
    module.walk([&](func::CallOp callOp) {
      StringRef callee = callOp.getCallee();
      callCounts[callee]++;
    });

    // прикрепляем атрибуты к самим функциям
    module.walk([&](func::FuncOp funcOp) {
      StringRef funcName = funcOp.getName();
      int count = callCounts.lookup(funcName);
      funcOp->setAttr("call_count", builder.getI32IntegerAttr(count));
    });

    auto totalOps = 0;
    module.walk([&](Operation *op) { ++totalOps; });
    llvm::outs() << "Count operations: " << totalOps << '\n';
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(callcountpass_kruglova)
MLIR_DEFINE_EXPLICIT_TYPE_ID(callcountpass_kruglova)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "callcountpass_kruglova", "1.0",
          []() { mlir::PassRegistration<callcountpass_kruglova>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}