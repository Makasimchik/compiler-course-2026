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
#include <map>
#include <string>

using namespace llvm;

namespace {

static std::map<std::string, MachineFunction *> FunctionRegistry;

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    FunctionRegistry[MF.getName().str()] = &MF;

    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

    bool Changed = false;

    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++;

          // Явно матчим x86-вызов.
          if (MIInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = findCallee(MIInst, MF, MMI);
          if (!Callee)
            continue;

          if (!shouldInline(*Callee))
            continue;

          bool IsRecursive = (Callee == &MF);

          performInline(MBB, MIInst, *Callee, IsRecursive);
          LocalChanged = true;
          Changed = true;
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
      StringRef Name;

      if (MO.isGlobal() && MO.getGlobal())
        Name = MO.getGlobal()->getName();
      else if (MO.isSymbol())
        Name = MO.getSymbolName();
      else
        continue;

      if (Name.empty())
        continue;

      // Рекурсивный вызов.
      if (Name == Caller.getName())
        return &Caller;

      auto It = FunctionRegistry.find(Name.str());
      if (It != FunctionRegistry.end())
        return It->second;

      // Попробовать достать MachineFunction через MMI.
      Module &M = *Caller.getFunction().getParent();
      if (Function *F = M.getFunction(Name)) {
        if (MachineFunction *MF = MMI.getMachineFunction(*F))
          return MF;
      }
    }
    return nullptr;
  }

  bool shouldInline(MachineFunction &Callee) {
    unsigned Count = 0;
    for (auto &MBB : Callee)
      for (auto &MI : MBB)
        if (!MI.isReturn())
          Count++;
    return Count > 0 && Count <= MaxInstrs;
  }

  void cloneInstrInto(MachineInstr &Src, MachineBasicBlock &Dst,
                      MachineInstr &Before) {
    MachineInstrBuilder MIB =
        BuildMI(Dst, Before, Src.getDebugLoc(), Src.getDesc());

    for (const MachineOperand &MO : Src.operands())
      MIB.add(MO);
  }

  void performInline(MachineBasicBlock &MBB, MachineInstr &CallInst,
                     MachineFunction &Callee, bool IsRecursive) {
    SmallVector<MachineInstr *, 16> Body;

    for (auto &CBB : Callee) {
      for (auto &CMI : CBB) {
        if (CMI.isReturn())
          continue;
        if (CMI.isCall())
          break;
        Body.push_back(&CMI);
      }
    }

    for (auto *I : Body)
      cloneInstrInto(*I, MBB, CallInst);

    if (!IsRecursive)
      CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;

} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);
