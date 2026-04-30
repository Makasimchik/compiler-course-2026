#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

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

    // Глубина инлайнинга для обработки рекурсии и вложенности
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++;

          if (MIInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = findCallee(MIInst, MF, MMI);
          if (!Callee || !shouldInline(*Callee))
            continue;

          bool IsRecursive = (Callee == &MF);
          performInline(MBB, MIInst, *Callee, IsRecursive);

          LocalChanged = true;
          Changed = true;

          // Перезапускаем поиск в текущей функции, так как структура изменилась
          goto restart;
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

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (const MachineOperand &MO : MI.operands()) {
      // 1. Попытка найти через GlobalAddress (самый частый случай для CALL)
      if (MO.isGlobal()) {
        if (auto *F = dyn_cast_or_null<Function>(MO.getGlobal())) {
          if (F->getName() == Caller.getName())
            return &Caller;
          if (auto *TargetMF = MMI.getMachineFunction(*F))
            return TargetMF;
        }
      }
      // 2. Попытка найти через имя символа (если это ExternalSymbol)
      if (MO.isSymbol()) {
        StringRef Name = MO.getSymbolName();
        if (Name == Caller.getName())
          return &Caller;

        Module &M = const_cast<Module &>(*Caller.getFunction().getParent());
        if (auto *F = M.getFunction(Name)) {
          if (auto *TargetMF = MMI.getMachineFunction(*F))
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
        if (!MI.isReturn() && !MI.isTerminator())
          Count++;
      }
    }
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee, bool IsRecursive) {
    MachineFunction &CallerMF = *MBB.getParent();
    SmallVector<MachineInstr *, 16> ToInline;

    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;
        ToInline.push_back(&CMI);
      }
    }

    // Вставляем инструкции ПЕРЕД вызовом
    for (auto *I : ToInline) {
      MachineInstr *Cloned = CallerMF.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // Удаляем вызов, если это не рекурсия (в рекурсии мы просто развернули тело
    // N раз)
    if (!IsRecursive) {
      CallInst.eraseFromParent();
    }
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);