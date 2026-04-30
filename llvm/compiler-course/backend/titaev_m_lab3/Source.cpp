#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
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

    // Проход по глубине для обработки вложенного инлайнинга
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++;

          // Ищем только X86 вызовы
          if (MIInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = findCallee(MIInst, MF, MMI);
          if (!Callee || !shouldInline(*Callee))
            continue;

          performInline(MBB, MIInst, *Callee);
          LocalChanged = true;
          Changed = true;

          // Безопасный выход из циклов для перезапуска поиска в измененной MF
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
  static constexpr unsigned MaxInstrs = 25;
  static constexpr unsigned MaxDepth = 3;

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (const MachineOperand &MO : MI.operands()) {
      StringRef Name;
      if (MO.isGlobal() && MO.getGlobal()) {
        Name = MO.getGlobal()->getName();
      } else if (MO.isSymbol()) {
        Name = MO.getSymbolName();
      } else {
        continue;
      }

      if (Name.empty())
        continue;

      // Обработка рекурсии
      if (Name == Caller.getName())
        return &Caller;

      // Поиск MachineFunction в модуле через MMI
      const Module *M = Caller.getFunction().getParent();
      if (const Function *F = M->getFunction(Name)) {
        if (MachineFunction *TargetMF = MMI.getMachineFunction(*F))
          return TargetMF;
      }
    }
    return nullptr;
  }

  bool shouldInline(MachineFunction &Callee) {
    unsigned Count = 0;
    for (auto &MBB : Callee) {
      for (auto &MI : MBB) {
        // Не считаем возвраты и служебные инструкции
        if (MI.isReturn() || MI.isTerminator() || MI.isImplicitDef() ||
            MI.isDebugInstr())
          continue;
        Count++;
      }
    }
    // ВАЖНО: Count > 0, так как функции могут быть очень короткими
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &CallerMF = *MBB.getParent();
    bool IsRecursive = (&Callee == &CallerMF);

    SmallVector<MachineInstr *, 16> Body;
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;
        Body.push_back(&CMI);
      }
    }

    // Вставка тела функции перед инструкцией вызова
    for (auto *I : Body) {
      MachineInstr *Cloned = CallerMF.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // Если это не рекурсия, удаляем сам вызов
    if (!IsRecursive) {
      CallInst.eraseFromParent();
    }
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);