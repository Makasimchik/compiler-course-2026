#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/Support/Casting.h" // Обязательно для llvm::cast

using namespace mlir;

namespace {

struct MemcopyToLoopsPattern : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern<memref::CopyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp copyOp,
                                PatternRewriter &rewriter) const override {
    Value src = copyOp.getSource();
    Value dst = copyOp.getTarget();
    Location loc = copyOp.getLoc();

    auto memrefType = llvm::cast<MemRefType>(src.getType());
    int64_t rank = memrefType.getRank();

    if (rank == 0) {
      Value elem = rewriter.create<memref::LoadOp>(loc, src);
      rewriter.create<memref::StoreOp>(loc, elem, dst);
      rewriter.eraseOp(copyOp);
      return success();
    }

    Value zeroIdx = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value oneIdx = rewriter.create<arith::ConstantIndexOp>(loc, 1);

    SmallVector<Value> lowerBounds(rank, zeroIdx);
    SmallVector<Value> steps(rank, oneIdx);
    SmallVector<Value> upperBounds;
    upperBounds.reserve(rank);

    for (int64_t d = 0; d < rank; ++d) {
      int64_t dimSize = memrefType.getDimSize(d);
      if (ShapedType::isDynamic(dimSize))
        upperBounds.push_back(rewriter.create<memref::DimOp>(loc, src, d));
      else
        upperBounds.push_back(
            rewriter.create<arith::ConstantIndexOp>(loc, dimSize));
    }

    scf::buildLoopNest(rewriter, loc, lowerBounds, upperBounds, steps,
                       [src, dst](OpBuilder &b, Location loc, ValueRange ivs) {
                         Value val = b.create<memref::LoadOp>(loc, src, ivs);
                         b.create<memref::StoreOp>(loc, val, dst, ivs);
                       });

    rewriter.eraseOp(copyOp);
    return success();
  }
};

struct ReplaceMemrefPass
    : public PassWrapper<ReplaceMemrefPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ReplaceMemrefPass)

  StringRef getArgument() const final { return "expand-memcopy"; }
  StringRef getDescription() const final {
    return "Expand memref.copy to scf.for loops.";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, arith::ArithDialect, memref::MemRefDialect>();
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<MemcopyToLoopsPattern>(&getContext());

    // Используем актуальное название функции
    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns))))
      signalPassFailure();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ReplaceMemref", "1.0",
          []() { PassRegistration<ReplaceMemrefPass>(); }};
}