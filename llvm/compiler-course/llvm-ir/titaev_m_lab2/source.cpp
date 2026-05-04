#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

class RemainderTransformVisitor
    : public InstVisitor<RemainderTransformVisitor, bool> {
public:
  bool run(Function &F) {
    bool Changed = false;
    for (auto &BB : F) {
      for (auto I = BB.begin(), E = BB.end(); I != E;) {
        Instruction &Inst = *I++;
        Changed |= visit(Inst);
      }
    }
    return Changed;
  }

  bool visitInstruction(Instruction &I) { return false; }

  bool visitSRem(BinaryOperator &I) {
    IRBuilder<> Builder(&I);
    Value *Op0 = I.getOperand(0);
    Value *Op1 = I.getOperand(1);

    Value *Quotient = Builder.CreateSDiv(Op0, Op1, "ext.s.div");
    Value *Product = Builder.CreateMul(Quotient, Op1, "ext.s.mul");
    Value *Result = Builder.CreateSub(Op0, Product, "ext.s.rem");

    I.replaceAllUsesWith(Result);
    I.eraseFromParent();
    return true;
  }

  bool visitURem(BinaryOperator &I) {
    IRBuilder<> Builder(&I);
    Value *Op0 = I.getOperand(0);
    Value *Op1 = I.getOperand(1);

    Value *Quotient = Builder.CreateUDiv(Op0, Op1, "ext.u.div");
    Value *Product = Builder.CreateMul(Quotient, Op1, "ext.u.mul");
    Value *Result = Builder.CreateSub(Op0, Product, "ext.u.rem");

    I.replaceAllUsesWith(Result);
    I.eraseFromParent();
    return true;
  }

  bool visitFRem(BinaryOperator &I) {
    IRBuilder<> Builder(&I);
    Value *Op0 = I.getOperand(0);
    Value *Op1 = I.getOperand(1);

    Value *FDiv = Builder.CreateFDiv(Op0, Op1, "ext.f.div");
    Value *Trunc = Builder.CreateUnaryIntrinsic(Intrinsic::trunc, FDiv, nullptr,
                                                "ext.f.trunc");
    Value *FMul = Builder.CreateFMul(Trunc, Op1, "ext.f.mul");
    Value *FSub = Builder.CreateFSub(Op0, FMul, "ext.f.rem");

    I.replaceAllUsesWith(FSub);
    I.eraseFromParent();
    return true;
  }
};

struct RemDecompositionPass : public PassInfoMixin<RemDecompositionPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    RemainderTransformVisitor Visitor;
    if (Visitor.run(F))
      return PreservedAnalyses::none();
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "RemDecompositionPlugin", "3.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "decompose-remainder") {
                    FPM.addPass(RemDecompositionPass());
                    return true;
                  }
                  return false;
                });
          }};
}