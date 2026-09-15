#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

class LiulinLoopFusion
    : public PassWrapper<LiulinLoopFusion, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "liulin_loop_fuse"; }
  StringRef getDescription() const final {
    return "Fuses two consecutive scf.for loops if conditions are met";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    bool changed;

    do {
      changed = false;

      auto walkResult = moduleOp.walk([&](Block *block) {
        for (auto it = block->begin(); it != block->end(); ++it) {
          auto loop1 = dyn_cast<scf::ForOp>(&*it);
          if (!loop1)
            continue;

          auto nextIt = std::next(it);
          if (nextIt == block->end())
            continue;

          auto loop2 = dyn_cast<scf::ForOp>(&*nextIt);
          if (!loop2)
            continue;

          if (tryFuseLoops(loop1, loop2)) {
            return WalkResult::interrupt();
          }
        }
        return WalkResult::advance();
      });

      if (walkResult.wasInterrupted()) {
        changed = true;
      }
    } while (changed);
  }

private:
  bool tryFuseLoops(scf::ForOp loop1, scf::ForOp loop2) {
    if (loop1.getLowerBound() != loop2.getLowerBound() ||
        loop1.getUpperBound() != loop2.getUpperBound() ||
        loop1.getStep() != loop2.getStep()) {
      return false;
    }

    for (Value res : loop1.getResults()) {
      for (Operation *user : res.getUsers()) {
        if (loop2->isAncestor(user) || loop2 == user) {
          return false;
        }
      }
    }

    OpBuilder builder(loop1);

    SmallVector<Value> newInitArgs;
    newInitArgs.append(loop1.getInitArgs().begin(), loop1.getInitArgs().end());
    newInitArgs.append(loop2.getInitArgs().begin(), loop2.getInitArgs().end());

    auto newLoop = builder.create<scf::ForOp>(
        loop1.getLoc(), loop1.getLowerBound(), loop1.getUpperBound(),
        loop1.getStep(), newInitArgs);

    Block *newBody = newLoop.getBody();

    if (!newBody->empty()) {
      newBody->back().erase();
    }

    Block *body1 = loop1.getBody();
    Block *body2 = loop2.getBody();

    body1->getArgument(0).replaceAllUsesWith(newBody->getArgument(0));
    body2->getArgument(0).replaceAllUsesWith(newBody->getArgument(0));

    int argIdx = 1;
    for (auto arg : body1->getArguments().drop_front()) {
      arg.replaceAllUsesWith(newBody->getArgument(argIdx++));
    }
    for (auto arg : body2->getArguments().drop_front()) {
      arg.replaceAllUsesWith(newBody->getArgument(argIdx++));
    }

    auto yield1 = cast<scf::YieldOp>(body1->getTerminator());
    auto yield2 = cast<scf::YieldOp>(body2->getTerminator());

    SmallVector<Value> newYields;
    newYields.append(yield1.getOperands().begin(), yield1.getOperands().end());
    newYields.append(yield2.getOperands().begin(), yield2.getOperands().end());

    newBody->getOperations().splice(newBody->end(), body1->getOperations(),
                                    body1->begin(), Block::iterator(yield1));
    newBody->getOperations().splice(newBody->end(), body2->getOperations(),
                                    body2->begin(), Block::iterator(yield2));

    builder.setInsertionPointToEnd(newBody);
    builder.create<scf::YieldOp>(builder.getUnknownLoc(), newYields);

    if (loop1.getNumResults() > 0) {
      auto res1 = newLoop.getResults().take_front(loop1.getNumResults());
      loop1.replaceAllUsesWith(res1);
    }
    if (loop2.getNumResults() > 0) {
      auto res2 = newLoop.getResults().take_back(loop2.getNumResults());
      loop2.replaceAllUsesWith(res2);
    }

    loop2.erase();
    loop1.erase();

    return true;
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(LiulinLoopFusion)
MLIR_DEFINE_EXPLICIT_TYPE_ID(LiulinLoopFusion)

mlir::PassPluginLibraryInfo getLiulinLoopFusionPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "LiulinLoopFusion", "1.0",
          []() { mlir::PassRegistration<LiulinLoopFusion>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getLiulinLoopFusionPluginInfo();
}