#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include <algorithm>
#include <cstdint>
#include <optional>

using namespace llvm;

namespace {
class LoopUnrollingPass : public MachineFunctionPass {
public:
  static char ID;
  static constexpr unsigned MaxUnrollCount = 5;

  LoopUnrollingPass() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addUsedIfAvailable<LoopInfoWrapperPass>();
    AU.addUsedIfAvailable<ScalarEvolutionWrapperPass>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  static bool shouldCloneInstruction(const MachineInstr &MI) {
    return !MI.isPHI() && !MI.isTerminator() && !MI.isDebugInstr();
  }

  static bool isBlockInSubLoop(const MachineLoop *Loop,
                               const MachineBasicBlock *MBB) {
    for (auto It = Loop->begin(), End = Loop->end(); It != End; ++It) {
      MachineLoop *SubLoop = *It;
      if (SubLoop->contains(MBB) || isBlockInSubLoop(SubLoop, MBB))
        return true;
    }
    return false;
  }

  // Консервативный фильтр: разворачиваем только такие циклы, где в собственном
  // теле нет загрузок, записей в память, вызовов и прочих инструкций с
  // неявными побочными эффектами.
  static bool isIndependentLoop(const MachineLoop *Loop) {
    for (MachineBasicBlock *MBB : Loop->getBlocks()) {
      if (isBlockInSubLoop(Loop, MBB))
        continue;

      for (const MachineInstr &MI : *MBB) {
        if (!shouldCloneInstruction(MI))
          continue;

        if (MI.mayLoad() || MI.mayStore() || MI.hasUnmodeledSideEffects() ||
            MI.isCall())
          return false;
      }
    }
    return true;
  }

