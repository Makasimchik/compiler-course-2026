#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
struct CopyToLoopPattern : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern<memref::CopyOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(memref::CopyOp copyOp,
                                PatternRewriter &rewriter) const override {
    Location loc = copyOp.getLoc();
    Value src = copyOp.getSource();
    Value dst = copyOp.getTarget();

    auto memrefType = llvm::cast<MemRefType>(src.getType());

    if (!memrefType.hasStaticShape() || memrefType.getRank() != 1)
      return failure();

    int64_t size = memrefType.getDimSize(0);

    Value c0 = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value c1 = rewriter.create<arith::ConstantIndexOp>(loc, 1);
    Value cSize = rewriter.create<arith::ConstantIndexOp>(loc, size);

    rewriter.create<scf::ForOp>(
        loc, c0, cSize, c1, ValueRange{},
        [&](OpBuilder &builder, Location loc, Value iv, ValueRange) {
          Value val = builder.create<memref::LoadOp>(loc, src, iv);

          builder.create<memref::StoreOp>(loc, val, dst, iv);
          builder.create<scf::YieldOp>(loc);
        });

    rewriter.eraseOp(copyOp);
    return success();
  }
};

class ZeninMemrefPass
    : public PassWrapper<ZeninMemrefPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "zenin-memref-copy"; }
  StringRef getDescription() const final {
    return "Replace memref.copy with scf.for loops";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, memref::MemRefDialect, arith::ArithDialect>();
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    RewritePatternSet patterns(&getContext());
    patterns.add<CopyToLoopPattern>(&getContext());

    if (failed(applyPatternsGreedily(moduleOp, std::move(patterns)))) {
      signalPassFailure();
    }
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ZeninMemrefPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ZeninMemrefPass)

mlir::PassPluginLibraryInfo getZeninMemrefPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ZeninMemrefPass", "1.0",
          []() { mlir::PassRegistration<ZeninMemrefPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getZeninMemrefPassPluginInfo();
}
