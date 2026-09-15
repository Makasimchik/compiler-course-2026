#include "mlir/Pass/Pass.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;

namespace {

struct MemRefCopyLowering : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern<memref::CopyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp op,
                                PatternRewriter &rewriter) const override {
    Location loc = op.getLoc();
    Value source = op.getSource();
    Value target = op.getTarget();

    auto memRefType = cast<MemRefType>(source.getType());
    int64_t rank = memRefType.getRank();

    Value zero = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value one = rewriter.create<arith::ConstantIndexOp>(loc, 1);

    SmallVector<Value> lowerBounds(rank, zero);
    SmallVector<Value> steps(rank, one);
    SmallVector<Value> upperBounds;

    for (int64_t i = 0; i < rank; ++i) {
      upperBounds.push_back(rewriter.create<memref::DimOp>(loc, source, i));
    }

    scf::buildLoopNest(rewriter, loc, lowerBounds, upperBounds, steps,
                       [&](OpBuilder &b, Location l, ValueRange ivs) {
                         Value val = b.create<memref::LoadOp>(l, source, ivs);
                         b.create<memref::StoreOp>(l, val, target, ivs);
                       });

    rewriter.eraseOp(op);
    return success();
  }
};

struct MemRefCopyToLowerPass
    : public PassWrapper<MemRefCopyToLowerPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MemRefCopyToLowerPass)

  StringRef getArgument() const final { return "memref-copy-to-loops"; }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, arith::ArithDialect, memref::MemRefDialect>();
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<MemRefCopyLowering>(&getContext());
    if (failed(
            applyPatternsAndFoldGreedily(getOperation(), std::move(patterns))))
      signalPassFailure();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MemRefCopyToLowerPass", "v0.1",
          []() { PassRegistration<MemRefCopyToLowerPass>(); }};
}