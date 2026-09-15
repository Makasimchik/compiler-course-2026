#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

using namespace mlir;

namespace {

static func::FuncOp getOrInsertTraceFunction(ModuleOp moduleOp,
                                             StringRef name) {
  if (auto func = moduleOp.lookupSymbol<func::FuncOp>(name))
    return func;

  OpBuilder builder(moduleOp.getContext());
  builder.setInsertionPointToStart(&moduleOp.getBodyRegion().front());

  auto funcType = builder.getFunctionType({}, {});
  auto func = builder.create<func::FuncOp>(moduleOp.getLoc(), name, funcType);
  func.setPrivate();

  return func;
}

static void insertCallAtRegionBegin(Region &region, StringRef callee,
                                    Location loc, OpBuilder &builder) {
  if (region.empty())
    return;

  Block &block = region.front();
  builder.setInsertionPointToStart(&block);
  builder.create<func::CallOp>(loc, callee, TypeRange{}, ValueRange{});
}

static void insertCallAtRegionEnd(Region &region, StringRef callee,
                                  Location loc, OpBuilder &builder) {
  if (region.empty())
    return;

  Block &block = region.front();
  Operation *terminator = block.getTerminator();

  if (terminator)
    builder.setInsertionPoint(terminator);
  else
    builder.setInsertionPointToEnd(&block);

  builder.create<func::CallOp>(loc, callee, TypeRange{}, ValueRange{});
}

static void instrumentRegion(Region &region, StringRef beginFunc,
                             StringRef endFunc, Location loc,
                             OpBuilder &builder) {
  if (region.empty())
    return;

  insertCallAtRegionBegin(region, beginFunc, loc, builder);
  insertCallAtRegionEnd(region, endFunc, loc, builder);
}

class ExamplePass : public PassWrapper<ExamplePass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "fatehov_k_lab4"; }

  StringRef getDescription() const final {
    return "Insert trace calls at begin/end of conditional regions";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry
        .insert<affine::AffineDialect, func::FuncDialect, scf::SCFDialect>();
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    OpBuilder builder(moduleOp.getContext());

    constexpr StringLiteral thenBeginName = "trace_condition_then_begin";
    constexpr StringLiteral thenEndName = "trace_condition_then_end";
    constexpr StringLiteral elseBeginName = "trace_condition_else_begin";
    constexpr StringLiteral elseEndName = "trace_condition_else_end";

    getOrInsertTraceFunction(moduleOp, elseEndName);
    getOrInsertTraceFunction(moduleOp, elseBeginName);
    getOrInsertTraceFunction(moduleOp, thenEndName);
    getOrInsertTraceFunction(moduleOp, thenBeginName);

    moduleOp.walk([&](scf::IfOp ifOp) {
      instrumentRegion(ifOp.getThenRegion(), thenBeginName, thenEndName,
                       ifOp.getLoc(), builder);

      Region &elseRegion = ifOp.getElseRegion();
      if (!elseRegion.empty()) {
        instrumentRegion(elseRegion, elseBeginName, elseEndName, ifOp.getLoc(),
                         builder);
      }
    });

    moduleOp.walk([&](affine::AffineIfOp ifOp) {
      instrumentRegion(ifOp.getThenRegion(), thenBeginName, thenEndName,
                       ifOp.getLoc(), builder);

      Region &elseRegion = ifOp.getElseRegion();
      if (!elseRegion.empty()) {
        instrumentRegion(elseRegion, elseBeginName, elseEndName, ifOp.getLoc(),
                         builder);
      }
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(ExamplePass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(ExamplePass)

mlir::PassPluginLibraryInfo getTraceConditionPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "ExamplePass", "1.0",
          []() { mlir::PassRegistration<ExamplePass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTraceConditionPassPluginInfo();
}
