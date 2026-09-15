#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class ExamplePass : public PassWrapper<ExamplePass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "yurkin-condition-tracer"; }
  StringRef getDescription() const final {
    return "Insert trace calls for condition blocks";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = module.getContext();
    OpBuilder moduleBuilder(ctx);
    moduleBuilder.setInsertionPointToStart(&module.getBodyRegion().front());
    auto getOrCreateFunc = [&](StringRef name) -> func::FuncOp {
      if (auto f = module.lookupSymbol<func::FuncOp>(name))
        return f;
      OpBuilder::InsertionGuard g(moduleBuilder);
      moduleBuilder.setInsertionPointToStart(&module.getBodyRegion().front());
      FunctionType funcType = FunctionType::get(ctx, {}, {});
      return moduleBuilder.create<func::FuncOp>(module.getLoc(), name,
                                                funcType);
    };
    for (auto ifOp : module.getOps<scf::IfOp>()) {
      Location loc = ifOp.getLoc();
      Region &thenReg = ifOp.getThenRegion();
      if (!thenReg.empty()) {
        Block &thenBlock = thenReg.front();
        OpBuilder b(&thenBlock, thenBlock.begin());
        func::FuncOp fbegin = getOrCreateFunc("trace_condition_then_begin");
        b.create<func::CallOp>(loc, fbegin.getName(), ArrayRef<Type>{},
                               ArrayRef<Value>{});
        Operation *term = thenBlock.getTerminator();
        OpBuilder b2(ctx);
        b2.setInsertionPoint(term);
        func::FuncOp fend = getOrCreateFunc("trace_condition_then_end");
        b2.create<func::CallOp>(loc, fend.getName(), ArrayRef<Type>{},
                                ArrayRef<Value>{});
      }
      Region &elseReg = ifOp.getElseRegion();
      if (!elseReg.empty()) {
        Block &elseBlock = elseReg.front();
        OpBuilder b(&elseBlock, elseBlock.begin());
        func::FuncOp febegin = getOrCreateFunc("trace_condition_else_begin");
        b.create<func::CallOp>(loc, febegin.getName(), ArrayRef<Type>{},
                               ArrayRef<Value>{});
        Operation *term = elseBlock.getTerminator();
        OpBuilder b2(ctx);
        b2.setInsertionPoint(term);
        func::FuncOp feend = getOrCreateFunc("trace_condition_else_end");
        b2.create<func::CallOp>(loc, feend.getName(), ArrayRef<Type>{},
                                ArrayRef<Value>{});
      }
    }
    for (auto ifOp : module.getOps<mlir::affine::AffineIfOp>()) {
      Location loc = ifOp.getLoc();
      Region &thenReg = ifOp.getThenRegion();
      if (!thenReg.empty()) {
        Block &thenBlock = thenReg.front();
        OpBuilder b(&thenBlock, thenBlock.begin());
        func::FuncOp fbegin = getOrCreateFunc("trace_condition_then_begin");
        b.create<func::CallOp>(loc, fbegin.getName(), ArrayRef<Type>{},
                               ArrayRef<Value>{});
        Operation *term = thenBlock.getTerminator();
        OpBuilder b2(ctx);
        b2.setInsertionPoint(term);
        func::FuncOp fend = getOrCreateFunc("trace_condition_then_end");
        b2.create<func::CallOp>(loc, fend.getName(), ArrayRef<Type>{},
                                ArrayRef<Value>{});
      }
      Region &elseReg = ifOp.getElseRegion();
      if (!elseReg.empty()) {
        Block &elseBlock = elseReg.front();
        OpBuilder b(&elseBlock, elseBlock.begin());
        func::FuncOp febegin = getOrCreateFunc("trace_condition_else_begin");
        b.create<func::CallOp>(loc, febegin.getName(), ArrayRef<Type>{},
                               ArrayRef<Value>{});
        Operation *term = elseBlock.getTerminator();
        OpBuilder b2(ctx);
        b2.setInsertionPoint(term);
        func::FuncOp feend = getOrCreateFunc("trace_condition_else_end");
        b2.create<func::CallOp>(loc, feend.getName(), ArrayRef<Type>{},
                                ArrayRef<Value>{});
      }
    }
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ExamplePass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ExamplePass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "yurkin-condition-tracer", "1.0",
          []() { mlir::PassRegistration<ExamplePass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
