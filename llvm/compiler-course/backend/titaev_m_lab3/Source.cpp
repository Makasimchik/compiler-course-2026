#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class ExamplePass : public ModulePass {
public:
  static char ID;
  ExamplePass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override {
    MachineModuleInfo &MMI =
        getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    bool Changed = false;

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      if (MachineFunction *MF = MMI.getMachineFunction(F)) {
        Changed |= runOnMachineFunction(*MF, MMI);
      }
    }
    return Changed;
  }

private:
  static constexpr unsigned MaxInstrs = 25;

  bool runOnMachineFunction(MachineFunction &MF, MachineModuleInfo &MMI) {
    bool LocalChanged = false;
    for (auto &MBB : MF) {
      for (auto MI = MBB.begin(); MI != MBB.end();) {
        MachineInstr &CallInst = *MI++;

        if (CallInst.getOpcode() != X86::CALL64pcrel32)
          continue;

        MachineFunction *Callee = findCallee(CallInst, MF, MMI);
        if (!Callee || !shouldInline(*Callee))
          continue;

        if (Callee == &MF)
          continue;

        performInline(MBB, CallInst, *Callee);
        return true;
      }
    }
    return LocalChanged;
  }

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller,
                              MachineModuleInfo &MMI) {
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isGlobal() && isa<Function>(MO.getGlobal())) {
        return MMI.getMachineFunction(*cast<Function>(MO.getGlobal()));
      }
      if (MO.isSymbol()) {
        if (Function *F = Caller.getFunction().getParent()->getFunction(
                MO.getSymbolName()))
          return MMI.getMachineFunction(*F);
      }
    }
    return nullptr;
  }

  bool shouldInline(MachineFunction &Callee) {
    unsigned Count = 0;
    for (auto &MBB : Callee) {
      for (auto &MI : MBB) {
        if (MI.isReturn() || MI.isTerminator() || MI.isDebugInstr())
          continue;
        Count++;
      }
    }
    return Count > 0 && Count <= MaxInstrs;
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee) {
    MachineFunction &CallerMF = *MBB.getParent();
    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn() || CMI.isTerminator())
          continue;
        MBB.insert(CallInst, CallerMF.CloneMachineInstr(&CMI));
      }
    }
    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass> X("example-x86", "X86 Machine Inline Pass",
                                   false, false);