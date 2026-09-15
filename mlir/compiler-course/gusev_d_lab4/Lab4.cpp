#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
class GusevDLab4Pass
    : public PassWrapper<GusevDLab4Pass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "gusev-d-lab4"; }
  StringRef getDescription() const final {
    return "Insert trace calls at the beginning and end of condition blocks";
  }

  void getDependentDialects(DialectRegistry &Registry) const override {
    Registry
        .insert<affine::AffineDialect, func::FuncDialect, scf::SCFDialect>();
  }

  void runOnOperation() override {
    ModuleOp Module = getOperation();

    ensureTraceFunction(Module, "trace_condition_then_begin");
    ensureTraceFunction(Module, "trace_condition_then_end");
    ensureTraceFunction(Module, "trace_condition_else_begin");
    ensureTraceFunction(Module, "trace_condition_else_end");

    Module.walk([&](scf::IfOp IfOp) {
      instrumentRegion(IfOp.getThenRegion(), "trace_condition_then_begin",
                       "trace_condition_then_end");
      if (!IfOp.getElseRegion().empty()) {
        instrumentRegion(IfOp.getElseRegion(), "trace_condition_else_begin",
                         "trace_condition_else_end");
      }
    });

    Module.walk([&](affine::AffineIfOp IfOp) {
      instrumentRegion(IfOp.getThenRegion(), "trace_condition_then_begin",
                       "trace_condition_then_end");
      if (!IfOp.getElseRegion().empty()) {
        instrumentRegion(IfOp.getElseRegion(), "trace_condition_else_begin",
                         "trace_condition_else_end");
      }
    });
  }

private:
  static void ensureTraceFunction(ModuleOp Module, StringRef Name) {
    if (Module.lookupSymbol<func::FuncOp>(Name)) {
      return;
    }

    OpBuilder Builder(Module.getContext());
    Builder.setInsertionPointToStart(Module.getBody());

    FunctionType Type = Builder.getFunctionType({}, {});
    auto Function = func::FuncOp::create(Module.getLoc(), Name, Type);
    Function.setVisibility(SymbolTable::Visibility::Private);
    Builder.insert(Function);
  }

  static void instrumentRegion(Region &Region, StringRef BeginName,
                               StringRef EndName) {
    if (Region.empty()) {
      return;
    }

    Block &Block = Region.front();
    OpBuilder Builder(Region.getContext());

    Builder.setInsertionPointToStart(&Block);
    Builder.create<func::CallOp>(Block.front().getLoc(), BeginName, TypeRange{},
                                 ValueRange{});

    Operation *Terminator = Block.getTerminator();
    Builder.setInsertionPoint(Terminator);
    Builder.create<func::CallOp>(Terminator->getLoc(), EndName, TypeRange{},
                                 ValueRange{});
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(GusevDLab4Pass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(GusevDLab4Pass)

mlir::PassPluginLibraryInfo getGusevDLab4PassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "GusevDLab4Pass", "1.0",
          []() { mlir::PassRegistration<GusevDLab4Pass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getGusevDLab4PassPluginInfo();
}