  static bool definesRegisterExactly(const MachineInstr &MI, Register Reg) {
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isDef())
        continue;
      if (MO.getReg() == Reg && MO.getSubReg() == 0)
        return true;
    }
    return false;
  }

  static std::optional<int64_t> getConstantDefValue(const MachineInstr &MI,
                                                    Register Reg) {
    if (!definesRegisterExactly(MI, Reg))
      return std::nullopt;

    switch (MI.getOpcode()) {
    case X86::MOV32r0:
      return 0;
    case X86::MOV32ri:
    case X86::MOV32ri64:
    case X86::MOV64ri32:
    case X86::MOV64ri:
      if (MI.getNumOperands() > 1 && MI.getOperand(1).isImm())
        return MI.getOperand(1).getImm();
      return std::nullopt;
    default:
      return std::nullopt;
    }
  }

  static std::optional<int64_t> getSelfUpdateStep(const MachineInstr &MI,
                                                  Register Reg) {
    if (!definesRegisterExactly(MI, Reg) || MI.getNumOperands() < 3 ||
        !MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != Reg ||
        !MI.getOperand(2).isImm())
      return std::nullopt;

    switch (MI.getOpcode()) {
    case X86::ADD32ri:
    case X86::ADD64ri32:
      return MI.getOperand(2).getImm();
    case X86::SUB32ri:
    case X86::SUB64ri32:
      return -MI.getOperand(2).getImm();
    default:
      return std::nullopt;
    }
  }

  static std::optional<int64_t>
  findConstantRegisterValue(Register Reg, const MachineBasicBlock &MBB) {
    for (auto It = MBB.rbegin(), End = MBB.rend(); It != End; ++It) {
      if (auto Value = getConstantDefValue(*It, Reg))
        return Value;
      if (definesRegisterExactly(*It, Reg))
        return std::nullopt;
    }
    return std::nullopt;
  }

  static std::optional<int64_t> findUniqueSelfUpdate(const MachineFunction &MF,
                                                     const MachineLoop *Loop,
                                                     Register Reg) {
    std::optional<int64_t> Step;

    for (const MachineBasicBlock &MBB : MF) {
      if (!Loop->contains(&MBB) || isBlockInSubLoop(Loop, &MBB))
        continue;

      for (const MachineInstr &MI : MBB) {
        auto CandidateStep = getSelfUpdateStep(MI, Reg);
        if (!CandidateStep)
          continue;
        if (Step)
          return std::nullopt;
        Step = CandidateStep;
      }
    }

    return Step;
  }

  static const MachineInstr *
  findLastConditionalBranch(const MachineBasicBlock &MBB) {
    for (auto It = MBB.rbegin(), End = MBB.rend(); It != End; ++It) {
      if (It->getOpcode() == X86::JCC_1)
        return &*It;
    }
    return nullptr;
  }

  static MachineBasicBlock *findConditionBlock(MachineLoop *MLoop) {
    if (MachineBasicBlock *ControlBlock = MLoop->findLoopControlBlock())
      if (findLastConditionalBranch(*ControlBlock))
        return ControlBlock;

    if (MachineBasicBlock *Header = MLoop->getHeader())
      if (findLastConditionalBranch(*Header))
        return Header;

    if (MachineBasicBlock *Latch = MLoop->getLoopLatch())
      if (findLastConditionalBranch(*Latch))
        return Latch;

    for (MachineBasicBlock *MBB : MLoop->getBlocks())
      if (findLastConditionalBranch(*MBB))
        return MBB;

    return nullptr;
  }

  static const MachineInstr *findLastCompare(const MachineBasicBlock &MBB) {
    const MachineInstr *Compare = nullptr;
    for (const MachineInstr &MI : MBB) {
      switch (MI.getOpcode()) {
      case X86::CMP32rr:
      case X86::CMP32ri:
      case X86::CMP64rr:
      case X86::CMP64ri32:
        Compare = &MI;
        break;
      default:
        break;
      }
    }
    return Compare;
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

  static X86::CondCode swapSignedCond(X86::CondCode CC) {
    switch (CC) {
    case X86::COND_L:
      return X86::COND_G;
    case X86::COND_LE:
      return X86::COND_GE;
    case X86::COND_G:
      return X86::COND_L;
    case X86::COND_GE:
      return X86::COND_LE;
    default:
      return X86::COND_INVALID;
    }
  }

  static bool hasUpdateBeforeCompare(const MachineBasicBlock &MBB,
                                     const MachineInstr &CompareMI,
                                     Register Reg) {
    for (const MachineInstr &MI : MBB) {
      if (&MI == &CompareMI)
        return false;
      if (getSelfUpdateStep(MI, Reg))
        return true;
    }
    return false;
  }

  static uint64_t ceilDiv(uint64_t Numerator, uint64_t Denominator) {
    return (Numerator + Denominator - 1) / Denominator;
  }

  static std::optional<uint64_t> computeTripCount(int64_t Init, int64_t Bound,
                                                  int64_t Step,
                                                  X86::CondCode ContinueCC,
                                                  bool IsPostTested) {
    if (Step == 0)
      return std::nullopt;

    if (Step > 0) {
      const uint64_t PosStep = static_cast<uint64_t>(Step);
      switch (ContinueCC) {
      case X86::COND_L:
        if (Init >= Bound)
          return IsPostTested ? 1 : 0;
        return ceilDiv(static_cast<uint64_t>(Bound - Init), PosStep);
      case X86::COND_LE:
        if (Init > Bound)
          return IsPostTested ? 1 : 0;
        return static_cast<uint64_t>(Bound - Init) / PosStep + 1;
      default:
        return std::nullopt;
      }
    }

    const uint64_t PosStep = static_cast<uint64_t>(-Step);
    switch (ContinueCC) {
    case X86::COND_G:
      if (Init <= Bound)
        return IsPostTested ? 1 : 0;
      return ceilDiv(static_cast<uint64_t>(Init - Bound), PosStep);
    case X86::COND_GE:
      if (Init < Bound)
        return IsPostTested ? 1 : 0;
      return static_cast<uint64_t>(Init - Bound) / PosStep + 1;
    default:
      return std::nullopt;
    }
  }

  static std::optional<unsigned> normalizeTripCount(uint64_t TripCount) {
    if (TripCount == 0 || TripCount > MaxUnrollCount)
      return std::nullopt;
    return static_cast<unsigned>(TripCount);
  }

  std::optional<unsigned> getTripCountFromIR(MachineLoop *MLoop) {
    auto *LIWP = getAnalysisIfAvailable<LoopInfoWrapperPass>();
    auto *SEWP = getAnalysisIfAvailable<ScalarEvolutionWrapperPass>();
    if (!LIWP || !SEWP)
      return std::nullopt;

    MachineBasicBlock *Header = MLoop->getHeader();
    if (!Header || !Header->getBasicBlock())
      return std::nullopt;

    BasicBlock *IRHeader = const_cast<BasicBlock *>(Header->getBasicBlock());
    LoopInfo &LI = LIWP->getLoopInfo();
    llvm::Loop *IRLoop = LI.getLoopFor(IRHeader);
    if (!IRLoop || IRLoop->getHeader() != IRHeader)
      return std::nullopt;

    ScalarEvolution &SE = SEWP->getSE();
    const SCEV *BackedgeTakenCount = SE.getBackedgeTakenCount(IRLoop);
    if (isa<SCEVCouldNotCompute>(BackedgeTakenCount))
      return std::nullopt;

    const auto *ConstBackedgeTakenCount =
        dyn_cast<SCEVConstant>(BackedgeTakenCount);
    if (!ConstBackedgeTakenCount)
      return std::nullopt;

    uint64_t TripCount = ConstBackedgeTakenCount->getAPInt().getZExtValue() + 1;
    return normalizeTripCount(TripCount);
  }

  std::optional<unsigned> getTripCountFromMachine(MachineFunction &MF,
                                                  MachineLoopInfo &MLI,
                                                  MachineLoop *MLoop) {
    MachineBasicBlock *ControlBlock = findConditionBlock(MLoop);
    if (!ControlBlock)
      return std::nullopt;

    const MachineInstr *Branch = findLastConditionalBranch(*ControlBlock);
    const MachineInstr *Compare = findLastCompare(*ControlBlock);
    if (!Branch || !Compare || Branch->getNumOperands() < 2 ||
        !Branch->getOperand(0).isMBB() || !Branch->getOperand(1).isImm())
      return std::nullopt;

    MachineBasicBlock *Preheader = MLI.findLoopPreheader(
        MLoop, /*SpeculativePreheader=*/true, /*FindMultiLoopPreheader=*/true);
    if (!Preheader)
      return std::nullopt;

    X86::CondCode ContinueCC =
        static_cast<X86::CondCode>(Branch->getOperand(1).getImm());
    if (!MLoop->contains(Branch->getOperand(0).getMBB()))
      ContinueCC = invertSignedCond(ContinueCC);
    if (ContinueCC == X86::COND_INVALID)
      return std::nullopt;

    auto finalizeTripCount = [&](Register IVReg, int64_t Step, int64_t Bound,
                                 bool SwapCond) -> std::optional<unsigned> {
      auto Init = findConstantRegisterValue(IVReg, *Preheader);
      if (!Init)
        return std::nullopt;

      X86::CondCode EffectiveCC =
          SwapCond ? swapSignedCond(ContinueCC) : ContinueCC;
      if (EffectiveCC == X86::COND_INVALID)
        return std::nullopt;

      bool IsPostTested =
          ControlBlock != MLoop->getHeader() ||
          hasUpdateBeforeCompare(*ControlBlock, *Compare, IVReg);
      auto TripCount =
          computeTripCount(*Init, Bound, Step, EffectiveCC, IsPostTested);
      if (!TripCount)
        return std::nullopt;

      return normalizeTripCount(*TripCount);
    };

    switch (Compare->getOpcode()) {
    case X86::CMP32ri:
    case X86::CMP64ri32: {
      if (!Compare->getOperand(0).isReg() || !Compare->getOperand(1).isImm())
        return std::nullopt;

      Register IVReg = Compare->getOperand(0).getReg();
      auto Step = findUniqueSelfUpdate(MF, MLoop, IVReg);
      if (!Step)
        return std::nullopt;

      return finalizeTripCount(IVReg, *Step, Compare->getOperand(1).getImm(),
                               /*SwapCond=*/false);
    }
    case X86::CMP32rr:
    case X86::CMP64rr: {
      if (!Compare->getOperand(0).isReg() || !Compare->getOperand(1).isReg())
        return std::nullopt;

      Register LeftReg = Compare->getOperand(0).getReg();
      Register RightReg = Compare->getOperand(1).getReg();
      auto LeftStep = findUniqueSelfUpdate(MF, MLoop, LeftReg);
      auto RightStep = findUniqueSelfUpdate(MF, MLoop, RightReg);

      if (LeftStep && RightStep)
        return std::nullopt;
      if (!LeftStep && !RightStep)
        return std::nullopt;

      if (LeftStep) {
        auto Bound = findConstantRegisterValue(RightReg, *Preheader);
        if (!Bound)
          return std::nullopt;
        return finalizeTripCount(LeftReg, *LeftStep, *Bound,
                                 /*SwapCond=*/false);
      }

      auto Bound = findConstantRegisterValue(LeftReg, *Preheader);
      if (!Bound)
        return std::nullopt;
      return finalizeTripCount(RightReg, *RightStep, *Bound,
                               /*SwapCond=*/true);
    }
    default:
      return std::nullopt;
    }
  }

  std::optional<unsigned>
  getUnrollCount(MachineFunction &MF, MachineLoopInfo &MLI, MachineLoop *Loop) {
    if (auto TripCount = getTripCountFromIR(Loop))
      return *TripCount;
    if (auto TripCount = getTripCountFromMachine(MF, MLI, Loop))
      return *TripCount;
    return std::nullopt;
  }

  static void sanitizeClone(MachineInstr &MI) {
    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg())
        continue;
      if (MO.isUse())
        MO.setIsKill(false);
      if (MO.isDef())
        MO.setIsDead(false);
      MO.setIsUndef(false);
    }
  }

  bool cloneLoopBlocks(MachineFunction &MF, MachineLoop *Loop,
                       unsigned UnrollCount) {
    if (UnrollCount <= 1)
      return false;

    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      if (!Loop->contains(&MBB) || isBlockInSubLoop(Loop, &MBB))
        continue;

      SmallVector<MachineInstr *, 16> BlockInstrs;
      for (MachineInstr &MI : MBB) {
        if (shouldCloneInstruction(MI))
          BlockInstrs.push_back(&MI);
      }

      if (BlockInstrs.empty())
        continue;

      MachineBasicBlock::iterator InsertPt = MBB.getFirstTerminator();
      for (unsigned CopyIdx = 1; CopyIdx < UnrollCount; ++CopyIdx) {
        for (MachineInstr *MI : BlockInstrs) {
          MachineInstr *Clone = MF.CloneMachineInstr(MI);
          sanitizeClone(*Clone);
          MBB.insert(InsertPt, Clone);
        }
      }

      Changed = true;
    }

    return Changed;
  }

  bool processLoop(MachineFunction &MF, MachineLoopInfo &MLI,
                   MachineLoop *Loop) {
    bool Changed = false;

    // Рекурсивно обрабатываем вложенные циклы
    for (auto It = Loop->begin(), End = Loop->end(); It != End; ++It)
      Changed |= processLoop(MF, MLI, *It);

    if (!Loop || !isIndependentLoop(Loop))
      return Changed;

    auto UnrollCount = getUnrollCount(MF, MLI, Loop);
    if (!UnrollCount)
      return Changed;

    return cloneLoopBlocks(MF, Loop, *UnrollCount) || Changed;
  }

public:
  bool runOnMachineFunction(MachineFunction &MF) override {
    bool Changed = false;
    MachineLoopInfo &MLI = getAnalysis<MachineLoopInfoWrapperPass>().getLI();

    for (MachineLoop *Loop : MLI)
      Changed |= processLoop(MF, MLI, Loop);

    return Changed;
  }
};

char LoopUnrollingPass::ID = 0;
} // end anonymous namespace

static RegisterPass<LoopUnrollingPass>
    X("example-x86", "X86 Machine Loop Unroll Pass", false, false);
