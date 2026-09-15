#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace mlir;

namespace {

struct AccessInfo {
  bool reads = false;
  bool writes = false;
};

static bool isSameBound(Value lhs, Value rhs) {
  if (lhs == rhs)
    return true;

  auto lhsConst = lhs.getDefiningOp<arith::ConstantIndexOp>();
  auto rhsConst = rhs.getDefiningOp<arith::ConstantIndexOp>();
  return lhsConst && rhsConst && lhsConst.value() == rhsConst.value();
}

static bool isDefinedInsideFirstLoop(
    Value value, scf::ForOp firstLoop,
    const llvm::SmallPtrSetImpl<Operation *> &firstLoopOps) {
  Operation *defOp = value.getDefiningOp();
  if (!defOp)
    return false;

  return defOp == firstLoop.getOperation() || firstLoopOps.contains(defOp);
}

static LogicalResult
collectAccessesAndCheckEffects(scf::ForOp loop,
                               llvm::DenseMap<Value, AccessInfo> &accesses) {
  WalkResult result = loop.getBody()->walk([&](Operation *op) {
    if (op == loop.getBody()->getTerminator())
      return WalkResult::advance();

    if (op->getNumRegions() != 0)
      return WalkResult::advance();

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      accesses[load.getMemRef()].reads = true;
      return WalkResult::advance();
    }

    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      accesses[store.getMemRef()].writes = true;
      return WalkResult::advance();
    }

    if (isMemoryEffectFree(op))
      return WalkResult::advance();

    return WalkResult::interrupt();
  });

  return failure(result.wasInterrupted());
}

static bool hasInterLoopDependencies(scf::ForOp firstLoop,
                                     scf::ForOp secondLoop) {
  if (firstLoop.getNumResults() != 0 || secondLoop.getNumResults() != 0)
    return true;
  if (firstLoop.getInitArgs().size() != 0 ||
      secondLoop.getInitArgs().size() != 0)
    return true;

  llvm::SmallPtrSet<Operation *, 32> firstLoopOps;
  firstLoop.getBody()->walk([&](Operation *op) { firstLoopOps.insert(op); });

  for (Value operand : secondLoop->getOperands()) {
    if (isDefinedInsideFirstLoop(operand, firstLoop, firstLoopOps))
      return true;
  }

  WalkResult ssaDependencyCheck =
      secondLoop.getBody()->walk([&](Operation *op) {
        if (op == secondLoop.getBody()->getTerminator())
          return WalkResult::advance();

        for (Value operand : op->getOperands()) {
          if (isDefinedInsideFirstLoop(operand, firstLoop, firstLoopOps))
            return WalkResult::interrupt();
        }

        return WalkResult::advance();
      });
  if (ssaDependencyCheck.wasInterrupted())
    return true;

  llvm::DenseMap<Value, AccessInfo> firstAccesses;
  llvm::DenseMap<Value, AccessInfo> secondAccesses;
  if (failed(collectAccessesAndCheckEffects(firstLoop, firstAccesses)) ||
      failed(collectAccessesAndCheckEffects(secondLoop, secondAccesses)))
    return true;

  for (const auto &entry : firstAccesses) {
    auto secondIt = secondAccesses.find(entry.first);
    if (secondIt == secondAccesses.end())
      continue;

    bool hasWrite = entry.second.writes || secondIt->second.writes;
    if (hasWrite)
      return true;
  }

  return false;
}

struct LoopFusionPattern : public OpRewritePattern<scf::ForOp> {
  using OpRewritePattern<scf::ForOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(scf::ForOp firstLoop,
                                PatternRewriter &rewriter) const override {
    auto secondLoop = dyn_cast_or_null<scf::ForOp>(firstLoop->getNextNode());
    if (!secondLoop)
      return failure();

    if (!isSameBound(firstLoop.getLowerBound(), secondLoop.getLowerBound()) ||
        !isSameBound(firstLoop.getUpperBound(), secondLoop.getUpperBound()) ||
        !isSameBound(firstLoop.getStep(), secondLoop.getStep())) {
      return failure();
    }

    if (hasInterLoopDependencies(firstLoop, secondLoop))
      return failure();

    rewriter.setInsertionPoint(firstLoop.getBody()->getTerminator());
    IRMapping mapping;
    mapping.map(secondLoop.getInductionVar(), firstLoop.getInductionVar());

    for (Operation &op : secondLoop.getBody()->without_terminator())
      rewriter.clone(op, mapping);

    rewriter.eraseOp(secondLoop);
    return success();
  }
};

class LoopFusionPass
    : public PassWrapper<LoopFusionPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LoopFusionPass)

  StringRef getArgument() const final { return "adjacent-scf-loop-fusion"; }

  StringRef getDescription() const final {
    return "Fuses adjacent scf.for loops with identical bounds and no "
           "inter-loop dependencies";
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<LoopFusionPattern>(&getContext());
    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns))))
      signalPassFailure();
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(LoopFusionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(LoopFusionPass)

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "LoopFusionPass", "1.0",
          []() { mlir::PassRegistration<LoopFusionPass>(); }};
}