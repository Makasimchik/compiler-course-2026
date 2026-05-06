#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

class TraceConditionPass
    : public PassWrapper<TraceConditionPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "trace-condition"; }
  StringRef getDescription() const final {
    return "Inserts trace calls at the beginning and end of if-condition "
           "blocks";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](Operation *op) {
      if (auto scfIf = dyn_cast<scf::IfOp>(op)) {
        instrumentBlock(scfIf.thenBlock(), "trace_condition_then_begin",
                        "trace_condition_then_end");
        if (scfIf.elseBlock()) {
          instrumentBlock(scfIf.elseBlock(), "trace_condition_else_begin",
                          "trace_condition_else_end");
        }
      } else if (auto affineIf = dyn_cast<affine::AffineIfOp>(op)) {
        instrumentBlock(affineIf.getThenBlock(), "trace_condition_then_begin",
                        "trace_condition_then_end");
        if (affineIf.getElseBlock()) {
          instrumentBlock(affineIf.getElseBlock(), "trace_condition_else_begin",
                          "trace_condition_else_end");
        }
      }
    });
  }

private:
  void instrumentBlock(Block *block, StringRef beginFuncName,
                       StringRef endFuncName) {
    if (!block)
      return;

    MLIRContext *ctx = block->getParentOp()->getContext();
    OpBuilder builder(ctx);
    Location loc = block->getParentOp()->getLoc();

    ModuleOp module = block->getParentOp()->getParentOfType<ModuleOp>();
    if (!module)
      return;

    FlatSymbolRefAttr beginRef =
        getOrInsertFuncDeclaration(module, beginFuncName);
    FlatSymbolRefAttr endRef = getOrInsertFuncDeclaration(module, endFuncName);

    builder.setInsertionPointToStart(block);
    builder.create<func::CallOp>(loc, beginRef, TypeRange{});

    Operation *terminator = block->getTerminator();
    if (terminator) {
      builder.setInsertionPoint(terminator);
      builder.create<func::CallOp>(loc, endRef, TypeRange{});
    }
  }

  FlatSymbolRefAttr getOrInsertFuncDeclaration(ModuleOp module,
                                               StringRef name) {
    MLIRContext *ctx = module.getContext();
    if (module.lookupSymbol<func::FuncOp>(name))
      return SymbolRefAttr::get(ctx, name);

    // Вставляем декларацию в начало модуля
    OpBuilder builder(module.getBodyRegion());
    builder.setInsertionPointToStart(&module.getBodyRegion().front());
    auto funcType = FunctionType::get(ctx, {}, {});
    builder.create<func::FuncOp>(module.getLoc(), name, funcType).setPrivate();
    return SymbolRefAttr::get(ctx, name);
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TraceConditionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TraceConditionPass)

mlir::PassPluginLibraryInfo getTraceConditionPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TraceConditionPass", "1.0",
          []() { mlir::PassRegistration<TraceConditionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTraceConditionPassPluginInfo();
}