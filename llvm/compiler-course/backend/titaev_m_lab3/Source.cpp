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

  // Обязательно объявляем использование MachineModuleInfo
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    return inlineInFunction(MF, 0);
  }

private:
  static constexpr unsigned MaxInstrs = 15;
  static constexpr unsigned MaxDepth = 3;

  bool inlineInFunction(MachineFunction &MF, unsigned Depth) {
    if (Depth >= MaxDepth)
      return false;

    // Получаем MMI из анализа
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
        if (MI->isCall()) {
          MachineFunction *Callee = getCallee(*MI, MF, MMI);
          if (Callee && shouldInline(*Callee)) {
            performInline(MBB, MI, *Callee);
            Changed = true;
            inlineInFunction(MF, Depth + 1);
            return true;
          }
        }
      }
    }
    return Changed;
  }

  MachineFunction *getCallee(MachineInstr &MI, MachineFunction &Caller,
                             MachineModuleInfo &MMI) {
    for (auto &MO : MI.operands()) {
      if (MO.isGlobal()) {
        if (auto *F = dyn_cast<Function>(MO.getGlobal()))
          return MMI.getMachineFunction(*F); // Используем переданный MMI
      }
      if (MO.isSymbol()) {
        const char *Sym = MO.getSymbolName();
        auto &M = *Caller.getFunction().getParent();
        if (auto *F = M.getFunction(Sym))
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

  void performInline(MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator &CallPos,
                     MachineFunction &Callee) {
    MachineFunction &Caller = *MBB.getParent();
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;

        MachineInstr *Cloned = Caller.CloneMachineInstr(&CMI);
        MBB.insert(CallPos, Cloned);
      }
    }
    CallPos->eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);