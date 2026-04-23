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
// Статический кэш для хранения указателей на MachineFunction в рамках одного
// запуска llc
static std::map<std::string, MachineFunction *> MFCache;

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    // Сохраняем текущую функцию в кэш, чтобы другие могли её встроить
    MFCache[MF.getName().str()] = &MF;

    bool Changed = false;
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++;

          if (MIInst.isCall()) {
            MachineFunction *Callee = findCallee(MIInst, MF, MMI);

            if (Callee && shouldInline(*Callee)) {
              performInline(MBB, MIInst, *Callee);
              LocalChanged = true;
              Changed = true;
              // Сбрасываем итераторы для текущей глубины
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

  // Улучшенный поиск функции через реестр и MMI
  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (auto &MO : MI.operands()) {
      StringRef Name;
      if (MO.isGlobal()) {
        if (auto *F = dyn_cast<Function>(MO.getGlobal()))
          Name = F->getName();
      } else if (MO.isSymbol()) {
        Name = MO.getSymbolName();
      }

      if (!Name.empty()) {
        // 1. Проверяем на рекурсию
        if (Name == Caller.getName())
          return &Caller;
        // 2. Проверяем наш кэш (для функций из того же MIR файла)
        if (MFCache.count(Name.str()))
          return MFCache[Name.str()];
        // 3. Пробуем стандартный MMI
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
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &Caller = *MBB.getParent();
    SmallVector<MachineInstr *, 16> ToClone;

    // Собираем инструкции во временный список (важно для рекурсии!)
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (!CMI.isTerminator())
          ToClone.push_back(&CMI);
      }
    }

    for (auto *I : ToClone) {
      MachineInstr *Cloned = Caller.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);