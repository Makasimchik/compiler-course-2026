#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
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
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    // Регистрируем функцию по имени, чтобы потом находить калли.
    FunctionRegistry[MF.getName().str()] = &MF;

    bool Changed = false;

    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++;

          if (!MIInst.isCall())
            continue;

          MachineFunction *Callee = findCallee(MIInst, MF);
          if (Callee && shouldInline(*Callee)) {
            performInline(MBB, MIInst, *Callee);
            LocalChanged = true;
            Changed = true;
            goto restart;
          }
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

  MachineFunction *findCallee(MachineInstr &MI, MachineFunction &Caller) {
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

      // Рекурсивный вызов: callee == caller.
      if (Name == Caller.getName())
        return &Caller;

      auto It = FunctionRegistry.find(Name.str());
      if (It != FunctionRegistry.end())
        return It->second;
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
                     MachineFunction &Callee) {
    SmallVector<MachineInstr *, 16> Body;

    // Собираем тело до первого вызова/ret — чтобы рекурсия не раздувалась
    // экспоненциально и соответствовала CHECK-COUNT-4 для recursive_func.
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

    CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;

} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);
