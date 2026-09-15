#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
// Класс пасса для подсчёта вызовов функций
class FunctionCallCounterPass
    : public PassWrapper<FunctionCallCounterPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "FunctionCallCounterPass"; }
  StringRef getDescription() const final {
    return "Counts amount of times it was called by other functions "
           "(func.func) in the module";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    // Словарь для хранения количества вызовов для каждой функции
    // Ключ - имя функции, значение - счётчик вызовов
    llvm::StringMap<int> callCounts;

    llvm::StringMap<bool> functionNames;

    // Собираем все имена функций в модуле
    moduleOp.walk([&](func::FuncOp funcOp) {
      std::string funcName = funcOp.getName().str();
      functionNames[funcName] = true;
      callCounts[funcName] = 0;
    });

    // Подсчитываем все вызовы функций
    // Проходим по всем операциям вызова в модуле
    moduleOp.walk([&](func::CallOp callOp) {
      std::string calleeName = callOp.getCallee().str();
      if (functionNames.count(calleeName)) {
        callCounts[calleeName]++;
      }
    });

    // Добавляем атрибут call_count к каждой функции
    moduleOp.walk([&](func::FuncOp funcOp) {
      std::string funcName = funcOp.getName().str();
      int count = callCounts[funcName];

      IntegerAttr call_count =
          IntegerAttr::get(IntegerType::get(funcOp.getContext(), 32), count);

      funcOp->setAttr("call_count", call_count);
    });

    // Подсчитываем общее количество операций в модуле
    int totalOps = 0;
    moduleOp.walk([&](Operation *op) { totalOps++; });
    llvm::outs() << "Количество операций: " << totalOps << '\n';
  }
};
} // namespace

// Объявляем и определяем явный идентификатор типа для пасса
MLIR_DECLARE_EXPLICIT_TYPE_ID(FunctionCallCounterPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(FunctionCallCounterPass)

// Функция, возвращающая информацию о плагине пасса
mlir::PassPluginLibraryInfo getFunctionCallCounterPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "FunctionCallCounterPass", "1.0",
          []() { mlir::PassRegistration<FunctionCallCounterPass>(); }};
}

// Внешняя функция, необходимая для загрузки плагина в MLIR
extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getFunctionCallCounterPassPluginInfo();
}
