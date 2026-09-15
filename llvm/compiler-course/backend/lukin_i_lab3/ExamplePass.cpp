#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <map>
#include <set>

using namespace llvm;

namespace {
class LukinInliningModulePass : public ModulePass {
  static const int maxCountOfInstructions = 15;
  static const int recursionMaxDepth = 3;

  std::set<const Function *> globalProcessedSet;

public:
  static char ID;
  LukinInliningModulePass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return "LukinInliningModulePass"; }

  bool runOnModule(Module &M) override;

private:
  int getCountOfInstructions(const MachineFunction &MF) const;
  bool Inline(MachineFunction &Caller, MachineBasicBlock &MBB,
              MachineInstr &Ins, int &depth, MachineModuleInfo &MMI);
};

char LukinInliningModulePass::ID = 0;

bool LukinInliningModulePass::runOnModule(Module &M) {
  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  bool changed = false;

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

      for (auto &MBB : *MF) {
        for (auto it = MBB.begin(); it != MBB.end();) {
          MachineInstr &Ins = *it++;

          if (Ins.getOpcode() == X86::CALL64pcrel32 &&
              Ins.getNumOperands() > 0) {
            MachineOperand &Op = Ins.getOperand(0);
            if (!Op.isGlobal())
              continue;

            const Function *CalleeF = dyn_cast<Function>(Op.getGlobal());
            if (!CalleeF)
              continue;

            if (localBlacklist.count(CalleeF))
              continue;

            if (Inline(*MF, MBB, Ins, depth, MMI)) {
              if (globalProcessedSet.count(CalleeF)) {
                localBlacklist.insert(CalleeF);
              }
              onIterChanged = true;
              changed = true;
            }
          }
        }
      }
    }

    globalProcessedSet.insert(&F);
  }
  return changed;
}

int LukinInliningModulePass::getCountOfInstructions(
    const MachineFunction &MF) const {
  int count = 0;
  for (const auto &MBB : MF) {
    for (const auto &MI : MBB) {
      if (!MI.isDebugInstr() && !MI.isMetaInstruction()) {
        count++;
      }
    }
  }
  return count;
}

bool LukinInliningModulePass::Inline(MachineFunction &Caller,
                                     MachineBasicBlock &MBB, MachineInstr &Ins,
                                     int &depth, MachineModuleInfo &MMI) {
  MachineOperand &operand = Ins.getOperand(0);
  const Function *CalleeF = cast<Function>(operand.getGlobal());
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

  MachineRegisterInfo &CallerMRI = Caller.getRegInfo();
  MachineRegisterInfo &CalleeMRI = CalleeMF->getRegInfo();

  std::map<Register, Register> regsTransform;
  SmallVector<MachineInstr *, 16> clone;

  for (auto &MInst : CalleeMF->front()) {
    if (!MInst.isReturn())
      clone.push_back(&MInst);
  }

  for (MachineInstr *OriginalInstr : clone) {
    MachineInstr *InlinedInstr = Caller.CloneMachineInstr(OriginalInstr);

    for (int opIdx = 0, numOps = InlinedInstr->getNumOperands();
         opIdx != numOps; opIdx++) {
      MachineOperand &MOp = InlinedInstr->getOperand(opIdx);
      if (MOp.isReg() && MOp.getReg().isVirtual()) {
        Register OldVReg = MOp.getReg();
        auto Mapping = regsTransform.find(OldVReg);
        if (Mapping == regsTransform.end()) {
          const TargetRegisterClass *TClass = CalleeMRI.getRegClass(OldVReg);
          Register NewVReg = CallerMRI.createVirtualRegister(TClass);
          Mapping = regsTransform.insert({OldVReg, NewVReg}).first;
        }
        MOp.setReg(Mapping->second);
      }
    }
    MBB.insert(Ins, InlinedInstr);
  }

  Ins.eraseFromParent();
  return true;
}

} // namespace

static RegisterPass<LukinInliningModulePass>
    X("InliningPass", "function inlining pass -x86", false, false);