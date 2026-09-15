#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

class MaslovaConditionTracer
    : public PassWrapper<MaslovaConditionTracer, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MaslovaConditionTracer)

  StringRef getArgument() const final { return "maslova-condition-tracer"; }
  StringRef getDescription() const final {
    return "Inserts trace calls for scf.if and affine.if";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    OpBuilder builder(module.getContext());

    auto ensureDecl = [&](StringRef name) {
      if (module.lookupSymbol<func::FuncOp>(name))
        return;
      builder.setInsertionPointToEnd(module.getBody());
      builder
          .create<func::FuncOp>(module.getLoc(), name,
                                builder.getFunctionType({}, {}))
          .setPrivate();
    };

    ensureDecl("trace_condition_then_begin");
    ensureDecl("trace_condition_then_end");
    ensureDecl("trace_condition_else_begin");
    ensureDecl("trace_condition_else_end");

    module.walk([&](Operation *op) {
      if (auto scfIf = dyn_cast<scf::IfOp>(op)) {
        instrumentRegion(scfIf.getThenRegion(), "then");
        if (!scfIf.getElseRegion().empty())
          instrumentRegion(scfIf.getElseRegion(), "else");
      } else if (auto affineIf = dyn_cast<affine::AffineIfOp>(op)) {
        instrumentRegion(affineIf.getThenRegion(), "then");
        if (!affineIf.getElseRegion().empty())
          instrumentRegion(affineIf.getElseRegion(), "else");
      }
    });
  }

private:
  void instrumentRegion(Region &region, StringRef side) {
    if (region.empty())
      return;

    Block &block = region.front();
    OpBuilder builder(&block, block.begin());
    Location loc = block.front().getLoc();

    std::string beginFn = ("trace_condition_" + side + "_begin").str();
    std::string endFn = ("trace_condition_" + side + "_end").str();

    builder.create<func::CallOp>(loc, beginFn, TypeRange{});

    Operation *terminator = block.getTerminator();
    builder.setInsertionPoint(terminator);
    builder.create<func::CallOp>(loc, endFn, TypeRange{});
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MaslovaConditionTracer)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MaslovaConditionTracer)

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaslovaConditionTracer", "1.0",
          []() { mlir::PassRegistration<MaslovaConditionTracer>(); }};
}
