#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Module.h"
#include <map>
#include <string>

using namespace llvm;

namespace {
// Статический реестр для надежного поиска функций по именам в рамках одного
// модуля
static std::map<std::string, MachineFunction *> FunctionRegistry;

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    // Регистрируем функцию в глобальном реестре (позволяет видеть её при
    // обработке других функций)
    FunctionRegistry[MF.getName().str()] = &MF;

    bool Changed = false;
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

    // Глубина встраивания (3 уровня)
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &CallMI = *MI++; // Заранее сдвигаем итератор

          if (CallMI.isCall()) {
            MachineFunction *Callee = findCallee(CallMI, MF, MMI);

            if (Callee && shouldInline(*Callee)) {
              performInline(MBB, CallMI, *Callee);
              LocalChanged = true;
              Changed = true;
              // После модификации блока лучше перезапустить сканирование
              // функции
              goto start_over;
            }
          }
        }
      }
    start_over:
      if (!LocalChanged)
        break;
    }

    return Changed;
  }

private:
  static constexpr unsigned MaxInstrs = 15;
  static constexpr unsigned MaxDepth = 3;

  // Тщательный поиск вызываемой функции
  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (const MachineOperand &MO : MI.operands()) {
      std::string Name = "";
      if (MO.isGlobal() && MO.getGlobal()) {
        Name = MO.getGlobal()->getName().str();
      } else if (MO.isSymbol()) {
        Name = MO.getSymbolName();
      }

      if (!Name.empty()) {
        // 1. Проверяем рекурсию
        if (Name == Caller.getName())
          return &Caller;
        // 2. Проверяем реестр (самый надежный способ для lit-тестов)
        if (FunctionRegistry.count(Name))
          return FunctionRegistry[Name];
        // 3. Стандартный поиск через MMI
        auto &M = *Caller.getFunction().getParent();
        if (auto *F = M.getFunction(Name)) {
          MachineFunction *TargetMF = MMI.getMachineFunction(*F);
          if (TargetMF)
            return TargetMF;
        }
      }
    }
    return nullptr;
  }

  bool shouldInline(MachineFunction &Callee) {
    unsigned Count = 0;
    for (auto &MBB : Callee) {
      for (auto &MI : MBB) {
        if (!MI.isTerminator())
          Count++;
      }
    }
    // Лимит: функция не пустая и не более 15 инструкций
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &Caller = *MBB.getParent();
    SmallVector<MachineInstr *, 16> ToClone;

    // Собираем инструкции во временный список (чтобы не испортить итераторы при
    // рекурсии)
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (!CMI.isTerminator()) {
          ToClone.push_back(&CMI);
        }
      }
    }

    // Копируем инструкции в место вызова
    for (auto *I : ToClone) {
      MachineInstr *Cloned = Caller.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // Удаляем CALL
    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);