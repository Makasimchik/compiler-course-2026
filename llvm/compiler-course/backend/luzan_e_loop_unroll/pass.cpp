#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

using namespace llvm;

namespace {

struct LoopDescriptor {
  MachineBasicBlock *Preheader;
  MachineBasicBlock *Header;
  MachineBasicBlock *Latch;
  MachineBasicBlock *Exit;
};

// searching for virtual reg using INC32r/64r in the loop Header
Register findInductionVar(MachineBasicBlock *MBB) {
  for (MachineInstr &MI : *MBB)
    if (MI.getOpcode() == X86::CMP32ri || MI.getOpcode() == X86::CMP32ri8)
      return MI.getOperand(0).getReg();
  return Register();
}

// get loop Imm
int64_t getTripCount(MachineBasicBlock *MBB) {
  for (MachineInstr &MI : *MBB) {
    switch (MI.getOpcode()) {
    case X86::CMP32ri:
    case X86::CMP32ri8:
    case X86::CMP64ri32:
    case X86::CMP64ri8:
      for (const MachineOperand &Op : MI.operands())
        if (Op.isImm() && Op.getImm() >= 0) // get constant only
          return Op.getImm() + 1; // +1 because current MIR using JCC 15 ~ JG
                                  // (Jump if Greater)
      break;
    default:
      break;
    }
  }
  outs() << "[skip] no recognizable CMP in loop\n";
  return -1;
}

SmallVector<MachineBasicBlock *, 16>
collectLoopBlocks(MachineBasicBlock *Header, MachineBasicBlock *Exit) {
  SmallVector<MachineBasicBlock *, 16> Blocks;
  SmallPtrSet<MachineBasicBlock *, 16> Visited;
  SmallVector<MachineBasicBlock *, 16> Worklist = {Header};

  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.pop_back_val();
    if (!Visited.insert(MBB).second)
      continue;
    if (MBB == Exit)
      continue;
    Blocks.push_back(MBB);
    for (MachineBasicBlock *Succ : MBB->successors())
      Worklist.push_back(Succ);
  }
  return Blocks;
}

bool unrollLoop(const LoopDescriptor &LD, MachineFunction &MF) {
  MachineBasicBlock *Preheader = LD.Preheader;
  MachineBasicBlock *Header = LD.Header;
  MachineBasicBlock *Latch = LD.Latch;
  MachineBasicBlock *Exit = LD.Exit;

  // because after changing cfg MachineLoopInfo invalidates
  SmallVector<MachineBasicBlock *, 16> BodyBlocks =
      collectLoopBlocks(Header, Exit);
  int64_t TripCount = getTripCount(Header); // for this MIR
  if (TripCount < 1 || TripCount > 8) {
    outs() << "  [skip] bad trip count: " << TripCount << "\n";
    return false;
  }

  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  Register IndVar = findInductionVar(Header);

  // gen new bb and link to pre-header
  MachineBasicBlock *UnrollMBB = MF.CreateMachineBasicBlock();
  MF.insert(std::next(Preheader->getIterator()), UnrollMBB);

  for (int64_t Iter = 0; Iter < TripCount; ++Iter) {
    // insetring const
    Register IterReg = MRI.createVirtualRegister(MRI.getRegClass(IndVar));
    BuildMI(*UnrollMBB, UnrollMBB->end(), DebugLoc(), TII->get(X86::MOV32ri),
            IterReg)
        .addImm(Iter);

    // copy all instructions
    for (MachineBasicBlock *MBB : BodyBlocks) {
      for (MachineInstr &MI : *MBB) {
        if (MI.isBranch() || MI.isTerminator() || MI.isDebugInstr())
          continue;

        if (MI.getOpcode() == X86::INC32r)
          // || MI.getOpcode() == X86::CMP32ri ||
          // MI.getOpcode() == X86::CMP32ri8 ||
          // MI.getOpcode() == X86::CMP64ri32 ||
          // MI.getOpcode() == X86::CMP64ri8)
          if (MI.getOperand(0).getReg() == IndVar)
            continue;

        // replace indVar (counter in the loop) with const (Imm)
        MachineInstr *NewMI = MF.CloneMachineInstr(&MI);
        for (MachineOperand &MO : NewMI->operands())
          if (MO.isReg() && MO.getReg() == IndVar)
            MO.setReg(IterReg);
        UnrollMBB->push_back(NewMI);
      }
    }
  }

  // CFG change: Preheader -> UnrollMBB -> Exit
  TII->removeBranch(*Preheader);
  Preheader->removeSuccessor(Header);
  Preheader->addSuccessor(UnrollMBB);
  BuildMI(*Preheader, Preheader->end(), DebugLoc(), TII->get(X86::JMP_1))
      .addMBB(UnrollMBB);

  UnrollMBB->addSuccessor(Exit);
  Exit->replacePhiUsesWith(Latch, UnrollMBB);
  BuildMI(*UnrollMBB, UnrollMBB->end(), DebugLoc(), TII->get(X86::JMP_1))
      .addMBB(Exit);

  // erase BodyBlocks
  for (MachineBasicBlock *MBB : BodyBlocks) {
    while (!MBB->succ_empty())
      MBB->removeSuccessor(MBB->succ_begin());
    while (!MBB->pred_empty())
      (*MBB->pred_begin())->removeSuccessor(MBB);
    MBB->eraseFromParent();
  }

  return true;
}

void processLoop(MachineLoop *Loop, MachineFunction &MF,
                 SmallVector<LoopDescriptor, 8> &Loops) {
  // recurse into subloops
  for (MachineLoop *Sub : Loop->getSubLoops())
    processLoop(Sub, MF, Loops);

  MachineBasicBlock *Preheader = Loop->getLoopPreheader();
  MachineBasicBlock *Header = Loop->getHeader();
  MachineBasicBlock *Latch = Loop->getLoopLatch();
  MachineBasicBlock *Exit = Loop->getExitBlock();

  if (!Preheader || !Header || !Latch || !Exit) {
    outs() << "[skip] missing CFG anchor\n";
    return;
  }

  Loops.push_back({Preheader, Header, Latch, Exit});
}

class LuzanELoopUnroll : public MachineFunctionPass {
public:
  static char ID;
  LuzanELoopUnroll() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    // AU.addPreserved<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();

    SmallVector<LoopDescriptor, 8> Loops;
    for (MachineLoop *Top : MLI)
      processLoop(Top, MF, Loops);

    bool Changed = false;
    for (const LoopDescriptor &LD : Loops)
      Changed |= unrollLoop(LD, MF);

    return Changed;
  }
};

char LuzanELoopUnroll::ID = 0;

} // namespace

static RegisterPass<LuzanELoopUnroll> X("luzan_e_loop_unroll-x86",
                                        "X86 Full Loop Unroll", false, false);