#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "x86-loop-unroller"

using namespace llvm;

namespace {

class X86LoopUnroller : public MachineFunctionPass {
public:
  static char ID;

  X86LoopUnroller() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    auto &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    bool Changed = false;

    for (auto *Loop : MLI) {
      Changed |= processLoop(Loop, MF);
    }

    return Changed;
  }

private:
  bool processLoop(MachineLoop *L, MachineFunction &MF) {
    bool Changed = false;

    for (auto *SubLoop : *L) {
      Changed |= processLoop(SubLoop, MF);
    }

    Changed |= unrollLoop(L, MF);

    return Changed;
  }

  bool unrollLoop(MachineLoop *L, MachineFunction &MF) {
    MachineBasicBlock *Header = L->getHeader();
    MachineBasicBlock *Latch = L->getLoopLatch();

    if (!Header || !Latch || L->getNumBlocks() > 1) {
      return false;
    }

    LLVM_DEBUG(dbgs() << "Unrolling loop in function: " << MF.getName()
                      << "\n");

    MachineRegisterInfo &MRI = MF.getRegInfo();
    // const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

    DenseMap<Register, Register> VRegMap;

    SmallVector<MachineInstr *, 8> InstrsToClone;
    for (auto &MI : *Header) {
      if (!MI.isTerminator()) {
        InstrsToClone.push_back(&MI);
      }
    }

    MachineBasicBlock::iterator InsertPos = Header->getFirstTerminator();

    for (auto *MI : InstrsToClone) {
      MachineInstr *Cloned = MF.CloneMachineInstr(MI);

      for (auto &MO : Cloned->operands()) {
        if (MO.isReg() && MO.getReg().isVirtual()) {
          Register OldReg = MO.getReg();
          if (VRegMap.find(OldReg) == VRegMap.end()) {
            VRegMap[OldReg] =
                MRI.createVirtualRegister(MRI.getRegClass(OldReg));
          }
          MO.setReg(VRegMap[OldReg]);
        }
      }
      Header->insert(InsertPos, Cloned);
    }

    return true;
  }
};

char X86LoopUnroller::ID = 0;

} // namespace

static RegisterPass<X86LoopUnroller>
    X("x86-unroll", "X86 Machine Loop Unroller", false, false);