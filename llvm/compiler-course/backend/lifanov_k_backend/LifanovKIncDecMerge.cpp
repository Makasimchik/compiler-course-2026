#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class LifanovKIncDecFusionPass : public MachineFunctionPass {
public:
  static char ID;

  LifanovKIncDecFusionPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();

    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      auto It = MBB.begin();

      while (It != MBB.end()) {
        MachineInstr &Start = *It;

        int delta = classify(Start);
        if (delta == 0) {
          ++It;
          continue;
        }

        Register Reg = Start.getOperand(0).getReg();
        unsigned width = widthOf(Start.getOpcode());

        int total = delta;
        auto Scan = std::next(It);

        while (Scan != MBB.end()) {
          MachineInstr &MI = *Scan;

          int d = classify(MI);
          if (d == 0)
            break;

          if (MI.getOperand(0).getReg() != Reg)
            break;
          if (widthOf(MI.getOpcode()) != width)
            break;

          total += d;
          ++Scan;
        }

        DebugLoc DL = Start.getDebugLoc();

        if (total != 0) {
          unsigned Opc = selectOpcode(width, total);

          BuildMI(MBB, It, DL, TII->get(Opc))
              .addReg(Reg, RegState::Define)
              .addReg(Reg)
              .addImm(std::abs(total));
        }

        auto Erase = It;
        while (Erase != Scan)
          Erase = MBB.erase(Erase);

        It = Scan;
        Changed = true;
      }
    }

    return Changed;
  }

private:
  static int classify(const MachineInstr &MI) {
    switch (MI.getOpcode()) {
    case X86::INC32r:
    case X86::INC64r:
      return 1;

    case X86::DEC32r:
    case X86::DEC64r:
      return -1;

    default:
      return 0;
    }
  }

  static unsigned widthOf(unsigned Opc) {
    switch (Opc) {
    case X86::INC32r:
    case X86::DEC32r:
      return 32;

    case X86::INC64r:
    case X86::DEC64r:
      return 64;

    default:
      return 0;
    }
  }

  static unsigned selectOpcode(unsigned W, int Delta) {
    bool add = Delta > 0;

    if (W == 32)
      return add ? X86::ADD32ri : X86::SUB32ri;

    if (W == 64)
      return add ? X86::ADD64ri32 : X86::SUB64ri32;

    return 0;
  }
};

char LifanovKIncDecFusionPass::ID = 0;

} // namespace

static RegisterPass<LifanovKIncDecFusionPass>
    X("lifanovk-incdec-fusion",
      "LifanovK: INC/DEC fusion pass (ADD/SUB lowring)", false, false);