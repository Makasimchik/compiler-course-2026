#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {
class MyLoopingPass
    : public PassWrapper<MyLoopingPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "kiselev-cycle-merging"; }
  StringRef getDescription() const final {
    return "My plugin with cycle merging";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    module.walk([&](func::FuncOp func) {
      for (Block &block : func) {
        for (auto it = block.begin(); it != block.end();) {
          auto firstLoop = dyn_cast<scf::ForOp>(*it);
          if (!firstLoop) {
            ++it;
            continue;
          }
          auto nextIt = std::next(it);
          if (nextIt == block.end())
            break;
          auto secondLoop = dyn_cast<scf::ForOp>(*nextIt);
          if (!secondLoop) {
            ++it;
            continue;
          }
          if (firstLoop.getLowerBound() != secondLoop.getLowerBound() ||
              firstLoop.getUpperBound() != secondLoop.getUpperBound() ||
              firstLoop.getStep() != secondLoop.getStep()) {
            ++it;
            continue;
          }

          bool flagDeps = false;

          firstLoop.getBody()->walk([&](Operation *op) {
            for (auto result : op->getResults()) {
              for (auto user : result.getUsers()) {
                if (secondLoop->isAncestor(user)) {
                  flagDeps = true;
                }
              }
            }
          });

          bool flagSide = false;

          firstLoop.getBody()->walk([&](Operation *op) {
            if (auto iface = dyn_cast<MemoryEffectOpInterface>(op)) {
              if (!iface.hasNoEffect()) {
                flagSide = true;
              }
            } else {
              flagSide = true;
            }
          });

          if (flagDeps || flagSide) {
            ++it;
            continue;
          }

          OpBuilder builder(firstLoop);
          auto fusedLoop = builder.create<scf::ForOp>(
              firstLoop.getLoc(), firstLoop.getLowerBound(),
              firstLoop.getUpperBound(), firstLoop.getStep());

          Block *fusedBody = fusedLoop.getBody();
          Block *firstBody = firstLoop.getBody();
          Block *secondBody = secondLoop.getBody();

          IRMapping mapping; // клон операций циклов
          mapping.map(firstLoop.getInductionVar(), fusedLoop.getInductionVar());
          mapping.map(secondLoop.getInductionVar(),
                      fusedLoop.getInductionVar());

          OpBuilder bodyBuilder(fusedBody, fusedBody->begin());
          for (auto &op : firstBody->without_terminator()) {
            bodyBuilder.clone(op, mapping);
          }
          for (auto &op : secondBody->without_terminator()) {
            bodyBuilder.clone(op, mapping);
          }

          secondLoop.erase();
          firstLoop.erase();

          it = fusedLoop->getIterator();
          ++it;
        }
      }
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(MyLoopingPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(MyLoopingPass)

mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "KiselevILoopingPass", "1.0",
          []() { mlir::PassRegistration<MyLoopingPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
