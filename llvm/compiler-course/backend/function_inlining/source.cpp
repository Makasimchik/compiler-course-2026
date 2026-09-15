#include "X86.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {

constexpr unsigned MaxInlineInstructions = 15;
constexpr unsigned MaxRecursionDepth = 3;

class FunctionInliningPass : public ModulePass {
  using RecursiveGroupMap = DenseMap<const Function *, unsigned>;

public:
  static char ID;

  FunctionInliningPass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override {
    RecursiveDepths.clear();
    MachineModuleInfo &MMI =
        getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    const RecursiveGroupMap RecursiveGroups = collectRecursiveGroups(M, MMI);

    bool Changed = false;
    bool LocalChange = false;
    do {
      LocalChange = false;

      for (Function &F : M) {
        if (F.isDeclaration())
          continue;

        if (RecursiveGroups.find(&F) != RecursiveGroups.end())
          continue;

        MachineFunction *MF = MMI.getMachineFunction(F);
        if (MF == nullptr)
          continue;

        for (MachineBasicBlock &MBB : *MF) {
          for (auto It = MBB.begin(); It != MBB.end();) {
            MachineInstr &MI = *It++;
            const Function *Callee = getCalledFunction(MI);
            if (Callee == nullptr || Callee->isDeclaration())
              continue;

            MachineFunction *CalleeMF = MMI.getMachineFunction(*Callee);
            if (CalleeMF == nullptr)
              continue;

            if (!canInline(MI, *CalleeMF, RecursiveGroups))
              continue;

            inlineCall(*MF, MBB, MI, *CalleeMF, RecursiveGroups);
            Changed = true;
            LocalChange = true;
          }
        }
      }
    } while (LocalChange);

    return Changed;
  }

private:
  DenseMap<const MachineInstr *, unsigned> RecursiveDepths;

  static RecursiveGroupMap collectRecursiveGroups(Module &M,
                                                  MachineModuleInfo &MMI) {
    RecursiveGroupMap Groups;
    DenseMap<const Function *, SmallVector<const Function *, 4>> Edges;
    DenseMap<const Function *, unsigned> Indices;
    DenseMap<const Function *, unsigned> LowLinks;
    SmallVector<const Function *, 16> Stack;
    SmallPtrSet<const Function *, 16> OnStack;
    SmallVector<const Function *, 16> Nodes;
    unsigned NextIndex = 0;
    unsigned NextGroupId = 0;

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;

      MachineFunction *MF = MMI.getMachineFunction(F);
      if (MF == nullptr)
        continue;

      Nodes.push_back(&F);
      SmallPtrSet<const Function *, 4> SeenCallees;
      SmallVector<const Function *, 4> Callees;

      for (const MachineBasicBlock &MBB : *MF) {
        for (const MachineInstr &MI : MBB) {
          const Function *Callee = getCalledFunction(MI);
          if (Callee == nullptr || Callee->isDeclaration())
            continue;

          if (MMI.getMachineFunction(*Callee) == nullptr)
            continue;

          if (!SeenCallees.insert(Callee).second)
            continue;

          Callees.push_back(Callee);
        }
      }

      Edges[&F] = std::move(Callees);
    }

    auto Visit = [&](const Function *Start, auto &&Visit) -> void {
      Indices[Start] = NextIndex;
      LowLinks[Start] = NextIndex;
      ++NextIndex;

      Stack.push_back(Start);
      OnStack.insert(Start);

      for (const Function *Callee : Edges[Start]) {
        if (!Indices.contains(Callee)) {
          Visit(Callee, Visit);
          if (LowLinks[Start] > LowLinks[Callee])
            LowLinks[Start] = LowLinks[Callee];
          continue;
        }

        if (OnStack.contains(Callee) && LowLinks[Start] > Indices[Callee])
          LowLinks[Start] = Indices[Callee];
      }

      if (LowLinks[Start] != Indices[Start])
        return;

      SmallVector<const Function *, 4> Component;
      bool HasSelfCall = false;
      while (!Stack.empty()) {
        const Function *Current = Stack.pop_back_val();
        OnStack.erase(Current);
        Component.push_back(Current);

        for (const Function *Callee : Edges[Current]) {
          if (Callee == Current) {
            HasSelfCall = true;
            break;
          }
        }

        if (Current == Start)
          break;
      }

      if (Component.size() == 1 && !HasSelfCall)
        return;

      for (const Function *F : Component)
        Groups[F] = NextGroupId;

      ++NextGroupId;
    };

