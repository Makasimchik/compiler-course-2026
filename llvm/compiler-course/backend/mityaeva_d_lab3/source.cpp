#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include <cstdint>
#include <optional>

#define DEBUG_TYPE "mityaeva-lab3"

using namespace llvm;

namespace {

class LoopUnrollPass : public MachineFunctionPass {
public:
  static char ID;
  LoopUnrollPass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();
    for (MachineLoop *L : MLI) {
      Changed |= processLoop(L, MF, MLI);
    }
    return Changed;
  }

private:
  static const unsigned MaxUnroll = 5;

  bool processLoop(MachineLoop *L, MachineFunction &MF, MachineLoopInfo &MLI) {
    bool Changed = false;
    for (MachineLoop *SubLoop : *L)
      Changed |= processLoop(SubLoop, MF, MLI);
    Changed |= unrollLoopIfProfitable(L, MF, MLI);
    return Changed;
  }

  bool unrollLoopIfProfitable(MachineLoop *L, MachineFunction &MF,
                              MachineLoopInfo &MLI) {
    auto TripCount = computeTripCount(L, MF, MLI);
    if (!TripCount || *TripCount > MaxUnroll || *TripCount <= 1)
      return false;

    LLVM_DEBUG(dbgs() << "Unrolling loop in " << MF.getName()
                      << " with trip count " << *TripCount << "\n");

    return cloneLoopBodies(L, MF, *TripCount, MLI);
  }

  static X86::CondCode invertSignedCond(X86::CondCode CC) {
    switch (CC) {
    case X86::COND_L:
      return X86::COND_GE;
    case X86::COND_LE:
      return X86::COND_G;
    case X86::COND_G:
      return X86::COND_LE;
    case X86::COND_GE:
      return X86::COND_L;
    default:
      return X86::COND_INVALID;
    }
  }

