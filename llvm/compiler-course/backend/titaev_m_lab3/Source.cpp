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
          MachineInstr &MIInst = *MI++;

          if (MIInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = findCallee(MIInst, MF, MMI);
          if (!Callee || !shouldInline(*Callee))
            continue;

          performInline(MBB, MIInst, *Callee);
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
  static constexpr unsigned MaxInstrs = 25;
  static constexpr unsigned MaxDepth = 3;

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (const MachineOperand &MO : MI.operands()) {
      StringRef Name;
      if (MO.isGlobal())
        Name = MO.getGlobal()->getName();
      else if (MO.isSymbol())
        Name = MO.getSymbolName();
      else
        continue;

      if (Name.empty())
        continue;
      if (Name == Caller.getName())
        return &Caller;

      // Пытаемся найти через MMI (самый надежный способ в Backend)
      if (const Function *F =
              Caller.getFunction().getParent()->getFunction(Name)) {
        if (MachineFunction *MF = MMI.getMachineFunction(*F))
          return MF;
      }
    }
    return nullptr;
  }

  bool shouldInline(MachineFunction &Callee) {
    unsigned Count = 0;
    for (auto &MBB : Callee)
      Count += MBB.size();
    // 1 инструкция возврата + само тело.
    return Count > 1 && Count <= MaxInstrs;
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

    for (auto *I : Body) {
      MachineInstr *Cloned = CallerMF.CloneMachineInstr(I);
      MBB.insert(CallInst, Cloned);
    }

    // В случае рекурсии мы не удаляем вызов, чтобы не уйти в бесконечный цикл,
    // а просто разворачиваем тело перед ним. Для обычной функции - удаляем.
    if (!IsRecursive) {
      CallInst.eraseFromParent();
    }
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);