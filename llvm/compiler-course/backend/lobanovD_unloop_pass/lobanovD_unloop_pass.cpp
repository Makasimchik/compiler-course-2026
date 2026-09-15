#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
    for (MachineBasicBlock &MBB : MF)
      Changed |= processBlock(MBB, MF, TII);
    return Changed;
  }

private:
  bool processBlock(MachineBasicBlock &MBB, MachineFunction &MF,
                    const TargetInstrInfo &TII) {
    for (auto It = MBB.begin(); It != MBB.end(); ++It) {
      if (It->getOpcode() != X86::CMP32ri8 && It->getOpcode() != X86::CMP32ri)
        continue;
      MachineInstr &Cmp = *It;
      if (Cmp.getNumOperands() < 2 || !Cmp.getOperand(1).isImm())
        continue;
      int64_t TripCount = Cmp.getOperand(1).getImm();
      if (TripCount < 1 || TripCount > 5)
        continue;

      auto Next = std::next(It);
      if (Next == MBB.end() || Next->getOpcode() != X86::JCC_1)
        continue;
      MachineInstr &Jcc = *Next;
      if (Jcc.getOperand(1).getImm() != 13) // COND_GE
        continue;
      MachineBasicBlock *ExitBB = Jcc.getOperand(0).getMBB();
      if (!ExitBB || ExitBB == &MBB)
        continue;

      auto Next2 = std::next(Next);
      if (Next2 == MBB.end() || Next2->getOpcode() != X86::JMP_1)
        continue;
      MachineInstr &JmpBack = *Next2;
      if (JmpBack.getOperand(0).getMBB() != &MBB)
        continue;

      Cmp.eraseFromParent();
      Jcc.eraseFromParent();
      JmpBack.eraseFromParent();

      MBB.removeSuccessor(&MBB);
      while (MBB.succ_size() > 0) {
        MachineBasicBlock *Succ = *MBB.succ_begin();
        MBB.removeSuccessor(Succ);
      }
      MBB.addSuccessor(ExitBB);

      BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(X86::JMP_1)).addMBB(ExitBB);

      return true;
    }
    return false;
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass> X("example-x86", "loop unrolling pass", false,
                                   false);