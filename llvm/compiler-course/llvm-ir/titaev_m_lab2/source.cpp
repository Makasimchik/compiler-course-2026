#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct ReplaceAddPass : public PassInfoMixin<ReplaceAddPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    Function *AddFunction = M.getFunction("add");

    if (!AddFunction)
      return PreservedAnalyses::all();

    if (AddFunction->arg_size() != 2)
      return PreservedAnalyses::all();

    FunctionType *AddType = AddFunction->getFunctionType();
    bool Changed = false;

    for (Function &F : M) {
      // Саму функцию add не меняем
      if (&F == AddFunction)
        continue;

      for (BasicBlock &BB : F) {
        for (auto It = BB.begin(), End = BB.end(); It != End;) {
          Instruction &I = *It++;

          auto *BinOp = dyn_cast<BinaryOperator>(&I);
          if (!BinOp)
            continue;

          if (BinOp->getOpcode() != Instruction::Add)
            continue;

          Value *Op0 = BinOp->getOperand(0);
          Value *Op1 = BinOp->getOperand(1);

          // Типы операндов должны совпадать
          // с типами аргументов функции add
          if (AddType->getParamType(0) != Op0->getType())
            continue;

          if (AddType->getParamType(1) != Op1->getType())
            continue;

          // Тип результата тоже должен совпадать
          if (AddType->getReturnType() != BinOp->getType())
            continue;

          IRBuilder<> Builder(BinOp);

          CallInst *Call = Builder.CreateCall(AddFunction, {Op0, Op1});

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
  return {LLVM_PLUGIN_API_VERSION, "ReplaceAddPlugin", "1.0",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "replace-add") {
                    MPM.addPass(ReplaceAddPass());
                    return true;
                  }
                  return false;
                });
          }};
}