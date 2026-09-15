#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

static bool GetNewOpcode(unsigned Opc, unsigned &NewOpc) {
  switch (Opc) {
  default:
    return false;
  case X86::INC8r:
    NewOpc = X86::ADD8ri;
    return true;
  case X86::INC16r:
    NewOpc = X86::ADD16ri;
    return true;
  case X86::INC32r:
    NewOpc = X86::ADD32ri;
    return true;
  case X86::INC64r:
    NewOpc = X86::ADD64ri32;
    return true;
  case X86::DEC8r:
    NewOpc = X86::SUB8ri;
    return true;
  case X86::DEC16r:
    NewOpc = X86::SUB16ri;
    return true;
  case X86::DEC32r:
    NewOpc = X86::SUB32ri;
    return true;
  case X86::DEC64r:
    NewOpc = X86::SUB64ri32;
    return true;
  }
}

static bool IsTwoOp(const MachineInstr &MI) {
  if (MI.getNumOperands() < 2)
    return false;
  const MachineOperand &Def = MI.getOperand(0);
  const MachineOperand &Use = MI.getOperand(1);
  return Def.isReg() && Use.isReg() && Def.getReg().isValid() &&
         Def.getReg() == Use.getReg();
}

static bool IsMergeable(const MachineInstr &Head,
                        const MachineInstr &Candidate) {
  if (Head.getOpcode() != Candidate.getOpcode())
    return false;
  if (!IsTwoOp(Head) || !IsTwoOp(Candidate))
    return false;
  return Head.getOperand(0).getReg() == Candidate.getOperand(0).getReg();
}

class IncDecChanger : public MachineFunctionPass {
public:
  static char ID;
  IncDecChanger() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
};

char IncDecChanger::ID = 0;

bool IncDecChanger::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &sub_target = MF.getSubtarget<X86Subtarget>();
  const X86InstrInfo *instr_info = sub_target.getInstrInfo();

  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto instr = MBB.begin(); instr != MBB.end();) {
      unsigned NewOpc = 0;
      if (!GetNewOpcode(instr->getOpcode(), NewOpc) || !IsTwoOp(*instr)) {
        ++instr;
        continue;
      }

      int64_t Count = 1;
      auto RunEnd = std::next(instr);
      while (RunEnd != MBB.end() && IsMergeable(*instr, *RunEnd)) {
        ++Count;
        ++RunEnd;
      }

      MachineInstr &Head = *instr;
      MachineInstrBuilder MIB(MF, &Head);
      MIB->setDesc(instr_info->get(NewOpc));
      MIB.addImm(Count);

      auto EraseIt = std::next(instr);
      while (EraseIt != RunEnd)
        EraseIt = MBB.erase(EraseIt);

      Changed = true;
      instr = std::next(instr);
    }
  }

  return Changed;
}
} // namespace

static RegisterPass<IncDecChanger> X("incdec-changer-x86", "description pass",
                                     false, false);
