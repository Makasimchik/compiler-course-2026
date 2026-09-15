#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetMachine.h"

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
  bool hasExistingCheck(MachineBasicBlock::iterator Pos, Register Base) const;
  void injectCheck(MachineInstr *MI, Register Base, const X86InstrInfo *TII,
                   MachineBasicBlock *Trap);
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

bool NullCheckPass::hasExistingCheck(MachineBasicBlock::iterator Pos,
                                     Register Base) const {
  MachineBasicBlock *MBB = Pos->getParent();
  auto It = Pos;
  for (unsigned i = 0; i < 20; ++i) {
    if (It == MBB->begin())
      break;
    --It;

    if (It->getOpcode() == X86::TEST64rr || It->getOpcode() == X86::TEST32rr) {
      if (It->getOperand(0).isReg() && It->getOperand(1).isReg() &&
          It->getOperand(0).getReg() == Base &&
          It->getOperand(1).getReg() == Base) {
        return true;
      }
    }

    if (It->isCall() && It->getOpcode() == X86::CALL64pcrel32) {
      bool CallsCheckNull = false;
      for (const MachineOperand &MO : It->operands()) {
        if ((MO.isSymbol() &&
             StringRef(MO.getSymbolName()).contains("check_null")) ||
            (MO.isGlobal() &&
             MO.getGlobal()->getName().contains("check_null"))) {
          CallsCheckNull = true;
          break;
        }
      }
      if (CallsCheckNull) {
        auto CopyIt = It;
        if (CopyIt != MBB->begin()) {
          --CopyIt;
          if (CopyIt->getOpcode() == TargetOpcode::COPY &&
              CopyIt->getOperand(0).isReg() &&
              CopyIt->getOperand(0).getReg() == X86::RDI &&
              CopyIt->getOperand(1).isReg() &&
              CopyIt->getOperand(1).getReg() == Base) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void NullCheckPass::injectCheck(MachineInstr *MI, Register Base,
                                const X86InstrInfo *TII,
                                MachineBasicBlock *Trap) {
  MachineBasicBlock *MBB = MI->getParent();
  DebugLoc DL = MI->getDebugLoc();
  auto Pos = MI->getIterator();

  if (hasExistingCheck(Pos, Base))
    return;

  BuildMI(*MBB, Pos, DL, TII->get(X86::TEST64rr))
      .addReg(Base)
      .addReg(Base)
      .addReg(X86::EFLAGS, RegState::Define);

  BuildMI(*MBB, Pos, DL, TII->get(X86::JCC_1))
      .addMBB(Trap)
      .addImm(X86::COND_E)
      .addReg(X86::EFLAGS);

  if (!llvm::is_contained(MBB->successors(), Trap))
    MBB->addSuccessor(Trap);
}

bool NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  const X86InstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
  SmallVector<WorkItem, 16> Items;
  collectCandidates(MF, Items);
  if (Items.empty())
    return false;

  MachineBasicBlock *Trap = MF.CreateMachineBasicBlock();
  MF.push_back(Trap);
  BuildMI(Trap, DebugLoc(), TII->get(X86::TRAP));

  DenseMap<MachineBasicBlock *, SmallSet<Register, 8>> CheckedRegs;

  bool Changed = false;
  for (auto &It : Items) {
    MachineInstr *MI = It.first;
    Register Reg = It.second;
    MachineBasicBlock *MBB = MI->getParent();

    if (CheckedRegs[MBB].contains(Reg))
      continue;

    injectCheck(MI, Reg, TII, Trap);
    CheckedRegs[MBB].insert(Reg);
    Changed = true;
  }
  return Changed;
}

} // namespace

static RegisterPass<NullCheckPass>
    X("null-check-x86", "insert a null check before dereferencing a pointer",
      false, false);