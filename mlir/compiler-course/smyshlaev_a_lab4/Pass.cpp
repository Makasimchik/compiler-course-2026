#include "mlir/Pass/Pass.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/DialectConversion.h"

using namespace mlir;

namespace {

struct MemRefCopyConversion : public OpConversionPattern<memref::CopyOp> {
  using OpConversionPattern<memref::CopyOp>::OpConversionPattern;

  LogicalResult
  matchAndRewrite(memref::CopyOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto loc = op.getLoc();
    auto src = op.getSource();
    auto dst = op.getTarget();

    auto type = dyn_cast<MemRefType>(src.getType());
    if (!type)
      return rewriter.notifyMatchFailure(op, "requires MemRef type");

    auto rank = type.getRank();
    auto bounds = getDimValues(rewriter, loc, src, rank);

    scf::buildLoopNest(
        rewriter, loc,
        SmallVector<Value>(rank,
                           rewriter.create<arith::ConstantIndexOp>(loc, 0)),
        bounds,
        SmallVector<Value>(rank,
                           rewriter.create<arith::ConstantIndexOp>(loc, 1)),
        [&](OpBuilder &b, Location l, ValueRange ivs) {
          auto val = b.create<memref::LoadOp>(l, src, ivs);
          b.create<memref::StoreOp>(l, val, dst, ivs);
        });

    rewriter.eraseOp(op);
    return success();
  }

private:
  SmallVector<Value> getDimValues(OpBuilder &b, Location loc, Value memref,
                                  int64_t rank) const {
    SmallVector<Value> dims;
    for (int64_t i = 0; i < rank; ++i) {
      dims.push_back(b.create<memref::DimOp>(loc, memref, i));
    }
    return dims;
  }
};

struct CopyLoweringPass
    : public PassWrapper<CopyLoweringPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(CopyLoweringPass)

  void runOnOperation() override {
    auto func = getOperation();
    ConversionTarget target(getContext());
    target.addIllegalOp<memref::CopyOp>();
    target.addLegalDialect<scf::SCFDialect, arith::ArithDialect,
                           memref::MemRefDialect>();

    RewritePatternSet patterns(&getContext());
    patterns.add<MemRefCopyConversion>(&getContext());

    if (failed(applyPartialConversion(func, target, std::move(patterns)))) {
      signalPassFailure();
    }
  }
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<scf::SCFDialect, arith::ArithDialect, memref::MemRefDialect,
                    func::FuncDialect>();
  }

  StringRef getArgument() const final { return "lower-memref-copy-to-scf"; }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(CopyLoweringPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(CopyLoweringPass)

mlir::PassPluginLibraryInfo getCopyLoweringPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "CopyLoweringPass", "1.0",
          []() { mlir::PassRegistration<CopyLoweringPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getCopyLoweringPluginInfo();
}
