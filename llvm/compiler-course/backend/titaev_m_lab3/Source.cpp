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
    bool Changed = false;

    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &CallInst = *MI++;

          if (CallInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = findCallee(CallInst, MF);
          if (!Callee || !shouldInline(*Callee))
            continue;

          bool IsRecursive = (Callee == &MF);
          performInline(MBB, CallInst, *Callee, IsRecursive);

          LocalChanged = true;
          Changed = true;

          goto restart_function;
        }
      }
    restart_function:
      if (!LocalChanged)
        break;
    }

    return Changed;
  }

private:
  static constexpr unsigned MaxInstrs = 20;
  static constexpr unsigned MaxDepth = 3;

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller) {
    for (const MachineOperand &MO : MI.operands()) {
      const Function *F = nullptr;

      if (MO.isGlobal()) {
        F = dyn_cast_or_null<Function>(MO.getGlobal());
      } else if (MO.isSymbol()) {
        F = Caller.getFunction().getParent()->getFunction(MO.getSymbolName());
      }

      if (!F)
        continue;

      if (F->getName() == Caller.getName())
        return &Caller;

      if (MachineFunction *MF = Caller.getMMI().getMachineFunction(*F))
        return MF;
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
    SmallVector<MachineInstr *, 16> Body;

    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;
        Body.push_back(&CMI);
      }
    }

    for (auto *I : Body) {
      MachineInstr *Cloned = CallerMF.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    if (!IsRecursive) {
      CallInst.eraseFromParent();
    }
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);