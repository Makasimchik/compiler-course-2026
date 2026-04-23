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
// Глобальный реестр для поиска функций в рамках одного модуля MIR
static std::map<std::string, MachineFunction *> GlobalRegistry;

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    // Сохраняем текущую функцию в реестр, чтобы её могли найти другие
    GlobalRegistry[MF.getName().str()] = &MF;

    bool Changed = false;
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

    // Глубина встраивания (3 уровня)
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++; // Инкремент итератора до модификации

          if (MIInst.isCall()) {
            MachineFunction *Callee = findCallee(MIInst, MF, MMI);

            if (Callee && shouldInline(*Callee)) {
              performInline(MBB, MIInst, *Callee);
              LocalChanged = true;
              Changed = true;
              // После встраивания перезапускаем проверку для текущей глубины
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

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (const MachineOperand &MO : MI.operands()) {
      StringRef Name;
      if (MO.isGlobal() && MO.getGlobal()) {
        Name = MO.getGlobal()->getName();
      } else if (MO.isSymbol()) {
        Name = MO.getSymbolName();
      }

      if (!Name.empty()) {
        std::string SName = Name.str();
        // 1. Рекурсия
        if (SName == Caller.getName())
          return &Caller;
        // 2. Поиск в нашем реестре
        if (GlobalRegistry.count(SName))
          return GlobalRegistry[SName];
        // 3. Поиск через MMI
        auto &M = *Caller.getFunction().getParent();
        if (auto *F = M.getFunction(Name)) {
          MachineFunction *Target = MMI.getMachineFunction(*F);
          if (Target)
            return Target;
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
    SmallVector<MachineInstr *, 16> ToClone;

    // Собираем инструкции во временный список (критично для рекурсии)
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (!CMI.isTerminator()) {
          ToClone.push_back(&CMI);
        }
      }
    }

    for (auto *I : ToClone) {
      MachineInstr *Cloned = Caller.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // Удаляем вызов
    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);