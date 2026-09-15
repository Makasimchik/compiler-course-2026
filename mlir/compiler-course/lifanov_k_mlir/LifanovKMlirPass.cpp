#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

func::FuncOp getOrInsertFunc(ModuleOp module, StringRef name) {
  auto ctx = module.getContext();

  if (auto func = module.lookupSymbol<func::FuncOp>(name))
    return func;

  auto funcTy = FunctionType::get(ctx, {}, {});

  OpBuilder builder(module.getBodyRegion());
  builder.setInsertionPointToStart(module.getBody());
  auto func = builder.create<func::FuncOp>(module.getLoc(), name, funcTy);

  func.setPrivate();
  return func;
}

void instrumentBlock(Block &block, Location loc, func::FuncOp funcBegin,
                     func::FuncOp funcEnd) {
  OpBuilder builder(block.getParentOp()->getContext());

  builder.setInsertionPointToStart(&block);
  builder.create<func::CallOp>(loc, funcBegin, ValueRange{});

  if (auto *terminator = block.getTerminator()) {
    builder.setInsertionPoint(terminator);
    builder.create<func::CallOp>(loc, funcEnd, ValueRange{});
  }
}

class LifanovKPass : public PassWrapper<LifanovKPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "lifanovk_MLIR"; }
  StringRef getDescription() const final {
    return "Trace condition blocks using standard func.call";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<func::FuncDialect, scf::SCFDialect, affine::AffineDialect>();
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    auto thenBegin = getOrInsertFunc(moduleOp, "trace_condition_then_begin");
    auto thenEnd = getOrInsertFunc(moduleOp, "trace_condition_then_end");
    auto elseBegin = getOrInsertFunc(moduleOp, "trace_condition_else_begin");
    auto elseEnd = getOrInsertFunc(moduleOp, "trace_condition_else_end");

    moduleOp.walk([&](Operation *op) {
      if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
        instrumentBlock(ifOp.getThenRegion().front(), op->getLoc(), thenBegin,
                        thenEnd);
        if (!ifOp.getElseRegion().empty()) {
          instrumentBlock(ifOp.getElseRegion().front(), op->getLoc(), elseBegin,
                          elseEnd);
        }
      } else if (auto affineIfOp = dyn_cast<affine::AffineIfOp>(op)) {
        instrumentBlock(affineIfOp.getThenRegion().front(), op->getLoc(),
                        thenBegin, thenEnd);
        if (!affineIfOp.getElseRegion().empty()) {
          instrumentBlock(affineIfOp.getElseRegion().front(), op->getLoc(),
                          elseBegin, elseEnd);
        }
      }
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(LifanovKPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(LifanovKPass)

mlir::PassPluginLibraryInfo getLifanovKPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "LifanovKPass", "1.0",
          []() { mlir::PassRegistration<LifanovKPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getLifanovKPassPluginInfo();
}