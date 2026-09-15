#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class kruglova_incdec_to_addsub : public MachineFunctionPass {
public:
  static char ID;
  kruglova_incdec_to_addsub() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  struct repl {
    unsigned add_op;
    unsigned sub_op;
    int delta;
  };

  repl get_info(unsigned opc);
};

char kruglova_incdec_to_addsub::ID = 0;

kruglova_incdec_to_addsub::repl
kruglova_incdec_to_addsub::get_info(unsigned opc) {
  switch (opc) {
  case X86::INC8r:
    return {X86::ADD8ri, X86::SUB8ri, 1};
  case X86::DEC8r:
    return {X86::ADD8ri, X86::SUB8ri, -1};
  case X86::INC16r:
    return {X86::ADD16ri, X86::SUB16ri, 1};
  case X86::DEC16r:
    return {X86::ADD16ri, X86::SUB16ri, -1};
  case X86::INC32r:
    return {X86::ADD32ri, X86::SUB32ri, 1};
  case X86::DEC32r:
    return {X86::ADD32ri, X86::SUB32ri, -1};
  case X86::INC64r:
    return {X86::ADD64ri32, X86::SUB64ri32, 1};
  case X86::DEC64r:
    return {X86::ADD64ri32, X86::SUB64ri32, -1};
  default:
    return {0, 0, 0};
  }
}

bool kruglova_incdec_to_addsub::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
  const TargetInstrInfo *TII = ST.getInstrInfo();
  const TargetRegisterInfo *TRI = ST.getRegisterInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  bool Changed = false;
  SmallPtrSet<MachineInstr *, 8> EraseSet;

  for (auto &MBB : MF) {
    for (auto MII = MBB.begin(); MII != MBB.end();) {
      MachineInstr &MI = *MII++;

      if (EraseSet.count(&MI))
        continue;

      auto info = get_info(MI.getOpcode());
      if (info.delta == 0)
        continue;

      Register Src = MI.getOperand(1).getReg();
      Register Dst = MI.getOperand(0).getReg();
      int Sum = info.delta;

      SmallVector<MachineInstr *, 4> Chain;
      Chain.push_back(&MI);

      MachineInstr *Curr = &MI;
      while (true) {

        MachineInstr *Next = Curr->getNextNode();

        if (!Next)
          break;
        if (EraseSet.count(Next))
          break;

        auto NextInfo = get_info(Next->getOpcode());
        if (NextInfo.delta == 0)
          break;

        if (Next->getOperand(1).getReg() != Dst)
          break;
        if (!MRI.hasOneNonDBGUse(Dst))
          break;
        if (Next->readsRegister(X86::EFLAGS, TRI))
          break;

        Sum += NextInfo.delta;
        Chain.push_back(Next);
        Dst = Next->getOperand(0).getReg();
        Curr = Next;
      }

      if (Chain.size() < 2 && Sum == info.delta)
        continue;

      unsigned Opc = 0;
      int Imm = 0;

      if (Sum > 0) {
        Opc = get_info(Chain.back()->getOpcode()).add_op;
        Imm = Sum;
      } else if (Sum < 0) {
        Opc = get_info(Chain.back()->getOpcode()).sub_op;
        Imm = -Sum;
      }

      if (Sum != 0) {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(Opc), Dst)
            .addReg(Src)
            .addImm(Imm);
      } else {
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(X86::COPY), Dst)
            .addReg(Src);
      }

      for (auto *I : Chain)
        EraseSet.insert(I);

      Changed = true;
    }
  }

  for (auto *I : EraseSet) {
    I->eraseFromParent();
  }

  return Changed;
}
} // namespace

static RegisterPass<kruglova_incdec_to_addsub>
    X("kruglova_incdec_to_addsub-x86", "replace inc/dec with add/sub", false,
      false);
