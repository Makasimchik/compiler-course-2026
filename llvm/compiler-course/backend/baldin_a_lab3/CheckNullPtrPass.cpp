#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
class NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  NullCheckPass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
};

char NullCheckPass::ID = 0;

bool NullCheckPass::runOnMachineFunction(MachineFunction &func) {
  bool changed = false;

  const TargetInstrInfo *TII = func.getSubtarget().getInstrInfo();

  for (MachineBasicBlock &MBB : func) {
    for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
      MachineInstr &instr = *MI;

      if (instr.mayLoad() || instr.mayStore()) {
        Register baseReg = 0;

        const MCInstrDesc &desc = instr.getDesc();
        int memOpStart = X86II::getMemoryOperandNo(desc.TSFlags);

        if (memOpStart != -1) {
          memOpStart += X86II::getOperandBias(desc);

          const MachineOperand &baseOp =
              instr.getOperand(memOpStart + X86::AddrBaseReg);

          if (baseOp.isReg() && baseOp.getReg().isValid()) {
            Register r = baseOp.getReg();
            if (r != X86::RSP && r != X86::RBP) {
              baseReg = r;
            }
          }
        }

        if (!baseReg)
          continue;

        DebugLoc DL = instr.getDebugLoc();

        BuildMI(MBB, MI, DL, TII->get(TargetOpcode::COPY), X86::RDI)
            .addReg(baseReg);

        BuildMI(MBB, MI, DL, TII->get(X86::CALL64pcrel32))
            .addExternalSymbol("check_null");

        changed = true;
      }
    }
  }

  return changed;
}
} // namespace

static RegisterPass<NullCheckPass>
    X("null-check-x86", "Insert NULL checks before dereference", false, false);
