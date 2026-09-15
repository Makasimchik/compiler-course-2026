#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace {

class GutyanskyACopyReplPass
    : public PassWrapper<GutyanskyACopyReplPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "gutyansky_a_lab4_MLIR"; }
  StringRef getDescription() const final {
    return "A pass that replaces memref.copy operations with scf.for loops "
           "performing element-wise copies using memref.load and memref.store "
           "operations";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, arith::ArithDialect, memref::MemRefDialect>();
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    moduleOp.walk([&](memref::CopyOp op) { replaceCopyWithLoopNest(op); });
  }

private:
  void replaceCopyWithLoopNest(memref::CopyOp op) {
    OpBuilder builder(op);

    Value opFrom = op.getSource();
    Value opTo = op.getTarget();
    Location loc = op.getLoc();

    MemRefType type = llvm::cast<MemRefType>(opFrom.getType());
    int64_t rank = type.getRank();
    ArrayRef<int64_t> shape = type.getShape();

    Value lb = builder.create<arith::ConstantIndexOp>(loc, 0);
    llvm::SmallVector<Value> lbs(rank, lb);

    Value step = builder.create<arith::ConstantIndexOp>(loc, 1);
    llvm::SmallVector<Value> steps(rank, step);

    llvm::SmallVector<Value> ubs;
    ubs.reserve(rank);
    for (int64_t i = 0; i < rank; ++i) {
      if (ShapedType::isDynamic(shape[i])) {
        Value dimIdx = builder.create<arith::ConstantIndexOp>(loc, i);
        ubs.push_back(builder.create<memref::DimOp>(loc, opFrom, dimIdx));
      } else {
        ubs.push_back(builder.create<arith::ConstantIndexOp>(loc, shape[i]));
      }
    }

    scf::buildLoopNest(
        builder, loc, lbs, ubs, steps,
        [&](OpBuilder &nestBuilder, Location nestLoc, ValueRange ivs) {
          Value loaded =
              nestBuilder.create<memref::LoadOp>(nestLoc, opFrom, ivs);
          nestBuilder.create<memref::StoreOp>(nestLoc, loaded, opTo, ivs);
        });

    op.erase();
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(GutyanskyACopyReplPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(GutyanskyACopyReplPass)

static mlir::PassPluginLibraryInfo getGutyanskyACopyReplPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "GutyanskyACopyReplPass", "1.0",
          []() { mlir::PassRegistration<GutyanskyACopyReplPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getGutyanskyACopyReplPassPluginInfo();
}