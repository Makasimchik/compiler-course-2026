#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

void insertBlock(Block &block, Location loc, StringRef beginName,
                 StringRef endName) {
  OpBuilder builderBegin(&block, block.begin());
  builderBegin.create<func::CallOp>(loc, beginName, TypeRange{}, ValueRange{});

  if (auto *term = block.getTerminator()) {
    OpBuilder builderEnd(term);
    builderEnd.create<func::CallOp>(loc, endName, TypeRange{}, ValueRange{});
  }
}

class ChyokotovATraceCondPass
    : public PassWrapper<ChyokotovATraceCondPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "chyokotov_trace_cond_MLIR"; }
  StringRef getDescription() const final { return "Description pass"; }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    OpBuilder builder(moduleOp.getContext());
    builder.setInsertionPointToStart(moduleOp.getBody());

    auto addFuncDecl = [&](StringRef name) {
      if (!moduleOp.lookupSymbol<func::FuncOp>(name)) {
        auto funcType = builder.getFunctionType(TypeRange{}, TypeRange{});
        auto func =
            builder.create<func::FuncOp>(moduleOp.getLoc(), name, funcType);
        func.setPrivate();
      }
    };

    addFuncDecl("trace_condition_then_begin");
    addFuncDecl("trace_condition_then_end");
    addFuncDecl("trace_condition_else_begin");
    addFuncDecl("trace_condition_else_end");

    moduleOp.walk([&](Operation *op) {
      if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
        insertBlock(ifOp.getThenRegion().front(), ifOp.getLoc(),
                    "trace_condition_then_begin", "trace_condition_then_end");
        if (!ifOp.getElseRegion().empty()) {
          insertBlock(ifOp.getElseRegion().front(), ifOp.getLoc(),
                      "trace_condition_else_begin", "trace_condition_else_end");
        }
      } else if (auto forOp = dyn_cast<scf::ForOp>(op)) {
        insertBlock(forOp.getRegion().front(), forOp.getLoc(),
                    "trace_condition_then_begin", "trace_condition_then_end");
      } else if (auto whileOp = dyn_cast<scf::WhileOp>(op)) {
        insertBlock(whileOp.getBefore().front(), whileOp.getLoc(),
                    "trace_condition_then_begin", "trace_condition_then_end");
        insertBlock(whileOp.getAfter().front(), whileOp.getLoc(),
                    "trace_condition_then_begin", "trace_condition_then_end");
      } else if (auto affineIfOp = dyn_cast<affine::AffineIfOp>(op)) {
        insertBlock(*affineIfOp.getThenBlock(), affineIfOp.getLoc(),
                    "trace_condition_then_begin", "trace_condition_then_end");
        if (affineIfOp.hasElse()) {
          insertBlock(*affineIfOp.getElseBlock(), affineIfOp.getLoc(),
                      "trace_condition_else_begin", "trace_condition_else_end");
        }
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ChyokotovATraceCondPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ChyokotovATraceCondPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ChyokotovATraceCondPass", "1.0",
          []() { mlir::PassRegistration<ChyokotovATraceCondPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}