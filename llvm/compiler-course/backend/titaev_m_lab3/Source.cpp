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
// Реестр имен для поиска функций в рамках одного прохода компилятора
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
    // Регистрируем текущую функцию, чтобы её могли найти другие (для
    // инлайнинга)
    FunctionRegistry[MF.getName().str()] = &MF;

    bool Changed = false;
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

    // Цикл по глубине (ограничение 3 уровня)
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++; // Безопасный инкремент итератора

          if (MIInst.isCall()) {
            MachineFunction *Callee = findCallee(MIInst, MF, MMI);

            if (Callee && shouldInline(*Callee)) {
              performInline(MBB, MIInst, *Callee);
              LocalChanged = true;
              Changed = true;
              // После модификации блока итераторы MI невалидны, начинаем заново
              goto restart;
            }
          }
        }
      }
    restart:
      if (!LocalChanged)
        break;
    }

    return Changed;
  }

private:
  static constexpr unsigned MaxInstrs = 15;
  static constexpr unsigned MaxDepth = 3;

  // Ищем калли через реестр имен, MMI и модуль
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
        // 1. Проверка на рекурсию
        if (Name == Caller.getName())
          return &Caller;
        // 2. Поиск в реестре обработанных функций
        if (FunctionRegistry.count(Name))
          return FunctionRegistry[Name];
        // 3. Поиск через MMI и Module
        auto &M = *Caller.getFunction().getParent();
        if (auto *F = M.getFunction(Name)) {
          MachineFunction *MF = MMI.getMachineFunction(*F);
          if (MF)
            return MF;
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
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &Caller = *MBB.getParent();
    SmallVector<MachineInstr *, 16> Body;

    // Собираем инструкции тела (без возвратов)
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;
        Body.push_back(&CMI);
      }
    }

    // Вставляем клонированные инструкции перед CALL
    for (auto *I : Body) {
      MachineInstr *Cloned = Caller.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // Удаляем сам вызов
    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);