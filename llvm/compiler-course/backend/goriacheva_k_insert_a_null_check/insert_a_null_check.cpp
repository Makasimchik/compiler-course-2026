#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
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

private:
  using WorkItem = std::pair<MachineInstr *, Register>;

  void collectCandidates(MachineFunction &MF, SmallVectorImpl<WorkItem> &Out);

  Register findBase(const MachineInstr &MI) const;

  void injectCheck(MachineInstr *MI, Register Base, const X86InstrInfo *TII,
                   MachineBasicBlock *Trap);

  MachineBasicBlock *ensureTrap(MachineFunction &MF);

private:
  MachineBasicBlock *TrapBlock = nullptr;
};

char NullCheckPass::ID = 0;

Register NullCheckPass::findBase(const MachineInstr &MI) const {
  const MCInstrDesc &D = MI.getDesc();

  int Mem = X86II::getMemoryOperandNo(D.TSFlags);
  if (Mem < 0)
    return Register();

  Mem += X86II::getOperandBias(D);

  unsigned BaseIdx = Mem + X86::AddrBaseReg;
  if (BaseIdx < MI.getNumOperands()) {
    const MachineOperand &Op = MI.getOperand(BaseIdx);
    if (Op.isReg()) {
      Register R = Op.getReg();
      if (R && R != X86::RSP && R != X86::RBP)
        return R;
    }
  }

  unsigned IndexIdx = Mem + X86::AddrIndexReg;
  if (IndexIdx < MI.getNumOperands()) {
    const MachineOperand &Op = MI.getOperand(IndexIdx);
    if (Op.isReg()) {
      Register R = Op.getReg();
      if (R && R != X86::RSP && R != X86::RBP)
        return R;
    }
  }

  return Register();
}

void NullCheckPass::collectCandidates(MachineFunction &MF,
                                      SmallVectorImpl<WorkItem> &Out) {

  for (auto &MBB : MF) {
    for (auto &MI : MBB) {

      if (!(MI.mayLoad() || MI.mayStore()))
        continue;

      if (MI.isCall() || MI.isBranch())
        continue;

      Register Base = findBase(MI);
      if (!Base)
        continue;

      Out.emplace_back(&MI, Base);
    }
  }
}

MachineBasicBlock *NullCheckPass::ensureTrap(MachineFunction &MF) {
  if (TrapBlock)
    return TrapBlock;

  const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();

  TrapBlock = MF.CreateMachineBasicBlock();
  MF.push_back(TrapBlock);

  BuildMI(TrapBlock, DebugLoc(), TII->get(X86::TRAP));

  return TrapBlock;
}

void NullCheckPass::injectCheck(MachineInstr *MI, Register Base,
                                const X86InstrInfo *TII,
                                MachineBasicBlock *Trap) {

  MachineBasicBlock *MBB = MI->getParent();
  DebugLoc DL = MI->getDebugLoc();

  auto Pos = MI->getIterator();

  if (Pos != MBB->begin()) {
    auto Prev = std::prev(Pos);

    if (Prev->getOpcode() == X86::TEST64rr ||
        Prev->getOpcode() == X86::TEST32rr) {
      if (Prev->getOperand(0).isReg() && Prev->getOperand(1).isReg() &&
          Prev->getOperand(0).getReg() == Base &&
          Prev->getOperand(1).getReg() == Base) {
        return;
      }
    }
  }

  MachineInstrBuilder Test = BuildMI(*MBB, Pos, DL, TII->get(X86::TEST64rr));

  Test.addReg(Base);
  Test.addReg(Base);
  Test.addReg(X86::EFLAGS, RegState::Define);

  MachineInstrBuilder Br = BuildMI(*MBB, Pos, DL, TII->get(X86::JCC_1));

  Br.addMBB(Trap);
  Br.addImm(X86::COND_E);
  Br.addReg(X86::EFLAGS);

  if (!llvm::is_contained(MBB->successors(), Trap))
    MBB->addSuccessor(Trap);
}

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();

  SmallVector<WorkItem, 16> Items;
  collectCandidates(MF, Items);

  if (Items.empty())
    return false;

  MachineBasicBlock *Trap = ensureTrap(MF);

  DenseMap<MachineBasicBlock *, SmallSet<Register, 8>> CheckedRegs;

  for (auto &It : Items) {
    MachineInstr *MI = It.first;
    Register Reg = It.second;
    MachineBasicBlock *MBB = MI->getParent();

    if (CheckedRegs[MBB].contains(Reg))
      continue;

    injectCheck(MI, Reg, TII, Trap);
    CheckedRegs[MBB].insert(Reg);
  }

  return true;
}
} // namespace

static RegisterPass<NullCheckPass>
    X("null-check-x86", "insert a null check before dereferencing a pointer",
      false, false);
