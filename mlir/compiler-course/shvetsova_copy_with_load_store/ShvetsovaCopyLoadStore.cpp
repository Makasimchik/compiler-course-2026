#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;

namespace {

struct ReplaceCopyPattern : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern<memref::CopyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp copyOp,
                                PatternRewriter &rewriter) const override {
    auto loc = copyOp.getLoc();
    Value src = copyOp.getSource();
    Value dst = copyOp.getTarget();

    auto memrefType = llvm::cast<MemRefType>(src.getType());
    unsigned rank = memrefType.getRank();

    if (rank == 0) {
      Value val = rewriter.create<memref::LoadOp>(loc, src);
      rewriter.create<memref::StoreOp>(loc, val, dst);
      rewriter.eraseOp(copyOp);
      return success();
    }

    SmallVector<Value, 4> upperBounds;
    for (unsigned i = 0; i < rank; ++i) {
      int64_t size = memrefType.getDimSize(i);
      if (ShapedType::isDynamic(size)) {
        upperBounds.push_back(rewriter.create<memref::DimOp>(loc, src, i));
      } else {
        upperBounds.push_back(
            rewriter.create<arith::ConstantIndexOp>(loc, size));
      }
    }

    Value c0 = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value c1 = rewriter.create<arith::ConstantIndexOp>(loc, 1);
    SmallVector<Value, 4> ivs;

    std::function<void(unsigned)> buildNestedLoops = [&](unsigned dim) {
      if (dim == rank) {
        Value element = rewriter.create<memref::LoadOp>(loc, src, ivs);
        rewriter.create<memref::StoreOp>(loc, element, dst, ivs);
        return;
      }

      auto forOp = rewriter.create<scf::ForOp>(loc, c0, upperBounds[dim], c1);

      {
        OpBuilder::InsertionGuard guard(rewriter);
        rewriter.setInsertionPointToStart(forOp.getBody());

        ivs.push_back(forOp.getInductionVar());
        buildNestedLoops(dim + 1);
        ivs.pop_back();
      }
    };

    buildNestedLoops(0);

    rewriter.eraseOp(copyOp);
    return success();
  }
};

class LowerMemrefCopyPass
    : public PassWrapper<LowerMemrefCopyPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerMemrefCopyPass)

  StringRef getArgument() const final { return "lower-memref-copy"; }
  StringRef getDescription() const final {
    return "Replaces memref.copy with scf.for loop nests via recursive "
           "emission";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<memref::MemRefDialect, scf::SCFDialect, arith::ArithDialect>();
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<ReplaceCopyPattern>(&getContext());
    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns))))
      signalPassFailure();
  }
};

} // namespace

mlir::PassPluginLibraryInfo getLowerMemrefCopyPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "LowerMemrefCopyPass", "1.0",
          []() { PassRegistration<LowerMemrefCopyPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  return getLowerMemrefCopyPassPluginInfo();
}