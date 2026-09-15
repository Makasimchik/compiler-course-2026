#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "x86-nullcheck"

namespace {

class X86NullCheckPass : public MachineFunctionPass {
public:
  static char ID;
  X86NullCheckPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return "X86 Null Check Pass"; }
};

char X86NullCheckPass::ID = 0;

} // namespace

bool X86NullCheckPass::runOnMachineFunction(MachineFunction &MF) {
  bool Modified = false;
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

  for (MachineBasicBlock &MBB : MF) {
    SmallVector<MachineInstr *, 4> DerefInsts;

    // Собираем все инструкции-разыменования
    for (MachineInstr &MI : MBB) {
      unsigned Opcode = MI.getOpcode();
      if (Opcode == X86::MOV32rm || Opcode == X86::MOV64rm ||
          Opcode == X86::MOV32mr || Opcode == X86::MOV64mr) {
        DerefInsts.push_back(&MI);
      }
    }

    if (DerefInsts.empty())
      continue;

    // Берем только первую инструкцию
    MachineInstr *MI = DerefInsts[0];

    // Находим регистр указателя
    unsigned PtrReg = 0;
    for (const MachineOperand &MO : MI->operands()) {
      if (MO.isReg() && MO.isUse() && MO.getReg()) {
        PtrReg = MO.getReg();
        break;
      }
    }

    if (!PtrReg)
      continue;

    // Сохраняем успешников
    SmallVector<MachineBasicBlock *, 4> OldSuccs(MBB.successors().begin(),
                                                 MBB.successors().end());

    // Создаем блоки
    MachineFunction &F = *MBB.getParent();
    MachineBasicBlock *CheckBlock = F.CreateMachineBasicBlock();
    MachineBasicBlock *TrapBlock = F.CreateMachineBasicBlock();
    MachineBasicBlock *ContBlock = F.CreateMachineBasicBlock();

    // Вставляем блоки
    auto NextIt = std::next(MBB.getIterator());
    F.insert(NextIt, CheckBlock);
    F.insert(std::next(CheckBlock->getIterator()), TrapBlock);
    F.insert(std::next(TrapBlock->getIterator()), ContBlock);

    // Trap блок
    BuildMI(TrapBlock, DebugLoc(), TII->get(X86::TRAP));

    // Переносим инструкции в ContBlock
    auto SplitPoint = MI->getIterator();
    ContBlock->splice(ContBlock->begin(), &MBB, SplitPoint, MBB.end());

    // Переносим успешников в ContBlock
    for (auto *Succ : OldSuccs) {
      ContBlock->addSuccessor(Succ);
      MBB.removeSuccessor(Succ);
    }

    // Заполняем CheckBlock
    BuildMI(CheckBlock, DebugLoc(), TII->get(X86::TEST64rr))
        .addReg(PtrReg)
        .addReg(PtrReg);
    BuildMI(CheckBlock, DebugLoc(), TII->get(X86::JCC_1))
        .addMBB(TrapBlock)
        .addImm(X86::COND_E);
    BuildMI(CheckBlock, DebugLoc(), TII->get(X86::JMP_1)).addMBB(ContBlock);

    // Обновляем связи MBB
    MBB.addSuccessor(CheckBlock);

    // Связи для новых блоков
    CheckBlock->addSuccessor(TrapBlock);
    CheckBlock->addSuccessor(ContBlock);

    Modified = true;
    break; // Только один блок за раз
  }

  return Modified;
}

static RegisterPass<X86NullCheckPass> X("x86-nullcheck", "X86 Null Check Pass",
                                        false, false);