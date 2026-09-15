#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
class LoopFusionPass
    : public PassWrapper<LoopFusionPass, OperationPass<func::FuncOp>> {
public:
  StringRef getArgument() const final { return "spichek-loop-fusion"; }
  StringRef getDescription() const final {
    return "Fuses adjacent scf.for loops with identical bounds";
  }

  void runOnOperation() override {
    getOperation().walk([&](Block *block) {
      for (auto it = block->begin(); it != block->end();) {
        auto loop1 = dyn_cast<scf::ForOp>(&*it);
        if (!loop1) {
          ++it;
          continue;
        }

        auto nextIt = std::next(it);
        if (nextIt == block->end()) {
          ++it;
          break;
        }

        auto loop2 = dyn_cast<scf::ForOp>(&*nextIt);
        if (!loop2) {
          ++it;
          continue;
        }

        if (loop1.getLowerBound() != loop2.getLowerBound() ||
            loop1.getUpperBound() != loop2.getUpperBound() ||
            loop1.getStep() != loop2.getStep()) {
          ++it;
          continue;
        }

        bool hasDependency = false;
        for (Value res : loop1.getResults()) {
          for (Operation *user : res.getUsers()) {
            if (loop2->isAncestor(user)) {
              hasDependency = true;
              break;
            }
          }
        }
        if (hasDependency) {
          ++it;
          continue;
        }

        IRRewriter rewriter(loop1.getContext());
        rewriter.setInsertionPoint(loop2);

        SmallVector<Value> newIterArgs;
        llvm::append_range(newIterArgs, loop1.getInitArgs());
        llvm::append_range(newIterArgs, loop2.getInitArgs());

        auto newLoop = rewriter.create<scf::ForOp>(
            loop1.getLoc(), loop1.getLowerBound(), loop1.getUpperBound(),
            loop1.getStep(), newIterArgs);

        Block *newBlock = newLoop.getBody();
        Block *body1 = loop1.getBody();
        Block *body2 = loop2.getBody();

        rewriter.replaceAllUsesWith(body1->getArgument(0),
                                    newBlock->getArgument(0));
        rewriter.replaceAllUsesWith(body2->getArgument(0),
                                    newBlock->getArgument(0));

        for (auto [oldArg, newArg] :
             llvm::zip(body1->getArguments().drop_front(),
                       newBlock->getArguments().drop_front())) {
          rewriter.replaceAllUsesWith(oldArg, newArg);
        }
        size_t numArgs1 = body1->getNumArguments() - 1;
        for (auto [oldArg, newArg] :
             llvm::zip(body2->getArguments().drop_front(),
                       newBlock->getArguments().drop_front(1 + numArgs1))) {
          rewriter.replaceAllUsesWith(oldArg, newArg);
        }

        auto yield1 = cast<scf::YieldOp>(body1->getTerminator());
        auto yield2 = cast<scf::YieldOp>(body2->getTerminator());

        if (!newBlock->empty()) {
          rewriter.eraseOp(&newBlock->back());
        }

        newBlock->getOperations().splice(newBlock->end(),
                                         body1->getOperations(), body1->begin(),
                                         std::prev(body1->end()));
        newBlock->getOperations().splice(newBlock->end(),
                                         body2->getOperations(), body2->begin(),
                                         std::prev(body2->end()));

        SmallVector<Value> newYieldOperands;
        llvm::append_range(newYieldOperands, yield1.getOperands());
        llvm::append_range(newYieldOperands, yield2.getOperands());

        rewriter.setInsertionPointToEnd(newBlock);
        rewriter.create<scf::YieldOp>(newLoop.getLoc(), newYieldOperands);

        for (auto [oldRes, newRes] : llvm::zip(
                 loop1.getResults(),
                 newLoop.getResults().take_front(loop1.getNumResults()))) {
          rewriter.replaceAllUsesWith(oldRes, newRes);
        }
        for (auto [oldRes, newRes] : llvm::zip(
                 loop2.getResults(),
                 newLoop.getResults().drop_front(loop1.getNumResults()))) {
          rewriter.replaceAllUsesWith(oldRes, newRes);
        }

        rewriter.eraseOp(loop1);
        rewriter.eraseOp(loop2);

        it = Block::iterator(newLoop);
      }
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(LoopFusionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(LoopFusionPass)

mlir::PassPluginLibraryInfo getLoopFusionPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "LoopFusionPass", "1.0",
          []() { mlir::PassRegistration<LoopFusionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getLoopFusionPassPluginInfo();
}