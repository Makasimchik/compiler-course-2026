#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
class ConditionTracingPass
    : public PassWrapper<ConditionTracingPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "trace-conditions"; }
  StringRef getDescription() const final {
    return "Adds tracing calls to condition branches (ashihmin_d_lab4)";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, affine::AffineDialect, func::FuncDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    auto *ctx = module.getContext();

    SmallVector<StringRef, 4> funcNames = {
        "trace_condition_then_begin", "trace_condition_then_end",
        "trace_condition_else_begin", "trace_condition_else_end"};

    OpBuilder modBuilder(module.getBodyRegion());
    for (auto name : funcNames) {
      if (!module.lookupSymbol<func::FuncOp>(name)) {
        modBuilder.setInsertionPointToStart(module.getBody());
        auto type = FunctionType::get(ctx, {}, {});
        modBuilder.create<func::FuncOp>(module.getLoc(), name, type)
            .setPrivate();
      }
    }

    module.walk([&](Operation *op) {
      auto processRegion = [&](Region &region, StringRef startNm,
                               StringRef endNm) {
        if (region.empty())
          return;

        Block &block = region.front();
        OpBuilder builder(ctx);

        builder.setInsertionPointToStart(&block);
        builder.create<func::CallOp>(op->getLoc(), startNm, TypeRange{});

        Operation *term = block.getTerminator();
        builder.setInsertionPoint(term);
        builder.create<func::CallOp>(op->getLoc(), endNm, TypeRange{});
      };

      if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
        processRegion(ifOp.getThenRegion(), "trace_condition_then_begin",
                      "trace_condition_then_end");
        if (!ifOp.getElseRegion().empty())
          processRegion(ifOp.getElseRegion(), "trace_condition_else_begin",
                        "trace_condition_else_end");
      } else if (auto affIf = dyn_cast<affine::AffineIfOp>(op)) {
        processRegion(affIf.getThenRegion(), "trace_condition_then_begin",
                      "trace_condition_then_end");
        if (!affIf.getElseRegion().empty())
          processRegion(affIf.getElseRegion(), "trace_condition_else_begin",
                        "trace_condition_else_end");
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ConditionTracingPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ConditionTracingPass)

mlir::PassPluginLibraryInfo getConditionTracingPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ConditionTracingPass", "1.0",
          []() { mlir::PassRegistration<ConditionTracingPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getConditionTracingPassPluginInfo();
}