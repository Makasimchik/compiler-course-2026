#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

namespace {

class DecomposeFRemPass : public PassInfoMixin<DecomposeFRemPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    bool Changed = false;
    SmallVector<Instruction *, 8> ToReplace;
    for (Instruction &I : instructions(F)) {
      unsigned Op = I.getOpcode();
      if (Op == Instruction::FRem || Op == Instruction::SRem ||
          Op == Instruction::URem)
        ToReplace.push_back(&I);
    }
    for (Instruction *I : ToReplace) {
      replaceRem(I);
      Changed = true;
    }
    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

private:
  void replaceRem(Instruction *Rem) {
    IRBuilder<> Builder(Rem);
    Value *A = Rem->getOperand(0);
    Value *B = Rem->getOperand(1);
    Type *Ty = Rem->getType();
    switch (Rem->getOpcode()) {
    case Instruction::FRem: {
      Value *Div = Builder.CreateFDiv(A, B);
      FunctionCallee TruncFn = Intrinsic::getOrInsertDeclaration(
          Rem->getModule(), Intrinsic::trunc, Ty);
      Value *TruncDiv = Builder.CreateCall(TruncFn, Div);
      Value *Mul = Builder.CreateFMul(TruncDiv, B);
      Value *Result = Builder.CreateFSub(A, Mul);
      Rem->replaceAllUsesWith(Result);
      break;
    }
    case Instruction::SRem: {
      Value *Div = Builder.CreateSDiv(A, B);
      Value *Mul = Builder.CreateMul(Div, B);
      Value *Result = Builder.CreateSub(A, Mul);
      Rem->replaceAllUsesWith(Result);
      break;
    }
    case Instruction::URem: {
      Value *Div = Builder.CreateUDiv(A, B);
      Value *Mul = Builder.CreateMul(Div, B);
      Value *Result = Builder.CreateSub(A, Mul);
      Rem->replaceAllUsesWith(Result);
      break;
    }
    default:
      llvm_unreachable("unknown remainder instruction");
    }
    Rem->eraseFromParent();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DecomposeFRem", "v0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "decompose-frem") {
                    FPM.addPass(DecomposeFRemPass());
                    return true;
                  }
                  return false;
                });
          }};
}