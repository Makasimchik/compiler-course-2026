#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace {
class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    bool Changed = false;

    // Внешний цикл для соблюдения глубины рекурсии (3 уровня)
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++; // Заранее инкрементируем итератор

          if (MIInst.isCall()) {
            MachineFunction *Callee = getCallee(MIInst, MF, MMI);

            if (Callee && shouldInline(*Callee)) {
              performInline(MBB, MIInst, *Callee);
              LocalChanged = true;
              Changed = true;
              // После встраивания в текущий блок, итераторы MI могут стать
              // невалидными. Проще всего начать обработку MF заново для этой
              // глубины.
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

  MachineFunction *getCallee(MachineInstr &MI, MachineFunction &Caller,
                             MachineModuleInfo &MMI) {
    for (auto &MO : MI.operands()) {
      // Случай 1: GlobalAddress (ссылка на IR функцию)
      if (MO.isGlobal()) {
        if (auto *F = dyn_cast<Function>(MO.getGlobal())) {
          if (F->getName() == Caller.getName())
            return &Caller;
          return MMI.getMachineFunction(*F);
        }
      }
      // Случай 2: Имя символа (часто встречается в MIR)
      if (MO.isSymbol()) {
        StringRef Name = MO.getSymbolName();
        if (Name == Caller.getName())
          return &Caller;

        auto &M = *Caller.getFunction().getParent();
        if (auto *F = M.getFunction(Name))
          return MMI.getMachineFunction(*F);
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
    // Функция должна содержать полезный код и не превышать лимит инструкций
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &Caller = *MBB.getParent();
    SmallVector<MachineInstr *, 16> ToClone;

    // Собираем все инструкции, кроме терминаторов (RET/JMP)
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (!CMI.isTerminator()) {
          ToClone.push_back(&CMI);
        }
      }
    }

    // Вставляем клонированные инструкции перед CALL
    for (auto *I : ToClone) {
      MachineInstr *Cloned = Caller.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // Удаляем оригинальную инструкцию CALL
    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);