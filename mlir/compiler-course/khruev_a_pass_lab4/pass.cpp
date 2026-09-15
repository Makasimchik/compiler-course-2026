#include "mlir/Pass/Pass.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/CallInterfaces.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class CallCounterPassKhruev
    : public PassWrapper<CallCounterPassKhruev, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "func_call_counter"; }
  StringRef getDescription() const final {
    return "Counts function calls and attaches count as an attribute.";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(&getContext());

    for (Operation &op : moduleOp.getBody()->getOperations()) {

      if (isa<SymbolOpInterface>(op)) {

        std::optional<SymbolTable::UseRange> uses =
            SymbolTable::getSymbolUses(&op, moduleOp);

        int32_t callCount = 0;
        if (uses.has_value()) {
          for (SymbolTable::SymbolUse use : *uses) {
            if (isa<CallOpInterface>(use.getUser())) {
              callCount++;
            }
          }
        }

        op.setAttr("call_count", builder.getI32IntegerAttr(callCount));
      }
    }
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(CallCounterPassKhruev)
MLIR_DEFINE_EXPLICIT_TYPE_ID(CallCounterPassKhruev)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "CallCounterPassKhruev", "1.0",
          []() { mlir::PassRegistration<CallCounterPassKhruev>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}