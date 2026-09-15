#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

class SavvaDariyaCopyToLoopPass
    : public PassWrapper<SavvaDariyaCopyToLoopPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "savva-copy-to-loop"; }

  StringRef getDescription() const final {
    return "Replace memref.copy with explicit loop-based element copy";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<scf::SCFDialect, memref::MemRefDialect, arith::ArithDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    IRRewriter rewriter(module.getContext());

    SmallVector<memref::CopyOp> copies;

    // 1. Собираем все copy (чтобы безопасно модифицировать IR)
    module.walk([&](memref::CopyOp op) { copies.push_back(op); });

    // 2. Обрабатываем каждую
    for (auto copyOp : copies) {
      lowerCopy(copyOp, rewriter);
    }
  }

private:
  void lowerCopy(memref::CopyOp op, IRRewriter &rewriter) {
    Location loc = op.getLoc();

    Value src = op.getSource();
    Value dst = op.getTarget();

    auto memType = cast<MemRefType>(src.getType());
    int rank = memType.getRank();

    rewriter.setInsertionPoint(op);

    // Константы
    Value zero = rewriter.create<arith::ConstantIndexOp>(loc, 0);
    Value one = rewriter.create<arith::ConstantIndexOp>(loc, 1);

    SmallVector<Value> lowers(rank, zero);
    SmallVector<Value> steps(rank, one);
    SmallVector<Value> uppers;

    // Формируем upper bounds
    for (int i = 0; i < rank; ++i) {
      int64_t dim = memType.getDimSize(i);

      if (dim == ShapedType::kDynamic) {
        Value dynSize = rewriter.create<memref::DimOp>(loc, src, i);
        uppers.push_back(dynSize);
      } else {
        uppers.push_back(rewriter.create<arith::ConstantIndexOp>(loc, dim));
      }
    }

    // 3. Строим вложенные циклы
    scf::buildLoopNest(
        rewriter, loc, lowers, uppers, steps,
        [&](OpBuilder &nestedBuilder, Location nestedLoc, ValueRange ivs) {
          Value val = nestedBuilder.create<memref::LoadOp>(nestedLoc, src, ivs);

          nestedBuilder.create<memref::StoreOp>(nestedLoc, val, dst, ivs);
        });

    // 4. Удаляем исходный copy
    rewriter.eraseOp(op);
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(SavvaDariyaCopyToLoopPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(SavvaDariyaCopyToLoopPass)

mlir::PassPluginLibraryInfo getSavvaCopyPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "SavvaDariyaCopyToLoopPass", "1.0",
          []() { mlir::PassRegistration<SavvaDariyaCopyToLoopPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getSavvaCopyPassPluginInfo();
}