  std::optional<unsigned> computeTripCount(MachineLoop *L, MachineFunction &MF,
                                           MachineLoopInfo &MLI) {
    MachineBasicBlock *Header = L->getHeader();
    MachineBasicBlock *Latch = L->getLoopLatch();
    MachineBasicBlock *Preheader = MLI.findLoopPreheader(L);
    if (!Header || !Latch || !Preheader)
      return std::nullopt;

    MachineInstr *CmpMI = nullptr;
    Register CmpReg;
    int64_t Bound = 0;
    for (MachineInstr &MI : *Header) {
      if (MI.getOpcode() == X86::CMP32ri || MI.getOpcode() == X86::CMP64ri32) {
        if (MI.getOperand(0).isReg() && MI.getOperand(1).isImm()) {
          CmpMI = &MI;
          CmpReg = MI.getOperand(0).getReg();
          Bound = MI.getOperand(1).getImm();
          break;
        }
      }
    }
    if (!CmpMI) {
      for (MachineInstr &MI : *Latch) {
        if (MI.getOpcode() == X86::CMP32ri ||
            MI.getOpcode() == X86::CMP64ri32) {
          if (MI.getOperand(0).isReg() && MI.getOperand(1).isImm()) {
            CmpMI = &MI;
            CmpReg = MI.getOperand(0).getReg();
            Bound = MI.getOperand(1).getImm();
            break;
          }
        }
      }
    }
    if (!CmpMI)
      return std::nullopt;

    MachineInstr *UpdateMI = nullptr;
    int64_t Step = 0;
    for (MachineBasicBlock *BB : L->blocks()) {
      if (MLI.getLoopFor(BB) != L)
        continue;
      for (MachineInstr &MI : *BB) {
        if ((MI.getOpcode() == X86::ADD32ri ||
             MI.getOpcode() == X86::ADD64ri32 ||
             MI.getOpcode() == X86::SUB32ri ||
             MI.getOpcode() == X86::SUB64ri32) &&
            MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
            MI.getOperand(0).isDef() && MI.getOperand(0).getReg() == CmpReg &&
            MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == CmpReg &&
            MI.getOperand(2).isImm()) {
          if (UpdateMI)
            return std::nullopt;
          UpdateMI = &MI;
          Step = MI.getOperand(2).getImm();
          if (MI.getOpcode() == X86::SUB32ri ||
              MI.getOpcode() == X86::SUB64ri32)
            Step = -Step;
        }
      }
    }
    if (!UpdateMI || Step == 0)
      return std::nullopt;

    int64_t Init = 0;
    bool FoundInit = false;
    for (auto It = Preheader->rbegin(), End = Preheader->rend(); It != End;
         ++It) {
      const MachineInstr &MI = *It;
      if (MI.getOpcode() == X86::MOV32ri || MI.getOpcode() == X86::MOV64ri32 ||
          MI.getOpcode() == X86::MOV32r0) {
        if (MI.getNumOperands() > 0 && MI.getOperand(0).isReg() &&
            MI.getOperand(0).isDef() && MI.getOperand(0).getReg() == CmpReg) {
          if (MI.getOpcode() == X86::MOV32r0) {
            Init = 0;
            FoundInit = true;
          } else if (MI.getNumOperands() > 1 && MI.getOperand(1).isImm()) {
            Init = MI.getOperand(1).getImm();
            FoundInit = true;
          }
          break;
        }
      }
      if (MI.getNumOperands() > 0 && MI.getOperand(0).isReg() &&
          MI.getOperand(0).isDef() && MI.getOperand(0).getReg() == CmpReg)
        break;
    }
    if (!FoundInit)
      return std::nullopt;

    MachineInstr *JCC = nullptr;
    MachineBasicBlock *CmpBlock = CmpMI->getParent();
    for (MachineInstr &MI : *CmpBlock) {
      if (MI.getOpcode() == X86::JCC_1) {
        JCC = &MI;
        break;
      }
    }
    if (!JCC || JCC->getNumOperands() < 2 || !JCC->getOperand(1).isImm())
      return std::nullopt;

    X86::CondCode CC = static_cast<X86::CondCode>(JCC->getOperand(1).getImm());
    MachineBasicBlock *BranchTarget = JCC->getOperand(0).getMBB();
    bool IsContinueCond = (BranchTarget == Header || L->contains(BranchTarget));

    if (!IsContinueCond) {
      CC = invertSignedCond(CC);
      if (CC == X86::COND_INVALID)
        return std::nullopt;
    }

    if (CC != X86::COND_L && CC != X86::COND_LE && CC != X86::COND_G &&
        CC != X86::COND_GE)
      return std::nullopt;

    int64_t Delta;
    if (Step > 0) {
      if (CC == X86::COND_L) {
        if (Init >= Bound)
          return std::nullopt;
        Delta = Bound - Init;
      } else if (CC == X86::COND_LE) {
        if (Init > Bound)
          return std::nullopt;
        Delta = Bound - Init + 1;
      } else
        return std::nullopt;
    } else {
      if (CC == X86::COND_G) {
        if (Init <= Bound)
          return std::nullopt;
        Delta = Init - Bound;
      } else if (CC == X86::COND_GE) {
        if (Init < Bound)
          return std::nullopt;
        Delta = Init - Bound + 1;
      } else
        return std::nullopt;
    }

    uint64_t AbsStep = std::abs(Step);
    uint64_t TripCount = (Delta + AbsStep - 1) / AbsStep;
    if (TripCount > MaxUnroll || TripCount <= 1)
      return std::nullopt;
    return static_cast<unsigned>(TripCount);
  }

  bool cloneLoopBodies(MachineLoop *L, MachineFunction &MF,
                       unsigned UnrollCount, MachineLoopInfo &MLI) {
    bool Changed = false;
    for (MachineBasicBlock *MBB : L->blocks()) {
      if (MLI.getLoopFor(MBB) != L)
        continue;

      SmallVector<MachineInstr *, 16> InstrsToClone;
      for (MachineInstr &MI : *MBB) {
        if (!MI.isPHI() && !MI.isTerminator() && !MI.isDebugInstr())
          InstrsToClone.push_back(&MI);
      }
      if (InstrsToClone.empty())
        continue;

      MachineBasicBlock::iterator InsertPt = MBB->getFirstTerminator();
      for (unsigned Copy = 1; Copy < UnrollCount; ++Copy) {
        for (MachineInstr *Orig : InstrsToClone) {
          MachineInstr *Cloned = MF.CloneMachineInstr(Orig);
          for (MachineOperand &MO : Cloned->operands()) {
            if (MO.isReg()) {
              if (MO.isUse())
                MO.setIsKill(false);
              if (MO.isDef())
                MO.setIsDead(false);
              MO.setIsUndef(false);
            }
          }
          MBB->insert(InsertPt, Cloned);
        }
      }
      Changed = true;
    }
    return Changed;
  }
};

} // namespace

char LoopUnrollPass::ID = 0;
static RegisterPass<LoopUnrollPass>
    X("mityaeva-d-lab3", "Loop unrolling pass for lab3 (Mityaeva D)", false,
      false);