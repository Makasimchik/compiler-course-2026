#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

struct OpcodeEntry {
  unsigned IncOpc;
  unsigned DecOpc;
  unsigned AddOpc;
  unsigned SubOpc;
};

static const OpcodeEntry OpcodeTable[] = {
    {X86::INC64r, X86::DEC64r, X86::ADD64ri32, X86::SUB64ri32},
    {X86::INC32r, X86::DEC32r, X86::ADD32ri, X86::SUB32ri},
    {X86::INC16r, X86::DEC16r, X86::ADD16ri, X86::SUB16ri},
    {X86::INC8r, X86::DEC8r, X86::ADD8ri, X86::SUB8ri},
};

static bool getNewOpcodes(unsigned Opc, unsigned &AddOpc, unsigned &SubOpc) {
  for (const auto &E : OpcodeTable) {
    if (E.IncOpc == Opc || E.DecOpc == Opc) {
      AddOpc = E.AddOpc;
      SubOpc = E.SubOpc;
      return true;
    }
  }
  return false;
}

static bool isInc(unsigned Opc) {
  for (const auto &E : OpcodeTable)
    if (E.IncOpc == Opc)
      return true;
  return false;
}

static bool isDec(unsigned Opc) {
  for (const auto &E : OpcodeTable)
    if (E.DecOpc == Opc)
      return true;
  return false;
}

static bool isIncOrDec(unsigned Opc) { return isInc(Opc) || isDec(Opc); }

static Register getDstReg(const MachineInstr &MI) {
  return MI.getOperand(0).getReg();
}

class IncDecCombinePass : public MachineFunctionPass {
public:
  static char ID;
  IncDecCombinePass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
};

char IncDecCombinePass::ID = 0;

bool IncDecCombinePass::runOnMachineFunction(MachineFunction &MF) {
  const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      unsigned Opc = I->getOpcode();
      unsigned AddOpc = 0, SubOpc = 0;

      if (!isIncOrDec(Opc) || !getNewOpcodes(Opc, AddOpc, SubOpc)) {
        ++I;
        continue;
      }

      Register Reg = getDstReg(*I);
      int Delta = isInc(Opc) ? 1 : -1;
      unsigned FirstOpc = Opc;
      DebugLoc DL = I->getDebugLoc();
      auto SeqBegin = I;
      ++I;

      while (I != MBB.end() && isIncOrDec(I->getOpcode()) &&
             getDstReg(*I) == Reg) {
        Delta += isInc(I->getOpcode()) ? 1 : -1;
        ++I;
      }

      MBB.erase(SeqBegin, I);

      if (Delta != 0) {
        unsigned AddO = 0, SubO = 0;
        getNewOpcodes(FirstOpc, AddO, SubO);
        unsigned NewOpc = (Delta > 0) ? AddO : SubO;
        int Imm = (Delta > 0) ? Delta : -Delta;
        BuildMI(MBB, I, DL, TII->get(NewOpc), Reg).addReg(Reg).addImm(Imm);
      }

      Changed = true;
    }
  }

  return Changed;
}

} // namespace

static RegisterPass<IncDecCombinePass>
    X("inc-dec-combine", "X86 INC/DEC to ADD/SUB combine", false, false);
