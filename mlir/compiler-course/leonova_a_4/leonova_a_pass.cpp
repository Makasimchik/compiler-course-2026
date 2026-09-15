#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

struct LowerCopyOp : public OpRewritePattern<memref::CopyOp> {
  using OpRewritePattern::OpRewritePattern;

  static Value makeIndexConstant(OpBuilder &builder, Location loc,
                                 int64_t value) {
    return builder.create<arith::ConstantIndexOp>(loc, value);
  }

  static void emitElementTransfer(OpBuilder &builder, Location loc,
                                  Value sourceBuffer, Value targetBuffer,
                                  Value indexValue) {

    auto loadedElement = builder.create<memref::LoadOp>(loc, sourceBuffer,
                                                        ValueRange{indexValue});

    builder.create<memref::StoreOp>(loc, loadedElement, targetBuffer,
                                    ValueRange{indexValue});
  }

  LogicalResult matchAndRewrite(memref::CopyOp copyInst,
                                PatternRewriter &rewriter) const override {

    auto bufferType = llvm::cast<MemRefType>(copyInst.getSource().getType());

    if (bufferType.getRank() != 1 || !bufferType.hasStaticShape())
      return failure();

    auto elementCount = bufferType.getShape()[0];

    auto loc = copyInst.getLoc();

    auto lowerBound = makeIndexConstant(rewriter, loc, 0);

    auto upperBound = makeIndexConstant(rewriter, loc, elementCount);

    auto stepValue = makeIndexConstant(rewriter, loc, 1);

    auto loopOp =
        rewriter.create<scf::ForOp>(loc, lowerBound, upperBound, stepValue);

    Block &loopBody = loopOp.getRegion().front();

    rewriter.setInsertionPointToStart(&loopBody);

    Value inductionVar = loopOp.getInductionVar();

    emitElementTransfer(rewriter, loc, copyInst.getSource(),
                        copyInst.getTarget(), inductionVar);

    rewriter.replaceOp(copyInst, ValueRange{});

    return success();
  }
};

class LinearCopyExpansionPass
    : public PassWrapper<LinearCopyExpansionPass, OperationPass<ModuleOp>> {

public:
  StringRef getArgument() const final { return "memref-copy-to-loop"; }

  StringRef getDescription() const final {
    return "Expand memref.copy into explicit loops";
  }

  void getDependentDialects(DialectRegistry &registry) const override {

    registry
        .insert<scf::SCFDialect, memref::MemRefDialect, arith::ArithDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    RewritePatternSet rewritePatterns(&getContext());

    rewritePatterns.add<LowerCopyOp>(&getContext());

    if (failed(applyPatternsGreedily(module, std::move(rewritePatterns)))) {

      signalPassFailure();
    }
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(LinearCopyExpansionPass)

MLIR_DEFINE_EXPLICIT_TYPE_ID(LinearCopyExpansionPass)

mlir::PassPluginLibraryInfo getLinearCopyExpansionPassPluginInfo() {

  return {MLIR_PLUGIN_API_VERSION, "LinearCopyExpansionPass", "1.0",
          []() { mlir::PassRegistration<LinearCopyExpansionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {

  return getLinearCopyExpansionPassPluginInfo();
}