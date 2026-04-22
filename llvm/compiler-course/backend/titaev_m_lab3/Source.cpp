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
    bool GlobalChanged = false;

    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;
      SmallVector<MachineInstr *, 8> CallsToInline;

      // 1. Сначала находим все вызовы, которые подходят под условия
      for (auto &MBB : MF) {
        for (auto &MI : MBB) {
          if (MI.isCall()) {
            MachineFunction *Callee = getCallee(MI, MF, MMI);
            if (Callee && shouldInline(*Callee)) {
              CallsToInline.push_back(&MI);
            }
          }
        }
      }

      // 2. Встраиваем найденные вызовы
      for (MachineInstr *CI : CallsToInline) {
        MachineFunction *Callee = getCallee(*CI, MF, MMI);
        if (Callee) {
          performInline(*CI->getParent(), *CI, *Callee);
          LocalChanged = true;
          GlobalChanged = true;
        }
      }

      if (!LocalChanged)
        break;
    }

    return GlobalChanged;
  }

private:
  static constexpr unsigned MaxInstrs = 15;
  static constexpr unsigned MaxDepth = 3;

  MachineFunction *getCallee(MachineInstr &MI, MachineFunction &Caller,
                             MachineModuleInfo &MMI) {
    for (auto &MO : MI.operands()) {
      if (MO.isGlobal()) {
        if (auto *F = dyn_cast<Function>(MO.getGlobal()))
          return MMI.getMachineFunction(*F);
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

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &Caller = *MBB.getParent();
    SmallVector<MachineInstr *, 16> InstsToClone;

    // Сначала собираем инструкции в список, чтобы избежать проблем
    // при само-встраивании (рекурсии)
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (!CMI.isReturn() && !CMI.isTerminator()) {
          InstsToClone.push_back(&CMI);
        }
      }
    }

    for (auto *Inst : InstsToClone) {
      MachineInstr *Cloned = Caller.CloneMachineInstr(Inst);
      MBB.insert(CallInst, Cloned);
    }

    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);