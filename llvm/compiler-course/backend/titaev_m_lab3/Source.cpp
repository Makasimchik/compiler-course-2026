#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;

namespace {
class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    SmallVector<MachineBasicBlock *, 4> LoopBlocks;

    for (auto &MBB : MF) {
      for (auto *Succ : MBB.successors()) {
        if (Succ == &MBB) {
          LoopBlocks.push_back(&MBB);
          break;
        }
      }
    }

    for (auto *MBB : LoopBlocks)
      Changed |= tryUnrollLoop(MBB, TII);

    return Changed;
  }

private:
  static constexpr int MaxUnrollLimit = 5;

  bool tryUnrollLoop(MachineBasicBlock *MBB, const TargetInstrInfo *TII) {
    int TripCount = -1;
    MachineInstr *CmpInstr = nullptr;
    MachineInstr *JccInstr = nullptr;

    for (auto &MI : *MBB) {
      unsigned Opc = MI.getOpcode();
      if (Opc == X86::CMP32ri8 || Opc == X86::CMP32ri) {
        CmpInstr = &MI;
        for (const auto &Op : MI.operands()) {
          if (Op.isImm()) {
            TripCount = Op.getImm();
            break;
          }
        }
      }
      if (MI.isConditionalBranch())
        JccInstr = &MI;
    }

    if (TripCount <= 1 || TripCount > MaxUnrollLimit || !CmpInstr || !JccInstr)
      return false;

    SmallVector<MachineInstr *, 8> BodyInsts;
    for (auto &MI : *MBB) {
      if (MI.isTerminator() || &MI == CmpInstr)
        continue;
      BodyInsts.push_back(&MI);
    }

    MachineBasicBlock::iterator InsertPt = CmpInstr->getIterator();
    for (int i = 1; i < TripCount; ++i) {
      for (auto *OriginalMI : BodyInsts) {
        MachineInstr *Cloned = MBB->getParent()->CloneMachineInstr(OriginalMI);
        MBB->insert(InsertPt, Cloned);
      }
    }

    JccInstr->eraseFromParent();
    CmpInstr->eraseFromParent();
    MBB->removeSuccessor(MBB);

    return true;
  }
};

char ExamplePass::ID = 0;
} // namespace

static RegisterPass<ExamplePass> X("example-x86", "X86 Loop Unrolling Pass",
                                   false, false);