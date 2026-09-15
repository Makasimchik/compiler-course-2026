#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class FuncCallsCounter
    : public PassWrapper<FuncCallsCounter, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "func-calls-counter"; }
  StringRef getDescription() const final { return "Description pass"; }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    DenseMap<StringRef, int> funcCallCounts;
    SymbolTable symbTable(moduleOp);

    for (mlir::func::FuncOp func : moduleOp.getOps<mlir::func::FuncOp>()) {
      StringRef funcName = func.getName();
      funcCallCounts[funcName] = 0;
    }

    for (mlir::func::FuncOp func : moduleOp.getOps<mlir::func::FuncOp>()) {
      StringRef funcName = func.getName();

      func.walk([&](func::CallOp callOp) {
        StringRef calledFuncName = callOp.getCallee();
        if (calledFuncName != funcName &&
            symbTable.lookup<mlir::func::FuncOp>(calledFuncName))
          funcCallCounts[calledFuncName]++;
      });
    }

    for (auto &funcStat : funcCallCounts) {
      if (mlir::func::FuncOp func =
              symbTable.lookup<mlir::func::FuncOp>(funcStat.first)) {
        func->setAttr("call_count",
                      IntegerAttr::get(IntegerType::get(func.getContext(), 32),
                                       funcStat.second));
      }
    }
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(FuncCallsCounter)
MLIR_DEFINE_EXPLICIT_TYPE_ID(FuncCallsCounter)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "FuncCallsCounter", "1.0",
          []() { mlir::PassRegistration<FuncCallsCounter>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
