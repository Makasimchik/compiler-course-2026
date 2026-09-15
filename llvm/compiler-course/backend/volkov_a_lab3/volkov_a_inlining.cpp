#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace {
constexpr unsigned MaxInsts = 15;

unsigned getIRSize(const Function *F) {
  unsigned Cnt = 0;
  for (const auto &BB : *F) {
    Cnt += BB.size();
  }
  return Cnt;
}

bool checkRecursion(const Function *F) {
  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      if (const auto *Call = dyn_cast<CallBase>(&I)) {
        if (Call->getCalledFunction() == F) {
          return true;
        }
      }
    }
  }
  return false;
}
} // namespace

namespace {

class VolkovInliner : public MachineFunctionPass {
public:
  static char ID;
  VolkovInliner() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    SmallVector<MachineInstr *, 8> ToDelete;

    for (auto &MBB : MF) {
      for (auto &MI : MBB) {
        if (MI.isCall() && canBeInlined(MI)) {
          ToDelete.push_back(&MI);
        }
      }
    }

    for (auto *CallInstr : ToDelete) {
      CallInstr->eraseFromParent();
    }

    return !ToDelete.empty();
  }

private:
  bool canBeInlined(MachineInstr &CallInst) {
    const Function *FuncToCall = nullptr;

    for (const auto &Op : CallInst.operands()) {
      if (Op.isGlobal()) {
        FuncToCall = dyn_cast<Function>(Op.getGlobal());
        if (FuncToCall)
          break;
      }
    }

    if (!FuncToCall || FuncToCall->isDeclaration()) {
      return false;
    }

    if (getIRSize(FuncToCall) > MaxInsts) {
      return false;
    }

    if (checkRecursion(FuncToCall)) {
      return false;
    }

    return true;
  }
};

char VolkovInliner::ID = 0;

} // namespace

RegisterPass<VolkovInliner> X("volkov-a-inline", "Volkov Inliner Pass");