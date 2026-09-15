#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
struct DorofeevLoopUnroll : public MachineFunctionPass {
  static char ID;

  const unsigned MaxIterations = 5;

  DorofeevLoopUnroll() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    bool Changed = false;

    for (auto *L : MLI) {
      Changed |= processLoop(L, MF);
    }
    return Changed;
  }

  bool processLoop(MachineLoop *L, MachineFunction &MF) {
    bool Changed = false;

    for (auto *SubL : L->getSubLoops()) {
      Changed |= processLoop(SubL, MF);
    }

    if (L->getNumBlocks() != 1)
      return Changed;

    MachineBasicBlock *MBB = L->getHeader();
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();

    unsigned TripCount = 0;
    bool HasTripCount = false;
    MachineBasicBlock::iterator InsertPt = MBB->getFirstTerminator();

    for (auto I = MBB->begin(), E = MBB->end(); I != E; ++I) {
      StringRef OpName = TII->getName(I->getOpcode());
      if (OpName.starts_with("CMP")) {
        InsertPt = I;
        for (auto &MO : I->operands()) {
          if (MO.isImm()) {
            TripCount = MO.getImm();
            HasTripCount = true;
            break;
          }
        }
      }
    }

    if (HasTripCount && TripCount > MaxIterations) {
      return Changed;
    }

    unsigned UnrollFactor = HasTripCount ? TripCount : MaxIterations;
    SmallVector<MachineInstr *, 8> BodyInstrs;
    for (auto &MI : *MBB) {
      if (!MI.isBranch() && !MI.isTerminator() && !MI.isDebugInstr() &&
          !TII->getName(MI.getOpcode()).starts_with("CMP")) {
        BodyInstrs.push_back(&MI);
      }
    }
    for (unsigned i = 1; i < UnrollFactor; ++i) {
      DenseMap<Register, Register> RegMap;

      for (auto *MI : BodyInstrs) {
        MachineInstrBuilder MIB = BuildMI(*MBB, InsertPt, MI->getDebugLoc(),
                                          TII->get(MI->getOpcode()));

        for (auto &MO : MI->operands()) {
          if (MO.isReg()) {
            Register Reg = MO.getReg();
            if (Reg.isVirtual()) {
              if (MO.isDef()) {
                Register NewReg =
                    MRI.createVirtualRegister(MRI.getRegClass(Reg));
                RegMap[Reg] = NewReg;
                MIB.addReg(NewReg, RegState::Define);
              } else {
                Register UseReg = RegMap.count(Reg) ? RegMap[Reg] : Reg;
                MIB.addReg(UseReg);
              }
            } else {
              MIB.add(MO);
            }
          } else {
            MIB.add(MO);
          }
        }
      }
      Changed = true;
    }

    return Changed;
  }
};
} // namespace

char DorofeevLoopUnroll::ID = 0;

static RegisterPass<DorofeevLoopUnroll>
    X("dorofeev-loop-unroll", "Backend Loop Unroll Pass", false, false);