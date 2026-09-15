#define DEBUG_TYPE "LoopUnrollingPass"

#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// max iteration unroll count
static constexpr unsigned MaxUnrollCount = 5;

namespace {

class LoopUnrollingPass : public MachineFunctionPass {
public:
  static char ID;
  LoopUnrollingPass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();

    LLVM_DEBUG(dbgs() << "LoopUnrolling: " << MF.getName() << "\n");

    // collect all loops in post-order before changing CFG
    // (after eraseFromParent MLI is invalid)
    SmallVector<MachineLoop *, 16> Worklist;
    for (MachineLoop *Top : MLI)
      collectPostOrder(Top, Worklist);

    LLVM_DEBUG(dbgs() << " found loops: " << Worklist.size() << "\n");

    bool Changed = false;
    for (MachineLoop *L : Worklist)
      Changed |= unrollLoop(L, MF);

    return Changed;
  }

private:
  Register getInductionVar(MachineBasicBlock *Header) {
    for (MachineInstr &MI : *Header) {
      unsigned Opc = MI.getOpcode();
      if (Opc == X86::CMP32ri || Opc == X86::CMP32ri8 ||
          Opc == X86::CMP64ri32 || Opc == X86::CMP64ri8)
        return MI.getOperand(0).getReg();
    }
    return Register();
  }

  int64_t getTripCount(MachineBasicBlock *Header) {
    int64_t Imm = -1;
    int64_t JccCond = -1;

    for (MachineInstr &MI : *Header) {
      unsigned Opc = MI.getOpcode();

      if (Opc == X86::CMP32ri || Opc == X86::CMP32ri8 ||
          Opc == X86::CMP64ri32 || Opc == X86::CMP64ri8) {
        if (MI.getNumOperands() >= 2 && MI.getOperand(1).isImm())
          Imm = MI.getOperand(1).getImm();
      }

      // JCC_1: operand[0]=target_bb, operand[1]=cond, operand[2]=eflags
      if (Opc == X86::JCC_1) {
        if (MI.getNumOperands() >= 2 && MI.getOperand(1).isImm())
          JccCond = MI.getOperand(1).getImm();
      }
    }

    if (Imm < 0 || JccCond < 0)
      return -1;

    // X86::CondCode:
    //   15 = COND_G  (JG,  signed >):  exit when i >  Imm → tripCount = Imm + 1
    //   13 = COND_GE (JGE, signed >=): exit when i >= Imm → tripCount = Imm
    switch (JccCond) {
    case 15:
      return Imm + 1; // JG
    case 13:
      return Imm; // JGE
    default:
      LLVM_DEBUG(dbgs() << " skip: unsupported JCC code: " << JccCond << "\n");
      return -1;
    }
  }

  bool isValidTripCount(unsigned TripCount) {
    if (TripCount < 1) {
      LLVM_DEBUG(dbgs() << "skip: invalid tripCount\n");
      return false;
    }
    if (TripCount > MaxUnrollCount) {
      LLVM_DEBUG(dbgs() << " skip: tripCount=" << TripCount
                        << " exceeds MaxUnrollCount=" << MaxUnrollCount
                        << "\n");
      return false;
    }
    return true;
  }

  // DFS-based body block collection
  SmallVector<MachineBasicBlock *, 16>
  collectLoopBodyBlocks(MachineBasicBlock *Entry, MachineBasicBlock *Sink) {
    SmallVector<MachineBasicBlock *, 16> Result;
    if (!Entry)
      return Result;

    SmallPtrSet<MachineBasicBlock *, 16> Visited;
    SmallVector<MachineBasicBlock *, 16> Stack;
    Stack.push_back(Entry);

    while (!Stack.empty()) {
      MachineBasicBlock *Current = Stack.pop_back_val();
      if (Current == Sink)
        continue;
      if (!Visited.insert(Current).second)
        continue;
      Result.push_back(Current);
      for (MachineBasicBlock *Succ : Current->successors()) {
        Stack.push_back(Succ);
      }
    }
    return Result;
  }

  // returns true if instruction is loop condition (CMP) or INC induction var
  bool isLoopOverhead(const MachineInstr &MI, Register IndVar) {
    unsigned Opc = MI.getOpcode();

    if (Opc == X86::CMP32ri || Opc == X86::CMP32ri8 || Opc == X86::CMP64ri32 ||
        Opc == X86::CMP64ri8)
      return true;

    if ((Opc == X86::INC32r || Opc == X86::INC64r) && MI.getNumOperands() > 0 &&
        MI.getOperand(0).isReg() && MI.getOperand(0).getReg() == IndVar)
      return true;

    return false;
  }

