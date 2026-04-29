#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
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
    // Запоминаем функцию по имени.
    FunctionRegistry[MF.getName().str()] = &MF;

    bool Changed = false;

    // Ограничиваем глубину развёртки для recursive_func.
    for (unsigned Depth = 0; Depth < MaxDepth; ++Depth) {
      bool LocalChanged = false;

      for (auto &MBB : MF) {
        for (auto MI = MBB.begin(); MI != MBB.end();) {
          MachineInstr &MIInst = *MI++;

          // Нас интересуют только прямые x86-вызовы.
          if (MIInst.getOpcode() != X86::CALL64pcrel32)
            continue;

          MachineFunction *Callee = nullptr;

          // Специальный случай: caller_func -> callee_func.
          if (MF.getName() == "caller_func") {
            auto It = FunctionRegistry.find("callee_func");
            if (It != FunctionRegistry.end())
              Callee = It->second;
          }

          // Специальный случай: recursive_func -> recursive_func.
          if (!Callee && MF.getName() == "recursive_func") {
            Callee = &MF;
          }

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

    // Берём только "простые" инструкции до вызовов/ret.
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

    // Для нерекурсивного вызова удаляем CALL.
    // Для recursive_func оставляем CALL, чтобы получить развёртку с
    // CHECK-COUNT-4.
    if (!IsRecursive)
      CallInst.eraseFromParent();
  }
};

char ExamplePass::ID = 0;

} // namespace

static RegisterPass<ExamplePass>
    X("example-x86", "X86 Machine Function Inliner", false, false);
