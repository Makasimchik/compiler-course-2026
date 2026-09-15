#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

func::FuncOp getOrInsertTraceFunc(ModuleOp module, StringRef name) {
  MLIRContext *ctx = module.getContext();
  if (auto existing = module.lookupSymbol<func::FuncOp>(name))
    return existing;

  OpBuilder builder(module.getBodyRegion());
  builder.setInsertionPointToStart(module.getBody());
  auto fn = builder.create<func::FuncOp>(module.getLoc(), name,
                                         FunctionType::get(ctx, {}, {}));
  fn.setPrivate();
  return fn;
}

void instrumentBlock(Block &block, Location loc, func::FuncOp beginFn,
                     func::FuncOp endFn) {
  OpBuilder builder(block.getParentOp()->getContext());
  builder.setInsertionPointToStart(&block);
  builder.create<func::CallOp>(loc, beginFn, ValueRange{});

  if (Operation *term = block.getTerminator()) {
    builder.setInsertionPoint(term);
    builder.create<func::CallOp>(loc, endFn, ValueRange{});
  }
}

class EreminVLab4TraceConditionPass
    : public PassWrapper<EreminVLab4TraceConditionPass,
                         OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "eremin_v_lab_4_MLIR"; }
  StringRef getDescription() const final {
    return "Insert trace_condition_* calls around scf.if / affine.if regions";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<func::FuncDialect, scf::SCFDialect, affine::AffineDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    func::FuncOp thenBegin =
        getOrInsertTraceFunc(module, "trace_condition_then_begin");
    func::FuncOp thenEnd =
        getOrInsertTraceFunc(module, "trace_condition_then_end");
    func::FuncOp elseBegin =
        getOrInsertTraceFunc(module, "trace_condition_else_begin");
    func::FuncOp elseEnd =
        getOrInsertTraceFunc(module, "trace_condition_else_end");

    module.walk([&](Operation *op) {
      if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
        instrumentBlock(ifOp.getThenRegion().front(), ifOp.getLoc(), thenBegin,
                        thenEnd);
        if (!ifOp.getElseRegion().empty())
          instrumentBlock(ifOp.getElseRegion().front(), ifOp.getLoc(),
                          elseBegin, elseEnd);
      } else if (auto aif = dyn_cast<affine::AffineIfOp>(op)) {
        instrumentBlock(aif.getThenRegion().front(), aif.getLoc(), thenBegin,
                        thenEnd);
        if (!aif.getElseRegion().empty())
          instrumentBlock(aif.getElseRegion().front(), aif.getLoc(), elseBegin,
                          elseEnd);
      }
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(EreminVLab4TraceConditionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(EreminVLab4TraceConditionPass)

static mlir::PassPluginLibraryInfo getEreminVLab4PassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "EreminVLab4TraceConditionPass", "1.0",
          []() { mlir::PassRegistration<EreminVLab4TraceConditionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getEreminVLab4PassPluginInfo();
}
