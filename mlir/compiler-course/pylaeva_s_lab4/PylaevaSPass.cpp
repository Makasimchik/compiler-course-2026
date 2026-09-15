#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

class PylaevaSPass : public PassWrapper<PylaevaSPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "pylaeva_s_lab4_MLIR"; }
  StringRef getDescription() const final {
    return "Instruments conditional blocks with entry and exit trace calls";
  }

  void runOnOperation() override {
    ModuleOp rootModule = getOperation();
    MLIRContext *context = rootModule.getContext();
    OpBuilder builder(context);

    declareTraceFunctions(rootModule, builder);
    insertTraceCallsInAllConditions(rootModule, builder);
  }

private:
  void declareTraceFunctions(ModuleOp module, OpBuilder &builder) {
    auto declareFunction = [&](StringRef functionName) {
      if (!module.lookupSymbol<func::FuncOp>(functionName)) {
        builder.setInsertionPointToStart(module.getBody());
        auto functionType = builder.getFunctionType({}, {});
        builder
            .create<func::FuncOp>(builder.getUnknownLoc(), functionName,
                                  functionType)
            .setPrivate();
      }
    };

    declareFunction("trace_condition_then_begin");
    declareFunction("trace_condition_then_end");
    declareFunction("trace_condition_else_begin");
    declareFunction("trace_condition_else_end");
  }

  void insertTraceCallsInAllConditions(ModuleOp module, OpBuilder &builder) {
    module.walk([&](Operation *operation) {
      if (auto scfIfOp = dyn_cast<scf::IfOp>(operation)) {
        processScfIfOp(scfIfOp, builder);
      } else if (auto affineIfOp = dyn_cast<affine::AffineIfOp>(operation)) {
        processAffineIfOp(affineIfOp, builder);
      }
    });
  }

  void processScfIfOp(scf::IfOp ifOperation, OpBuilder &builder) {
    insertCallsInBlock(*ifOperation.thenBlock(), "trace_condition_then_begin",
                       "trace_condition_then_end", builder);

    if (ifOperation.elseBlock()) {
      insertCallsInBlock(*ifOperation.elseBlock(), "trace_condition_else_begin",
                         "trace_condition_else_end", builder);
    }
  }

  void processAffineIfOp(affine::AffineIfOp ifOperation, OpBuilder &builder) {
    insertCallsInBlock(*ifOperation.getThenBlock(),
                       "trace_condition_then_begin", "trace_condition_then_end",
                       builder);

    if (ifOperation.hasElse()) {
      insertCallsInBlock(*ifOperation.getElseBlock(),
                         "trace_condition_else_begin",
                         "trace_condition_else_end", builder);
    }
  }

  void insertCallsInBlock(Block &targetBlock, StringRef beginFunction,
                          StringRef endFunction, OpBuilder &builder) {
    if (targetBlock.empty())
      return;

    insertCallAtBlockBegin(targetBlock, beginFunction, builder);
    insertCallBeforeTerminator(targetBlock, endFunction, builder);
  }

  void insertCallAtBlockBegin(Block &targetBlock, StringRef functionName,
                              OpBuilder &builder) {
    builder.setInsertionPointToStart(&targetBlock);
    builder.create<func::CallOp>(builder.getUnknownLoc(), functionName,
                                 TypeRange{});
  }

  void insertCallBeforeTerminator(Block &targetBlock, StringRef functionName,
                                  OpBuilder &builder) {
    Operation &terminatorOperation = targetBlock.back();
    builder.setInsertionPoint(&terminatorOperation);
    builder.create<func::CallOp>(builder.getUnknownLoc(), functionName,
                                 TypeRange{});
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(PylaevaSPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(PylaevaSPass)

mlir::PassPluginLibraryInfo getTraceCondPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "PylaevaSPass", "1.0",
          []() { mlir::PassRegistration<PylaevaSPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTraceCondPassPluginInfo();
}