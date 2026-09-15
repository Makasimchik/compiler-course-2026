#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {
class InlinePass : public MachineFunctionPass {
public:
  static char ID;
  InlinePass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  static constexpr unsigned MaxInlineInstructions = 15;
  static constexpr unsigned MaxRecursionDepth = 3;

  StringRef getCalledName(const MachineInstr &MI) const;
  unsigned countInlineInstructions(const MachineFunction &CalleeMF) const;
  bool isInlineCandidate(const MachineFunction &CalleeMF) const;
  MachineInstr *cloneWithVRegRemap(MachineFunction &DstMF,
                                   const MachineInstr &SrcMI,
                                   DenseMap<Register, Register> &VRegMap) const;
  bool expandInline(const MachineFunction &CalleeMF, MachineFunction &DstMF,
                    SmallVectorImpl<MachineInstr *> &Out, unsigned Depth) const;
  MachineFunction *resolveCallee(StringRef Name,
                                 MachineFunction &Current) const;
};

char InlinePass::ID = 0;

StringRef InlinePass::getCalledName(const MachineInstr &MI) const {
  if (!MI.isCall() || MI.getNumOperands() == 0)
    return {};

  const MachineOperand &CalleeOp = MI.getOperand(0);
  if (CalleeOp.isGlobal()) {
    if (const auto *GV = CalleeOp.getGlobal()) {
      return GV->getName();
    }
  }
  if (CalleeOp.isSymbol())
    return CalleeOp.getSymbolName();
  return {};
}

unsigned
InlinePass::countInlineInstructions(const MachineFunction &CalleeMF) const {
  unsigned Count = 0;
  for (const MachineBasicBlock &MBB : CalleeMF) {
    for (const MachineInstr &MI : MBB) {
      if (MI.isReturn() || MI.isPHI() || MI.isDebugInstr() || MI.isPosition())
        continue;
      ++Count;
    }
  }
  return Count;
}

bool InlinePass::isInlineCandidate(const MachineFunction &CalleeMF) const {
  if (CalleeMF.empty())
    return false;

  if (!CalleeMF.getFunction().hasLocalLinkage())
    return false;

  if (std::distance(CalleeMF.begin(), CalleeMF.end()) != 1)
    return false;

  const MachineBasicBlock &MBB = CalleeMF.front();
  for (const MachineInstr &MI : MBB) {
    if (MI.isPHI() || MI.isInlineAsm())
      return false;
    if (MI.isBranch() || MI.isCall() || MI.isEHLabel())
      continue;
  }

  return countInlineInstructions(CalleeMF) <= MaxInlineInstructions;
}

MachineInstr *
InlinePass::cloneWithVRegRemap(MachineFunction &DstMF,
                               const MachineInstr &SrcMI,
                               DenseMap<Register, Register> &VRegMap) const {
  MachineInstr *Cloned = DstMF.CloneMachineInstr(&SrcMI);
  MachineRegisterInfo &MRI = DstMF.getRegInfo();

  for (MachineOperand &MO : Cloned->operands()) {
    if (!MO.isReg())
      continue;

    Register Reg = MO.getReg();
    if (!Reg.isVirtual())
      continue;

    Register &Mapped = VRegMap[Reg];
    if (!Mapped.isValid())
      Mapped = MRI.cloneVirtualRegister(Reg);
    MO.setReg(Mapped);
  }
  return Cloned;
}

bool InlinePass::expandInline(const MachineFunction &CalleeMF,
                              MachineFunction &DstMF,
                              SmallVectorImpl<MachineInstr *> &Out,
                              unsigned Depth) const {
  DenseMap<Register, Register> VRegMap;
  const MachineBasicBlock &CalleeMBB = CalleeMF.front();

  for (const MachineInstr &MI : CalleeMBB) {
    if (MI.isReturn() || MI.isDebugInstr() || MI.isPosition())
      continue;

    if (MI.isCall() && Depth < MaxRecursionDepth) {
      StringRef NestedName = getCalledName(MI);
      if (MachineFunction *NestedMF = resolveCallee(NestedName, DstMF)) {
        if (isInlineCandidate(*NestedMF) &&
            expandInline(*NestedMF, DstMF, Out, Depth + 1))
          continue;
      }
    }

    Out.push_back(cloneWithVRegRemap(DstMF, MI, VRegMap));
  }

  return true;
}

MachineFunction *InlinePass::resolveCallee(StringRef Name,
                                           MachineFunction &Current) const {
  if (Name.empty())
    return nullptr;
  return Name == Current.getName() ? &Current : nullptr;
}

bool InlinePass::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto It = MBB.begin(); It != MBB.end();) {
      MachineInstr &CallMI = *It;
      ++It;

      if (!CallMI.isCall())
        continue;

      MachineFunction *CalleeMF = resolveCallee(getCalledName(CallMI), MF);
      if (!CalleeMF || !isInlineCandidate(*CalleeMF))
        continue;

      SmallVector<MachineInstr *, 16> InlinedInstrs;
      if (!expandInline(*CalleeMF, MF, InlinedInstrs, 1))
        continue;

      for (MachineInstr *Cloned : InlinedInstrs)
        MBB.insert(It, Cloned);
      CallMI.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}
} // namespace

static RegisterPass<InlinePass> X("inline-x86", "function inlining pass", false,
                                  false);
