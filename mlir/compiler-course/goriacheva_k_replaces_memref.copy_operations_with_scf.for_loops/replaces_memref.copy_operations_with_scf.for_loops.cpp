#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;

namespace {

struct ReplaceMemrefCopyPattern : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp op,
                                PatternRewriter &rewriter) const override {
    Location loc = op.getLoc();

    Value src = op.getSource();
    Value dst = op.getTarget();

    auto type = dyn_cast<MemRefType>(src.getType());
    if (!type)
      return failure();

    if (src.getType() != dst.getType())
      return failure();

    int rank = type.getRank();

    SmallVector<Value> lbs, ubs, steps;

    for (int i = 0; i < rank; ++i) {
      lbs.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 0));
      steps.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 1));

      Value ub;
      if (type.isDynamicDim(i)) {
        ub = rewriter.create<memref::DimOp>(loc, src, i);
      } else {
        ub = rewriter.create<arith::ConstantIndexOp>(loc, type.getDimSize(i));
      }
      ubs.push_back(ub);
    }

    std::function<void(int, SmallVector<Value> &)> buildLoop =
        [&](int dim, SmallVector<Value> &ivs) {
          if (dim == rank) {
            Value val = rewriter.create<memref::LoadOp>(loc, src, ivs);
            rewriter.create<memref::StoreOp>(loc, val, dst, ivs);
            return;
          }

          auto forOp =
              rewriter.create<scf::ForOp>(loc, lbs[dim], ubs[dim], steps[dim]);

          OpBuilder::InsertionGuard guard(rewriter);
          rewriter.setInsertionPointToStart(forOp.getBody());

          ivs.push_back(forOp.getInductionVar());
          buildLoop(dim + 1, ivs);
          ivs.pop_back();
        };

    SmallVector<Value> ivs;
    buildLoop(0, ivs);

    rewriter.eraseOp(op);
    return success();
  }
};

class ReplacesMemrefWithScfPass
    : public PassWrapper<ReplacesMemrefWithScfPass, OperationPass<ModuleOp>> {

public:
  StringRef getArgument() const final {
    return "replace-memref-copy-with-loops";
  }

  StringRef getDescription() const final {
    return "Replace memref.copy with scf.for loops";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<arith::ArithDialect, memref::MemRefDialect, scf::SCFDialect>();
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<ReplaceMemrefCopyPattern>(&getContext());

    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns)))) {
      signalPassFailure();
    }
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ReplacesMemrefWithScfPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ReplacesMemrefWithScfPass)

mlir::PassPluginLibraryInfo getReplacesMemrefWithScfPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ReplacesMemrefWithScfPass", "1.0",
          []() { mlir::PassRegistration<ReplacesMemrefWithScfPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getReplacesMemrefWithScfPassPluginInfo();
}