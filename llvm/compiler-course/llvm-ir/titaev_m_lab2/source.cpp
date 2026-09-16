#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

static StringRef getFunctionName(unsigned Opcode) {
  switch (Opcode) {
  case Instruction::Add:
    return "add";
  case Instruction::Sub:
    return "sub";
  case Instruction::Mul:
    return "mul";
  case Instruction::SDiv:
    return "sdiv";
  case Instruction::UDiv:
    return "udiv";
  default:
    return "";
  }
}

struct ReplaceBinaryOpsPass : public PassInfoMixin<ReplaceBinaryOpsPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    bool Changed = false;

    for (Function &F : M) {
      for (BasicBlock &BB : F) {
        for (auto It = BB.begin(), End = BB.end(); It != End;) {
          Instruction &I = *It++;

          auto *BinOp = dyn_cast<BinaryOperator>(&I);
          if (!BinOp)
            continue;

          StringRef FunctionName = getFunctionName(BinOp->getOpcode());

          if (FunctionName.empty())
            continue;

          Function *ReplacementFunction = M.getFunction(FunctionName);

          if (!ReplacementFunction)
            continue;

          if (&F == ReplacementFunction)
            continue;

          if (ReplacementFunction->arg_size() != 2)
            continue;

          FunctionType *FunctionType = ReplacementFunction->getFunctionType();

          Value *Op0 = BinOp->getOperand(0);
          Value *Op1 = BinOp->getOperand(1);

          if (FunctionType->getParamType(0) != Op0->getType())
            continue;

          if (FunctionType->getParamType(1) != Op1->getType())
            continue;

          if (FunctionType->getReturnType() != BinOp->getType())
            continue;

          IRBuilder<> Builder(BinOp);

          CallInst *Call = Builder.CreateCall(ReplacementFunction, {Op0, Op1});

          BinOp->replaceAllUsesWith(Call);
          Call->takeName(BinOp);
          BinOp->eraseFromParent();

          Changed = true;
        }
      }
    }

    if (Changed)
      return PreservedAnalyses::none();

    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ReplaceBinaryOpsPlugin", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "replace-binops") {
                    MPM.addPass(ReplaceBinaryOpsPass());
                    return true;
                  }
                  return false;
                });
          }};
}