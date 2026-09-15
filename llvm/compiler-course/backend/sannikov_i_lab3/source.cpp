#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

#define PASS_NAME "Null Pointer Check Pass"

namespace {

class NullCheckPass : public ModulePass {
public:
  static char ID;
  NullCheckPass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnModule(Module &M) override;

private:
  bool runOnMachineFunction(MachineFunction &MF, const X86InstrInfo *TII) const;
  Register findBaseReg(const MachineInstr &MI) const;
};

char NullCheckPass::ID = 0;

Register NullCheckPass::findBaseReg(const MachineInstr &MI) const {
  const MCInstrDesc &Desc = MI.getDesc();
  int MemOp = X86II::getMemoryOperandNo(Desc.TSFlags);
  if (MemOp == -1)
    return Register();

  MemOp += X86II::getOperandBias(Desc);
  const MachineOperand &Base = MI.getOperand(MemOp + X86::AddrBaseReg);
  if (!Base.isReg() || !Base.getReg().isValid())
    return Register();

  Register rg = Base.getReg();
  if (rg == X86::RSP || rg == X86::RBP)
    return Register();

  return rg;
}

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF,
                                         const X86InstrInfo *TII) const {
  bool chng = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto It = MBB.begin(); It != MBB.end(); ++It) {
      MachineInstr &MI = *It;

      if (!MI.mayLoad() && !MI.mayStore())
        continue;

      Register BaseReg = findBaseReg(MI);
      if (!BaseReg)
        continue;

      DebugLoc DL = MI.getDebugLoc();
      BuildMI(MBB, It, DL, TII->get(X86::TEST64rr))
          .addReg(BaseReg)
          .addReg(BaseReg);
      BuildMI(MBB, It, DL, TII->get(X86::JCC_1)).addImm(2).addImm(X86::COND_NE);
      BuildMI(MBB, It, DL, TII->get(X86::TRAP));

      chng = true;
    }
  }

  return chng;
}

bool NullCheckPass::runOnModule(Module &M) {
  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  bool chng = false;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    MachineFunction *MF = MMI.getMachineFunction(F);
    if (!MF)
      continue;

    const X86InstrInfo *TII = MF->getSubtarget<X86Subtarget>().getInstrInfo();

    chng |= runOnMachineFunction(*MF, TII);
  }

  return chng;
}

} // namespace

static RegisterPass<NullCheckPass> RegPass("null-ptr-safety", PASS_NAME, false,
                                           false);