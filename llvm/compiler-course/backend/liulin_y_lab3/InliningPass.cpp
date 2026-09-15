#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

namespace {

class LiulinInliningModulePass : public ModulePass {
public:
  static char ID;

  LiulinInliningModulePass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;

private:
  bool Inline(MachineFunction &Caller, MachineBasicBlock &MBB,
              MachineInstr &Ins, int depth, MachineModuleInfo &MMI,
              std::set<const Function *> &globalProcessedSet,
              std::set<const Function *> &localBlacklist);

  int getCountOfInstructions(MachineFunction &MF);
};

} // anonymous namespace

char LiulinInliningModulePass::ID = 0;
static RegisterPass<LiulinInliningModulePass> X("InliningPass", "Inlining Pass",
                                                false, false);

bool LiulinInliningModulePass::runOnModule(Module &M) {
  bool Changed = false;
  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  std::set<const Function *> globalProcessedSet;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;

    std::set<const Function *> localBlacklist;
    int depth = 0;
    bool onIterChanged = true;

    while (onIterChanged) {
      onIterChanged = false;

      for (MachineBasicBlock &MBB : *MF) {
        for (auto It = MBB.begin(); It != MBB.end();) {
          MachineInstr &MI = *It;
          ++It;

          if (MI.getOpcode() == X86::CALL64pcrel32) {
            const MachineOperand &Op = MI.getOperand(0);
            if (Op.isGlobal()) {
              if (const Function *CalleeF =
                      dyn_cast<Function>(Op.getGlobal())) {
                if (localBlacklist.count(CalleeF))
                  continue;

                if (Inline(*MF, MBB, MI, depth, MMI, globalProcessedSet,
                           localBlacklist)) {
                  if (globalProcessedSet.count(CalleeF)) {
                    localBlacklist.insert(CalleeF);
                  }
                  onIterChanged = true;
                  Changed = true;
                }
              }
            }
          }
        }
      }
    }

    globalProcessedSet.insert(&F);
  }

  return Changed;
}

bool LiulinInliningModulePass::Inline(
    MachineFunction &Caller, MachineBasicBlock &MBB, MachineInstr &Ins,
    int depth, MachineModuleInfo &MMI,
    std::set<const Function *> &globalProcessedSet,
    std::set<const Function *> &localBlacklist) {
  const int maxCountOfInstructions = 15;
  const int recursionMaxDepth = 3;

  const MachineOperand &Op = Ins.getOperand(0);
  const Function *CalleeF = cast<Function>(Op.getGlobal());

  MachineFunction *CalleeMF = nullptr;

  if (CalleeF == &Caller.getFunction()) {
    if (depth >= recursionMaxDepth)
      return false;
    depth++;
    CalleeMF = &Caller;
  } else {
    CalleeMF = MMI.getMachineFunction(*CalleeF);
    if (!CalleeMF)
      return false;
  }

  if (getCountOfInstructions(*CalleeMF) > maxCountOfInstructions)
    return false;

  MachineBasicBlock &CalleeMBB = CalleeMF->front();

  SmallVector<MachineInstr *, 16> clone;
  for (MachineInstr &MI : CalleeMBB) {
    if (!MI.isReturn())
      clone.push_back(&MI);
  }

  MachineRegisterInfo &CallerMRI = Caller.getRegInfo();
  MachineRegisterInfo &CalleeMRI = CalleeMF->getRegInfo();
  std::map<Register, Register> regsTransform;

  for (MachineInstr *OriginalInstr : clone) {
    MachineInstr *NewMI = Caller.CloneMachineInstr(OriginalInstr);
    for (MachineOperand &MO : NewMI->operands()) {
      if (MO.isReg()) {
        Register Reg = MO.getReg();
        if (Reg.isVirtual()) {
          if (regsTransform.find(Reg) == regsTransform.end()) {
            const TargetRegisterClass *RC = CalleeMRI.getRegClass(Reg);
            Register NewReg = CallerMRI.createVirtualRegister(RC);
            regsTransform[Reg] = NewReg;
          }
          MO.setReg(regsTransform[Reg]);
        }
      }
    }

    MBB.insert(Ins.getIterator(), NewMI);
  }

  Ins.eraseFromParent();
  return true;
}

int LiulinInliningModulePass::getCountOfInstructions(MachineFunction &MF) {
  int count = 0;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (!MI.isDebugInstr() && !MI.isMetaInstruction())
        ++count;
    }
  }
  return count;
}