    for (const Function *F : Nodes) {
      if (!Indices.contains(F))
        Visit(F, Visit);
    }

    return Groups;
  }

  static const Function *getCalledFunction(const MachineInstr &MI) {
    if (!MI.isCall())
      return nullptr;

    for (const MachineOperand &Operand : MI.operands()) {
      if (!Operand.isGlobal())
        continue;

      return dyn_cast<Function>(Operand.getGlobal());
    }

    return nullptr;
  }

  static bool hasSingleBlock(const MachineFunction &MF) {
    auto It = MF.begin();
    if (It == MF.end())
      return false;

    ++It;
    return It == MF.end();
  }

  static unsigned countInstructions(const MachineFunction &MF) {
    unsigned Count = 0;

    for (const MachineBasicBlock &MBB : MF) {
      for (const MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || MI.isMetaInstruction())
          continue;

        ++Count;
      }
    }

    return Count;
  }

  bool canInline(MachineInstr &Call, const MachineFunction &CalleeMF,
                 const RecursiveGroupMap &RecursiveGroups) const {
    if (!hasSingleBlock(CalleeMF))
      return false;

    if (countInstructions(CalleeMF) > MaxInlineInstructions)
      return false;

    const Function *Callee = &CalleeMF.getFunction();
    auto GroupIt = RecursiveGroups.find(Callee);
    if (GroupIt == RecursiveGroups.end())
      return true;

    return RecursiveDepths.lookup(&Call) < MaxRecursionDepth;
  }

  static bool sameRecursiveGroup(const Function *Left, const Function *Right,
                                 const RecursiveGroupMap &RecursiveGroups) {
    if (Left == nullptr || Right == nullptr)
      return false;

    auto LeftIt = RecursiveGroups.find(Left);
    auto RightIt = RecursiveGroups.find(Right);
    if (LeftIt == RecursiveGroups.end() || RightIt == RecursiveGroups.end())
      return false;

    return LeftIt->second == RightIt->second;
  }

  static void remapVirtualRegisters(MachineInstr &MI,
                                    MachineRegisterInfo &CallerMRI,
                                    const MachineRegisterInfo &CalleeMRI,
                                    DenseMap<Register, Register> &RegisterMap) {
    for (MachineOperand &Operand : MI.operands()) {
      if (!Operand.isReg())
        continue;

      Register Reg = Operand.getReg();
      if (!Reg.isVirtual())
        continue;

      auto Mapping = RegisterMap.find(Reg);
      if (Mapping == RegisterMap.end()) {
        const TargetRegisterClass *RegClass = CalleeMRI.getRegClass(Reg);
        Register NewReg = CallerMRI.createVirtualRegister(RegClass);
        Mapping = RegisterMap.insert({Reg, NewReg}).first;
      }

      Operand.setReg(Mapping->second);
    }
  }

  void inlineCall(MachineFunction &CallerMF, MachineBasicBlock &CallerMBB,
                  MachineInstr &Call, const MachineFunction &CalleeMF,
                  const RecursiveGroupMap &RecursiveGroups) {
    const unsigned CurrentDepth = RecursiveDepths.lookup(&Call);
    const Function *Callee = &CalleeMF.getFunction();

    MachineRegisterInfo &CallerMRI = CallerMF.getRegInfo();
    const MachineRegisterInfo &CalleeMRI = CalleeMF.getRegInfo();
    DenseMap<Register, Register> RegisterMap;

    for (const MachineInstr &Original : CalleeMF.front()) {
      if (Original.isDebugInstr() || Original.isMetaInstruction() ||
          Original.isReturn()) {
        continue;
      }

      MachineInstr *Cloned = CallerMF.CloneMachineInstr(&Original);
      remapVirtualRegisters(*Cloned, CallerMRI, CalleeMRI, RegisterMap);
      CallerMBB.insert(Call, Cloned);

      const Function *NestedCallee = getCalledFunction(*Cloned);
      if (sameRecursiveGroup(Callee, NestedCallee, RecursiveGroups))
        RecursiveDepths[Cloned] = CurrentDepth + 1;
    }

    RecursiveDepths.erase(&Call);
    Call.eraseFromParent();
  }
};

char FunctionInliningPass::ID = 0;

} // namespace

static RegisterPass<FunctionInliningPass> X("function-inlining-backend",
                                            "Function inlining backend pass",
                                            false, false);