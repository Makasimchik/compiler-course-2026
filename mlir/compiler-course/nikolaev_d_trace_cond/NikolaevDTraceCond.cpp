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

static func::FuncOp getOrInsertFuncDecl(ModuleOp moduleOp, OpBuilder &builder,
                                        StringRef name) {
  if (auto fn = moduleOp.lookupSymbol<func::FuncOp>(name))
    return fn;

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(moduleOp.getBody());

  auto funcType = builder.getFunctionType(/*inputs=*/TypeRange{},
                                          /*results=*/TypeRange{});
  auto fn = builder.create<func::FuncOp>(moduleOp.getLoc(), name, funcType);
  fn.setPrivate();
  return fn;
}

static void insertTraceInBlock(Block &block, Location loc, func::FuncOp beginFn,
                               func::FuncOp endFn) {
  // begin: в начало блока
  {
    OpBuilder builderBegin(&block, block.begin());
    builderBegin.create<func::CallOp>(loc, beginFn.getSymName(),
                                      /*resultTypes=*/TypeRange{},
                                      /*operands=*/ValueRange{});
  }

  // end: перед терминатором
  if (Operation *term = block.getTerminator()) {
    OpBuilder builderEnd(term);
    builderEnd.create<func::CallOp>(loc, endFn.getSymName(),
                                    /*resultTypes=*/TypeRange{},
                                    /*operands=*/ValueRange{});
  }
}

class NikolaevDTraceCondPass
    : public PassWrapper<NikolaevDTraceCondPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "nikolaev_d_trace_cond_MLIR"; }
  StringRef getDescription() const final {
    return "Insert trace_condition_* calls on each condition block";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(moduleOp.getContext());

    auto thenBeginFn =
        getOrInsertFuncDecl(moduleOp, builder, "trace_condition_then_begin");
    auto thenEndFn =
        getOrInsertFuncDecl(moduleOp, builder, "trace_condition_then_end");
    auto elseBeginFn =
        getOrInsertFuncDecl(moduleOp, builder, "trace_condition_else_begin");
    auto elseEndFn =
        getOrInsertFuncDecl(moduleOp, builder, "trace_condition_else_end");

    moduleOp.walk([&](Operation *op) {
      if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
        if (!ifOp.getThenRegion().empty()) {
          insertTraceInBlock(ifOp.getThenRegion().front(), ifOp.getLoc(),
                             thenBeginFn, thenEndFn);
        }
        if (!ifOp.getElseRegion().empty()) {
          insertTraceInBlock(ifOp.getElseRegion().front(), ifOp.getLoc(),
                             elseBeginFn, elseEndFn);
        }
        return;
      }

      if (auto affineIfOp = dyn_cast<affine::AffineIfOp>(op)) {
        if (Block *thenBlock = affineIfOp.getThenBlock()) {
          insertTraceInBlock(*thenBlock, affineIfOp.getLoc(), thenBeginFn,
                             thenEndFn);
        }
        if (affineIfOp.hasElse()) {
          if (Block *elseBlock = affineIfOp.getElseBlock()) {
            insertTraceInBlock(*elseBlock, affineIfOp.getLoc(), elseBeginFn,
                               elseEndFn);
          }
        }
        return;
      }

      if (auto forOp = dyn_cast<scf::ForOp>(op)) {

        if (!forOp.getRegion().empty()) {
          insertTraceInBlock(forOp.getRegion().front(), forOp.getLoc(),
                             thenBeginFn, thenEndFn);
        }

        return;
      }

      if (auto whileOp = dyn_cast<scf::WhileOp>(op)) {

        if (!whileOp.getBefore().empty()) {
          insertTraceInBlock(whileOp.getBefore().front(), whileOp.getLoc(),
                             thenBeginFn, thenEndFn);
        }

        if (!whileOp.getAfter().empty()) {
          insertTraceInBlock(whileOp.getAfter().front(), whileOp.getLoc(),
                             thenBeginFn, thenEndFn);
        }

        return;
      }
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(NikolaevDTraceCondPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(NikolaevDTraceCondPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "NikolaevDTraceCondPass", "1.0",
          []() { mlir::PassRegistration<NikolaevDTraceCondPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}