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

    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++; // Инкрементируем заранее

          if (MIInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = findCallee(MIInst, MF, MMI);
          if (!Callee || !shouldInline(*Callee))
            continue;

          bool IsRecursive = (Callee == &MF);

          performInline(MBB, MIInst, *Callee, IsRecursive);

          LocalChanged = true;
          Changed = true;

          goto restart_search;
        }
      }

    restart_search:
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
      if (MO.isGlobal() && MO.getGlobal())
        Name = MO.getGlobal()->getName();
      else if (MO.isSymbol())
        Name = MO.getSymbolName();
      else
        continue;

      if (Name.empty())
        continue;

      // Рекурсия
      if (Name == Caller.getName())
        return &Caller;

      // Поиск в модуле
      const Module *M = Caller.getFunction().getParent();
      if (Function *F = M->getFunction(Name)) {
        return MMI.getMachineFunction(*F);
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

    SmallVector<MachineInstr *, 16> InstructionsToInline;
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;
        InstructionsToInline.push_back(&CMI);
      }
    }

    for (auto *I : InstructionsToInline) {
      MachineInstr *ClonedMI = CallerMF.CloneMachineInstr(I);
      MBB.insert(CallInst, ClonedMI);
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