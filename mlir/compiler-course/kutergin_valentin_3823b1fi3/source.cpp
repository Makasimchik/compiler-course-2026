#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/Support/Casting.h"

using namespace mlir;

namespace {

class LowerCopyPattern : public OpRewritePattern<memref::CopyOp> {
public:
  using OpRewritePattern<memref::CopyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp copyOp,
                                PatternRewriter &rewriter) const override {
    Value source = copyOp.getSource();
    Value target = copyOp.getTarget();

    auto memRefType = llvm::dyn_cast<MemRefType>(source.getType());
    if (!memRefType)
      return failure();

    Location loc = copyOp.getLoc();
    auto shape = memRefType.getShape();
    int rank = memRefType.getRank();

    Value c0 = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value c1 = rewriter.create<arith::ConstantIndexOp>(loc, 1);

    SmallVector<Value, 4> lbs(rank, c0);
    SmallVector<Value, 4> steps(rank, c1);
    SmallVector<Value, 4> ubs;
    for (int64_t dim : shape) {
      ubs.push_back(rewriter.create<arith::ConstantIndexOp>(loc, dim));
    }

    scf::buildLoopNest(rewriter, loc, lbs, ubs, steps,
                       [&](OpBuilder &b, Location loc, ValueRange ivs) {
                         Value val = b.create<memref::LoadOp>(loc, source, ivs);
                         b.create<memref::StoreOp>(loc, val, target, ivs);
                       });

    rewriter.eraseOp(copyOp);
    return success();
  }
};

struct MemRefToSCFPass
    : public PassWrapper<MemRefToSCFPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MemRefToSCFPass)

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, arith::ArithDialect, memref::MemRefDialect>();
  }

  void runOnOperation() override {
    MLIRContext *context = &getContext();
    RewritePatternSet patterns(context);
    patterns.add<LowerCopyPattern>(context);

    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns)))) {
      signalPassFailure();
    }
  }

  StringRef getArgument() const final { return "lower-memref-copy"; }
};

} // namespace

// Регистрация пасса
extern "C" LLVM_ATTRIBUTE_WEAK ::mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MemRefToSCFLab", "0.1",
          []() { PassRegistration<MemRefToSCFPass>(); }};
}
