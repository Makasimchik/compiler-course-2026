// LoadStoreElimination.cpp
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class LoadStoreEliminationPass
    : public PassInfoMixin<LoadStoreEliminationPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    bool Changed = false;

    for (BasicBlock &BB : F) {
      Changed |= eliminateInBasicBlock(BB);
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }

private:
  bool eliminateInBasicBlock(BasicBlock &BB) {
    DenseMap<Value *, Value *> LastStoredValue;
    DenseMap<Value *, StoreInst *> LastStoreInst;

    SmallVector<Instruction *, 16> ToErase;
    bool Changed = false;

    for (Instruction &I : make_early_inc_range(BB)) {
      if (auto *LI = dyn_cast<LoadInst>(&I)) {
        if (LI->isVolatile() || LI->isAtomic()) {
          LastStoredValue.clear();
          LastStoreInst.clear();
          continue;
        }

        Value *Ptr = LI->getPointerOperand();

        auto It = LastStoredValue.find(Ptr);
        if (It != LastStoredValue.end()) {
          Value *CachedVal = It->second;
          if (CachedVal->getType() == LI->getType()) {
            LI->replaceAllUsesWith(CachedVal);
            ToErase.push_back(LI);
            Changed = true;
            continue;
          }
        }

        LastStoredValue[Ptr] = LI;

        LastStoreInst.erase(Ptr);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (SI->isVolatile() || SI->isAtomic()) {
          LastStoredValue.clear();
          LastStoreInst.clear();
          continue;
        }

        Value *Ptr = SI->getPointerOperand();
        Value *StoredVal = SI->getValueOperand();

        auto StoreIt = LastStoreInst.find(Ptr);
        if (StoreIt != LastStoreInst.end() && StoreIt->second != nullptr) {
          ToErase.push_back(StoreIt->second);
          Changed = true;
        }

        LastStoredValue[Ptr] = StoredVal;
        LastStoreInst[Ptr] = SI;
      } else {
        if (I.mayReadOrWriteMemory() || I.mayHaveSideEffects()) {
          LastStoredValue.clear();
          LastStoreInst.clear();
        }
      }
    }

    for (Instruction *Inst : ToErase) {
      if (Inst->getParent() == &BB) {
        Inst->eraseFromParent();
      }
    }

    return Changed;
  }
};

} // namespace

llvm::PassPluginLibraryInfo getLoadStoreEliminationPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "loadStoreElimination", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "load-store-elim") {
                    FPM.addPass(LoadStoreEliminationPass());
                    return true;
                  }
                  return false;
                });
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {});
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLoadStoreEliminationPluginInfo();
}