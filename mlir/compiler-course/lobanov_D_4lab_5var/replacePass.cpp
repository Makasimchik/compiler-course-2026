#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;

namespace {

static void emitLoopNest(OpBuilder &builder, Location loc, Value src, Value dst,
                         ArrayRef<Value> lbs, ArrayRef<Value> ubs,
                         ArrayRef<Value> steps, unsigned dim,
                         SmallVectorImpl<Value> &ivs) {
  if (dim == lbs.size()) {
    Value element = builder.create<memref::LoadOp>(loc, src, ivs);
    builder.create<memref::StoreOp>(loc, element, dst, ivs);
    return;
  }

  auto forOp = builder.create<scf::ForOp>(loc, lbs[dim], ubs[dim], steps[dim]);

  {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(forOp.getBody());
    ivs.push_back(forOp.getInductionVar());
    emitLoopNest(builder, loc, src, dst, lbs, ubs, steps, dim + 1, ivs);
    ivs.pop_back();
  }
}

struct MemrefCopyExpansion : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp copyOp,
                                PatternRewriter &rewriter) const override {
    Location loc = copyOp.getLoc();
    Value src = copyOp.getSource();
    Value dst = copyOp.getTarget();

    auto memrefType = dyn_cast<MemRefType>(src.getType());
    if (!memrefType)
      return failure();
    int64_t rank = memrefType.getRank();

    SmallVector<Value> lbs, ubs, steps;
    for (int64_t d = 0; d < rank; ++d) {
      lbs.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 0));
      steps.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 1));
      ubs.push_back(rewriter.create<memref::DimOp>(loc, src, d));
    }

    SmallVector<Value> ivs;
    emitLoopNest(rewriter, loc, src, dst, lbs, ubs, steps, 0, ivs);

    rewriter.eraseOp(copyOp);
    return success();
  }
};

class MemrefCopyExpansionPass
    : public PassWrapper<MemrefCopyExpansionPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MemrefCopyExpansionPass)

  StringRef getArgument() const final { return "expand-memref-copy-to-scf"; }

  StringRef getDescription() const final {
    return "Replace memref.copy operations with element-wise scf.for loops "
           "using memref.load and memref.store";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, memref::MemRefDialect, arith::ArithDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    RewritePatternSet patterns(&getContext());
    patterns.add<MemrefCopyExpansion>(&getContext());

    if (failed(applyPatternsGreedily(module, std::move(patterns))))
      signalPassFailure();
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MemrefCopyExpansionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MemrefCopyExpansionPass)

mlir::PassPluginLibraryInfo getMemrefCopyExpansionPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MemrefCopyExpansion", "1.0",
          []() { mlir::PassRegistration<MemrefCopyExpansionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getMemrefCopyExpansionPluginInfo();
}