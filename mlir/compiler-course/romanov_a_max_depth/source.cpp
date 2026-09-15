#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Visitors.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/DenseMap.h"

using namespace mlir;

namespace {

static bool isDepthIncreasingOp(Operation *op) {
  return isa<scf::IfOp, scf::ForOp, scf::WhileOp, scf::ParallelOp,
             affine::AffineForOp, affine::AffineIfOp, affine::AffineParallelOp>(
      op);
}

class MaxDepthCountPass
    : public PassWrapper<MaxDepthCountPass, OperationPass<func::FuncOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MaxDepthCountPass)

  StringRef getArgument() const final { return "MaxDepthCountPass"; }
  StringRef getDescription() const final {
    return "Find the max depth of nested loops and if blocks in each function";
  }

  void runOnOperation() override {
    func::FuncOp func_op = getOperation();
    Operation *func_raw = func_op.getOperation();
    int64_t max_depth = 0;

    llvm::DenseMap<Operation *, int64_t> ops_depths;

    func_op.walk<WalkOrder::PreOrder>([&](Operation *op) {
      if (!isDepthIncreasingOp(op)) {
        return;
      }

      int64_t parent_depth = 0;
      for (Operation *parent = op->getParentOp(); parent && parent != func_raw;
           parent = parent->getParentOp()) {
        if (isDepthIncreasingOp(parent)) {
          parent_depth = ops_depths.lookup(parent);
          break;
        }
      }

      int64_t current_depth = parent_depth + 1;
      ops_depths[op] = current_depth;

      if (current_depth > max_depth) {
        max_depth = current_depth;
      }
    });

    func_op->setAttr(
        "max_depth",
        IntegerAttr::get(IntegerType::get(func_op.getContext(), 64),
                         max_depth));
  }
};

} // namespace

mlir::PassPluginLibraryInfo getMaxDepthCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "MaxDepthCountPass", "1.0",
          []() { mlir::PassRegistration<MaxDepthCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getMaxDepthCountPassPluginInfo();
}