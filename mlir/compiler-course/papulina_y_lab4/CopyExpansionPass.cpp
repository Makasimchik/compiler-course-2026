#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

struct CopyExpansionPass
    : public PassWrapper<CopyExpansionPass, OperationPass<ModuleOp>> {

  StringRef getArgument() const final { return "replace-memref-copy"; }
  StringRef getDescription() const final {
    return "Expand memref.copy into explicit scf.for loops";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, arith::ArithDialect, memref::MemRefDialect>();
  }
  void runOnOperation() override {
    auto module = getOperation();
    IRRewriter rewriter(module.getContext());
    module.walk([&](memref::CopyOp op) {
      if (failed(expandCopyOp(op, rewriter))) {
        signalPassFailure();
      }
    });
  }

  LogicalResult expandCopyOp(memref::CopyOp op, IRRewriter &rewriter) {
    auto source = op.getSource();
    auto target = op.getTarget();
    auto loc = op.getLoc();
    auto type = llvm::cast<MemRefType>(source.getType());
    auto shape = type.getShape();
    int64_t rank = type.getRank();

    rewriter.setInsertionPoint(op);

    auto zero = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    auto step = rewriter.create<arith::ConstantIndexOp>(loc, 1);

    SmallVector<Value> lowerBounds(rank, zero);
    SmallVector<Value> steps(rank, step);
    SmallVector<Value> upperBounds;
    upperBounds.reserve(rank);
    for (auto dim : shape) {
      upperBounds.push_back(rewriter.create<arith::ConstantIndexOp>(loc, dim));
    }
    scf::buildLoopNest(
        rewriter, loc, lowerBounds, upperBounds, steps,
        [&](OpBuilder &nestedBuilder, Location nestedLoc, ValueRange ivs) {
          auto pixel =
              nestedBuilder.create<memref::LoadOp>(nestedLoc, source, ivs);
          nestedBuilder.create<memref::StoreOp>(nestedLoc, pixel, target, ivs);
        });

    rewriter.eraseOp(op);
    return success();
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(CopyExpansionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(CopyExpansionPass)

mlir::PassPluginLibraryInfo getCopyExpansionPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "CopyExpansionPass", "0.1",
          []() { mlir::PassRegistration<CopyExpansionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getCopyExpansionPluginInfo();
}