#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;

namespace {
struct OvsyannikovLoopFusion : public OpRewritePattern<scf::ForOp> {
  using OpRewritePattern<scf::ForOp>::OpRewritePattern;
  LogicalResult matchAndRewrite(scf::ForOp firstLoop,
                                PatternRewriter &rewriter) const override {
    Operation *nextOp = firstLoop->getNextNode();
    auto secondLoop = dyn_cast_or_null<scf::ForOp>(nextOp);
    if (!secondLoop)
      return failure();
    if (firstLoop.getLowerBound() != secondLoop.getLowerBound() ||
        firstLoop.getUpperBound() != secondLoop.getUpperBound() ||
        firstLoop.getStep() != secondLoop.getStep())
      return failure();
    for (Value res : firstLoop.getResults()) {
      for (Operation *user : res.getUsers()) {
        if (secondLoop->isAncestor(user))
          return failure();
      }
    }
    rewriter.setInsertionPoint(firstLoop.getBody()->getTerminator());
    IRMapping mapper;
    mapper.map(secondLoop.getInductionVar(), firstLoop.getInductionVar());
    for (auto &op : secondLoop.getBody()->without_terminator()) {
      rewriter.clone(op, mapper);
    }
    rewriter.eraseOp(secondLoop);
    return success();
  }
};

class OvsyannikovSCFPass
    : public PassWrapper<OvsyannikovSCFPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(OvsyannikovSCFPass)
  StringRef getArgument() const final { return "ovsyannikov-loop-fusion"; }
  StringRef getDescription() const final {
    return "Fuses adjacent scf.for loops";
  }
  void runOnOperation() override {
    MLIRContext *context = &getContext();
    RewritePatternSet patterns(context);
    patterns.add<OvsyannikovLoopFusion>(context);
    GreedyRewriteConfig config;
    (void)applyPatternsGreedily(getOperation(), std::move(patterns), config);
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(OvsyannikovSCFPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(OvsyannikovSCFPass)

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "OvsyannikovSCF", "1.0",
          []() { mlir::PassRegistration<OvsyannikovSCFPass>(); }};
}
