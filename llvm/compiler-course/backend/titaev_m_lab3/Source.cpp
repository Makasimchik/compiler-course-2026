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
    unsigned Depth = 0;

    while (Depth < MaxDepth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &Instr = *MI++;

          if (Instr.isCall()) {
            MachineFunction *Callee = getCallee(Instr, MF, MMI);

            if (Callee && shouldInline(*Callee)) {
              performInline(MBB, Instr, *Callee);
              LocalChanged = true;
              GlobalChanged = true;
              break;
            }
          }
        }
        if (LocalChanged)
          break;
      }

      if (!LocalChanged)
        break;
      Depth++;
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
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;

        MachineInstr *Cloned = Caller.CloneMachineInstr(&CMI);
        MBB.insert(CallInst, Cloned);
      }
    }
    // Удаляем оригинальный вызов
    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);