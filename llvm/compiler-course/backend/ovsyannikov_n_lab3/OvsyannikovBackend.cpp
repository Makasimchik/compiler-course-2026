#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
class OvsyannikovIncDecMerge : public MachineFunctionPass {
public:
  static char ID;
  OvsyannikovIncDecMerge() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    bool Changed = false;

    for (auto &MBB : MF) {
      for (auto I = MBB.begin(); I != MBB.end();) {
        MachineInstr &MI = *I;
        unsigned Opcode = MI.getOpcode();

        bool is32 = (Opcode == X86::INC32r || Opcode == X86::DEC32r);
        bool is64 = (Opcode == X86::INC64r || Opcode == X86::DEC64r);

        if (!is32 && !is64) {
          ++I;
          continue;
        }

        Register Reg = MI.getOperand(0).getReg();
        int Delta = (Opcode == X86::INC32r || Opcode == X86::INC64r) ? 1 : -1;
        std::vector<MachineInstr *> Chain;
        Chain.push_back(&MI);

        auto Next = std::next(I);
        while (Next != MBB.end()) {
          unsigned NOpc = Next->getOpcode();
          if (Next->getOperand(0).isReg() &&
              Next->getOperand(0).getReg() == Reg) {
            if ((is32 && NOpc == X86::INC32r) ||
                (is64 && NOpc == X86::INC64r)) {
              Delta += 1;
              Chain.push_back(&*Next);
              ++Next;
            } else if ((is32 && NOpc == X86::DEC32r) ||
                       (is64 && NOpc == X86::DEC64r)) {
              Delta -= 1;
              Chain.push_back(&*Next);
              ++Next;
            } else
              break;
          } else
            break;
        }

        DebugLoc DL = MI.getDebugLoc();
        if (Delta != 0) {
          unsigned NewOpc;
          if (is32)
            NewOpc = (Delta > 0) ? X86::ADD32ri : X86::SUB32ri;
          else
            NewOpc = (Delta > 0) ? X86::ADD64ri32 : X86::SUB64ri32;

          BuildMI(MBB, I, DL, TII->get(NewOpc), Reg)
              .addReg(Reg)
              .addImm(std::abs(Delta));
        }

        for (auto *Inst : Chain)
          Inst->eraseFromParent();
        Changed = true;
        I = Next;
      }
    }
    return Changed;
  }
};
char OvsyannikovIncDecMerge::ID = 0;
} // namespace

static RegisterPass<OvsyannikovIncDecMerge>
    X("ovsyannikov-inc-dec-merge", "Merge INC/DEC sequences", false, false);
