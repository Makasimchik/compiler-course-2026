#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {
struct NullPtrPass : public MachineFunctionPass {
  static char ID;
  NullPtrPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &F) override;

private:
  Register fetchPointerBase(const MachineInstr &Inst) const;
  bool requiresVerification(Register Reg) const;
  void injectNullCheck(MachineBasicBlock &Block,
                       MachineBasicBlock::iterator Pos,
                       const DebugLoc &Location,
                       const TargetInstrInfo *InstrInfo, Register PtrReg) const;
};

char NullPtrPass::ID = 0;

Register NullPtrPass::fetchPointerBase(const MachineInstr &Inst) const {
  const MCInstrDesc &OpDesc = Inst.getDesc();

  int MemIdx = X86II::getMemoryOperandNo(OpDesc.TSFlags);
  if (MemIdx < 0) {
    return Register();
  }

  MemIdx += X86II::getOperandBias(OpDesc);
  const MachineOperand &BaseOp = Inst.getOperand(MemIdx + X86::AddrBaseReg);

  return BaseOp.isReg() ? BaseOp.getReg() : Register();
}

bool NullPtrPass::requiresVerification(Register Reg) const {
  return Reg.isValid() && Reg != X86::RSP && Reg != X86::RBP;
}

void NullPtrPass::injectNullCheck(MachineBasicBlock &Block,
                                  MachineBasicBlock::iterator Pos,
                                  const DebugLoc &Location,
                                  const TargetInstrInfo *InstrInfo,
                                  Register PtrReg) const {

  BuildMI(Block, Pos, Location, InstrInfo->get(TargetOpcode::COPY), X86::RDI)
      .addReg(PtrReg);

  BuildMI(Block, Pos, Location, InstrInfo->get(X86::CALL64pcrel32))
      .addExternalSymbol("check_null");
}

bool NullPtrPass::runOnMachineFunction(MachineFunction &F) {
  bool WasMutated = false;
  const TargetInstrInfo *InstrInfo = F.getSubtarget().getInstrInfo();

  for (MachineBasicBlock &Block : F) {
    for (auto Iter = Block.begin(); Iter != Block.end(); ++Iter) {
      MachineInstr &Inst = *Iter;

      // Проверяем инструкции чтения или записи памяти
      if (Inst.mayLoad() || Inst.mayStore()) {
        Register Base = fetchPointerBase(Inst);

        if (requiresVerification(Base)) {
          injectNullCheck(Block, Iter, Inst.getDebugLoc(), InstrInfo, Base);
          WasMutated = true;
        }
      }
    }
  }

  return WasMutated;
}
} // namespace

static RegisterPass<NullPtrPass>
    Registration("nullptrcheckpass",
                 "Injects null pointer checks before memory ops", false, false);