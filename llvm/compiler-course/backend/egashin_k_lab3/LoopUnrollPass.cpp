#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;

namespace {
class EgashinLoopUnrollPass : public MachineFunctionPass {
public:
  static char ID;

  EgashinLoopUnrollPass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    const TargetInstrInfo *TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();

    bool Changed = false;
    for (MachineLoop *Loop : MLI)
      Changed |= processLoop(Loop, MF, *TII, MRI);

    return Changed;
  }

private:
  static constexpr unsigned MaxTripCount = 5;

  bool processLoop(MachineLoop *Loop, MachineFunction &MF,
                   const TargetInstrInfo &TII, MachineRegisterInfo &MRI) {
    bool Changed = false;
    for (MachineLoop *SubLoop : Loop->getSubLoops())
      Changed |= processLoop(SubLoop, MF, TII, MRI);

    MachineBasicBlock *Header = Loop->getHeader();
    MachineBasicBlock *Exit = Loop->getExitBlock();
    if (!Header || !Exit || Loop->getNumBlocks() != 1 ||
        Loop->getLoopLatch() != Header)
      return Changed;

    unsigned TripCount = 0;
    MachineInstr *Cmp = nullptr;
    if (!getTripCount(*Header, TripCount, Cmp) || TripCount <= 1 ||
        TripCount > MaxTripCount)
      return Changed;

    SmallVector<MachineInstr *, 16> Body;
    Body.reserve(Header->size());
    for (MachineInstr &MI : *Header) {
      if (&MI == Cmp || MI.isBranch() || MI.isTerminator() || MI.isDebugInstr())
        continue;
      Body.push_back(&MI);
    }
    if (Body.empty())
      return Changed;

    DenseMap<Register, Register> LatestReg;
    for (MachineInstr *MI : Body) {
      for (const MachineOperand &MO : MI->operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isVirtual())
          LatestReg[MO.getReg()] = MO.getReg();
      }
    }

    MachineBasicBlock::iterator InsertPt = Header->getFirstTerminator();
    for (unsigned Iter = 1; Iter < TripCount; ++Iter) {
      for (MachineInstr *MI : Body) {
        MachineInstr *Clone = MF.CloneMachineInstr(MI);
        for (MachineOperand &MO : Clone->operands()) {
          if (!MO.isReg() || !MO.getReg().isVirtual())
            continue;

          Register Reg = MO.getReg();
          if (MO.isUse()) {
            if (Register Mapped = LatestReg.lookup(Reg))
              MO.setReg(Mapped);
            MO.setIsKill(false);
          }
          if (MO.isDef()) {
            Register NewReg = MRI.createVirtualRegister(MRI.getRegClass(Reg));
            MO.setReg(NewReg);
            LatestReg[Reg] = NewReg;
          }
        }
        Header->insert(InsertPt, Clone);
      }
    }

    TII.removeBranch(*Header);
    if (Cmp)
      Cmp->eraseFromParent();

    while (Header->succ_size() > 0)
      Header->removeSuccessor(Header->succ_begin());
    Header->addSuccessor(Exit);

    if (!Header->isLayoutSuccessor(Exit)) {
      BuildMI(*Header, Header->end(), DebugLoc(), TII.get(X86::JMP_1))
          .addMBB(Exit);
    }

    return true;
  }

  bool getTripCount(MachineBasicBlock &MBB, unsigned &TripCount,
                    MachineInstr *&Cmp) const {
    for (MachineInstr &MI : reverse(MBB)) {
      switch (MI.getOpcode()) {
      case X86::CMP32ri:
      case X86::CMP32ri8:
      case X86::CMP64ri32:
      case X86::CMP64ri8:
        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isImm() || MO.getImm() < 0)
            continue;
          TripCount = static_cast<unsigned>(MO.getImm());
          Cmp = &MI;
          return true;
        }
        break;
      default:
        break;
      }
    }
    return false;
  }
};
} // namespace

char EgashinLoopUnrollPass::ID = 0;

static RegisterPass<EgashinLoopUnrollPass>
    X("egashin-loop-unroll-x86", "Egashin backend loop unroll", false, false);
