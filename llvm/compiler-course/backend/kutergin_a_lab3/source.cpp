#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class IncDecToAddSubPass : public MachineFunctionPass {
public:
  static char ID;
  IncDecToAddSubPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
    const TargetInstrInfo *TII = ST.getInstrInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end();) {
        MachineInstr &MI = *I;
        unsigned Opcode = MI.getOpcode();

        if (isAllowedOpcode(Opcode)) {
          Register Reg = MI.getOperand(0).getReg();
          int Accumulator = getOpcodeValue(Opcode);

          auto NextI = std::next(I);
          SmallVector<MachineInstr *, 4> ToErase;
          ToErase.push_back(&MI);

          while (NextI != MBB.end()) {
            unsigned NextOp = NextI->getOpcode();
            if (isAllowedOpcode(NextOp) &&
                NextI->getOperand(0).getReg() == Reg) {
              Accumulator += getOpcodeValue(NextOp);
              ToErase.push_back(&*NextI);
              ++NextI;
            } else {
              break;
            }
          }

          if (Accumulator != 0) {
            bool Is64 = (Opcode == X86::INC64r || Opcode == X86::DEC64r);
            unsigned NewOpcode;
            if (Accumulator > 0) {
              NewOpcode = Is64 ? X86::ADD64ri8 : X86::ADD32ri8;
            } else {
              NewOpcode = Is64 ? X86::SUB64ri8 : X86::SUB32ri8;
              Accumulator = -Accumulator;
            }

            BuildMI(MBB, I, MI.getDebugLoc(), TII->get(NewOpcode), Reg)
                .addReg(Reg)
                .addImm(Accumulator);

            for (auto *Inst : ToErase)
              Inst->eraseFromParent();
            I = NextI;
            Changed = true;
          } else {
            for (auto *Inst : ToErase)
              Inst->eraseFromParent();
            I = NextI;
            Changed = true;
          }
        } else {
          ++I;
        }
      }
    }
    return Changed;
  }

  StringRef getPassName() const override {
    return "X86 INC/DEC to ADD/SUB transformation";
  }

private:
  bool isAllowedOpcode(unsigned Opcode) const {
    return Opcode == X86::INC32r || Opcode == X86::INC64r ||
           Opcode == X86::DEC32r || Opcode == X86::DEC64r;
  }

  int getOpcodeValue(unsigned Opcode) const {
    if (Opcode == X86::INC32r || Opcode == X86::INC64r)
      return 1;
    if (Opcode == X86::DEC32r || Opcode == X86::DEC64r)
      return -1;
    return 0;
  }
};
} // end anonymous namespace

char IncDecToAddSubPass::ID = 0;

static RegisterPass<IncDecToAddSubPass>
    X("inc-dec-to-add-sub", "X86 INC/DEC to ADD/SUB transformation", false,
      false);