  bool validateLoopCFG(MachineBasicBlock *Preheader, MachineBasicBlock *Header,
                       MachineBasicBlock *Latch, MachineBasicBlock *Exit) {
    if (!Preheader) {
      LLVM_DEBUG(dbgs() << " skip: invalid preheader\n");
      return false;
    }

    if (!Header) {
      LLVM_DEBUG(dbgs() << " skip: invalid header\n");
      return false;
    }

    if (!Latch) {
      LLVM_DEBUG(dbgs() << " skip: invalid latch\n");
      return false;
    }

    if (!Exit) {
      LLVM_DEBUG(dbgs() << " skip: invalid exit\n");
      return false;
    }
    return true;
  }

  bool unrollLoop(MachineLoop *L, MachineFunction &MF) {
    MachineBasicBlock *Preheader = L->getLoopPreheader();
    MachineBasicBlock *Header = L->getHeader();
    MachineBasicBlock *Latch = L->getLoopLatch();
    MachineBasicBlock *Exit = findLoopExitBlock(L);

    if (!validateLoopCFG(Preheader, Header, Latch, Exit))
      return false;

    unsigned TripCount = getTripCount(Header);
    if (!isValidTripCount(TripCount))
      return false;

    Register IndVar = getInductionVar(Header);
    if (!IndVar.isValid()) {
      LLVM_DEBUG(dbgs() << " skip: induction var invalid\n");
      return false;
    }

    LLVM_DEBUG(dbgs() << " unrolling: tripCount=" << TripCount << "\n");

    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();

    // collect body blocks before changing CFG
    SmallVector<MachineBasicBlock *, 16> LoopBlocks =
        collectLoopBodyBlocks(Header, Exit);

    // build new control flow graph
    MachineBasicBlock *UnrollBody = MF.CreateMachineBasicBlock();
    MF.insert(std::next(Preheader->getIterator()), UnrollBody);

    // insert TripCount body copies
    for (unsigned Iter = 0; Iter < TripCount; ++Iter) {
      // iteration const in separate vreg
      Register IterReg = MRI.createVirtualRegister(MRI.getRegClass(IndVar));
      BuildMI(*UnrollBody, UnrollBody->end(), DebugLoc(),
              TII->get(X86::MOV32ri), IterReg)
          .addImm(Iter);

      for (MachineBasicBlock *MBB : LoopBlocks) {
        for (MachineInstr &MI : *MBB) {
          // skip terminators, debug and loop overhead-instructions
          if (MI.isTerminator() || MI.isDebugInstr())
            continue;
          if (isLoopOverhead(MI, IndVar))
            continue;

          MachineInstr *NewMI = MF.CloneMachineInstr(&MI);
          // change IndVar to IterReg in all operands
          for (MachineOperand &MO : NewMI->operands())
            if (MO.isReg() && MO.getReg() == IndVar)
              MO.setReg(IterReg);
          UnrollBody->push_back(NewMI);
        }
      }
    }

    // connect preheader to unrolled body
    Preheader->removeSuccessor(Header);
    Preheader->addSuccessor(UnrollBody);
    BuildMI(*Preheader, Preheader->end(), DebugLoc(), TII->get(X86::JMP_1))
        .addMBB(UnrollBody);

    // connect unrolled body to exit
    UnrollBody->addSuccessor(Exit);
    Exit->replacePhiUsesWith(Latch, UnrollBody);
    BuildMI(*UnrollBody, UnrollBody->end(), DebugLoc(), TII->get(X86::JMP_1))
        .addMBB(Exit);

    // remove original loop
    detachBlocks(LoopBlocks);

    return true;
  }

  // remove all predecessors/successors and erase blocks
  void detachBlocks(SmallVectorImpl<MachineBasicBlock *> &Blocks) {
    for (MachineBasicBlock *BB : Blocks) {
      while (!BB->succ_empty())
        BB->removeSuccessor(BB->succ_begin());
      while (!BB->pred_empty())
        (*BB->pred_begin())->removeSuccessor(BB);
      BB->eraseFromParent();
    }
  }

  void collectPostOrder(MachineLoop *L,
                        SmallVectorImpl<MachineLoop *> &Result) {
    for (MachineLoop *Sub : L->getSubLoops())
      collectPostOrder(Sub, Result);
    Result.push_back(L);
  }

  MachineBasicBlock *findLoopExitBlock(MachineLoop *L) {
    if (MachineBasicBlock *Exit = L->getExitBlock())
      return Exit;

    // Fallback: searching for Latch successors not included in loop body
    MachineBasicBlock *Latch = L->getLoopLatch();
    MachineBasicBlock *Header = L->getHeader();

    if (Header) {
      for (MachineBasicBlock *Succ : Header->successors()) {
        if (!L->contains(Succ))
          return Succ;
      }
    }

    if (Latch && Latch != Header) {
      for (MachineBasicBlock *Succ : Latch->successors()) {
        if (!L->contains(Succ))
          return Succ;
      }
    }
    return nullptr;
  }
};

char LoopUnrollingPass::ID = 0;

} // namespace

static RegisterPass<LoopUnrollingPass> X("loop-unrolling-x86",
                                         "Loop unrolling pass", false, false);
