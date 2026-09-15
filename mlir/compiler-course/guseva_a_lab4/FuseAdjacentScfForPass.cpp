#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Operation.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace mlir;

namespace {

class FuseAdjacentScfForPass
    : public PassWrapper<FuseAdjacentScfForPass, OperationPass<ModuleOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FuseAdjacentScfForPass)

  StringRef getArgument() const final { return "FuseAdjacentScfForPass"; }

  StringRef getDescription() const final {
    return "Fuse two adjacent scf.for loops with identical bounds and no "
           "inter-loop data dependencies";
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool changed = true;

    while (changed) {
      changed = false;

      module.walk([&](Block *block) {
        if (changed)
          return WalkResult::interrupt();

        for (auto it = block->begin(), e = block->end(); it != e;) {
          Operation *firstOp = &*it++;
          auto firstFor = dyn_cast<scf::ForOp>(firstOp);
          if (!firstFor)
            continue;

          Operation *nextOp = firstFor->getNextNode();
          if (!nextOp)
            continue;

          auto secondFor = dyn_cast<scf::ForOp>(nextOp);
          if (!secondFor)
            continue;

          if (!canFuse(firstFor, secondFor))
            continue;

          fuse(firstFor, secondFor);
          changed = true;
          return WalkResult::interrupt();
        }

        return WalkResult::advance();
      });
    }
  }

private:
  bool equalValues(Value lhs, Value rhs) {
    if (lhs == rhs)
      return true;

    auto lhsConst = lhs.getDefiningOp<arith::ConstantIndexOp>();
    auto rhsConst = rhs.getDefiningOp<arith::ConstantIndexOp>();

    if (!lhsConst || !rhsConst)
      return false;

    return lhsConst.value() == rhsConst.value();
  }

  bool haveSameBounds(scf::ForOp firstFor, scf::ForOp secondFor) {
    return equalValues(firstFor.getLowerBound(), secondFor.getLowerBound()) &&
           equalValues(firstFor.getUpperBound(), secondFor.getUpperBound()) &&
           equalValues(firstFor.getStep(), secondFor.getStep());
  }

  bool hasIterArgs(scf::ForOp forOp) {
    return !forOp.getInitArgs().empty() || forOp.getNumResults() != 0;
  }

  bool isDefinedInside(Operation *container, Value value) {
    Operation *defOp = value.getDefiningOp();
    return defOp && container->isAncestor(defOp);
  }

  bool hasSSADependency(scf::ForOp firstFor, scf::ForOp secondFor) {
    bool hasDependency = false;

    secondFor.walk([&](Operation *op) {
      for (Value operand : op->getOperands()) {
        if (isDefinedInside(firstFor.getOperation(), operand)) {
          hasDependency = true;
          return WalkResult::interrupt();
        }
      }
      return WalkResult::advance();
    });

    return hasDependency;
  }

  void collectMemRefs(Operation *op, llvm::SmallPtrSetImpl<Value> &memrefs) {
    op->walk([&](Operation *nestedOp) {
      if (auto load = dyn_cast<memref::LoadOp>(nestedOp)) {
        memrefs.insert(load.getMemRef());
      } else if (auto store = dyn_cast<memref::StoreOp>(nestedOp)) {
        memrefs.insert(store.getMemRef());
      }
    });
  }

  bool hasMemoryDependency(scf::ForOp firstFor, scf::ForOp secondFor) {
    llvm::SmallPtrSet<Value, 8> firstMemrefs;
    llvm::SmallPtrSet<Value, 8> secondMemrefs;

    collectMemRefs(firstFor.getOperation(), firstMemrefs);
    collectMemRefs(secondFor.getOperation(), secondMemrefs);

    for (Value memref : firstMemrefs) {
      if (secondMemrefs.contains(memref))
        return true;
    }

    return false;
  }

  bool canFuse(scf::ForOp firstFor, scf::ForOp secondFor) {
    if (!haveSameBounds(firstFor, secondFor))
      return false;

    if (hasIterArgs(firstFor) || hasIterArgs(secondFor))
      return false;

    if (hasSSADependency(firstFor, secondFor))
      return false;

    if (hasMemoryDependency(firstFor, secondFor))
      return false;

    return true;
  }

  void fuse(scf::ForOp firstFor, scf::ForOp secondFor) {
    Block *firstBody = firstFor.getBody();
    Block *secondBody = secondFor.getBody();

    OpBuilder builder(firstFor.getContext());
    Operation *firstYield = firstBody->getTerminator();
    builder.setInsertionPoint(firstYield);

    IRMapping mapping;
    mapping.map(secondFor.getInductionVar(), firstFor.getInductionVar());

    for (Operation &op : secondBody->without_terminator()) {
      builder.clone(op, mapping);
    }

    secondFor.erase();
  }
};

} // namespace

mlir::PassPluginLibraryInfo getFuseAdjacentScfForPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "FuseAdjacentScfForPass", "1.0",
          []() { mlir::PassRegistration<FuseAdjacentScfForPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFuseAdjacentScfForPassPluginInfo();
}
