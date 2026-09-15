// main.cpp
// Decompose remainder instructions: frem/srem/urem -> div; (trunc for FP) ;
// mul; sub Plugin pipeline name: yurkin_g_lab2
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct YurkinGDecomposeRem : PassInfoMixin<YurkinGDecomposeRem> {
  // Build replacement for floating-point remainder:
  //   r = a - (trunc(a / b)) * b
  // (uses llvm.trunc.* intrinsic to match expected IR pattern)
  Value *buildFPRemReplacement(Instruction *Orig, IRBuilder<> &B) {
    Value *A = Orig->getOperand(0);
    Value *Bv = Orig->getOperand(1);

    // Preserve fast-math flags from original FP operator, if present.
    FastMathFlags FMF;
    if (auto *FPO = dyn_cast<FPMathOperator>(Orig))
      FMF = FPO->getFastMathFlags();
    B.setFastMathFlags(FMF);

    Value *Div = B.CreateFDiv(A, Bv, "frem.div");
    // Use the trunc intrinsic (scalar or vector) to reproduce the pattern
    Value *Trunc =
        B.CreateUnaryIntrinsic(Intrinsic::trunc, Div, nullptr, "frem.trunc");
    Value *Mul = B.CreateFMul(Trunc, Bv, "frem.mul");
    return B.CreateFSub(A, Mul, "frem.res");
  }

  // Build replacement for integer remainder (signed and unsigned)
  Value *buildIntRemReplacement(Instruction *Orig, IRBuilder<> &B) {
    Value *A = Orig->getOperand(0);
    Value *Bv = Orig->getOperand(1);

    if (Orig->getOpcode() == Instruction::SRem) {
      Value *Div = B.CreateSDiv(A, Bv, "srem.div");
      Value *Mul = B.CreateMul(Div, Bv, "srem.mul");
      return B.CreateSub(A, Mul, "srem.res");
    } else {
      Value *Div = B.CreateUDiv(A, Bv, "urem.div");
      Value *Mul = B.CreateMul(Div, Bv, "urem.mul");
      return B.CreateSub(A, Mul, "urem.res");
    }
  }

  Value *makeReplacement(Instruction *I) {
    IRBuilder<> B(I);
    unsigned Op = I->getOpcode();
    if (Op == Instruction::FRem)
      return buildFPRemReplacement(I, B);
    if (Op == Instruction::SRem || Op == Instruction::URem)
      return buildIntRemReplacement(I, B);
    return nullptr;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    SmallVector<Instruction *, 16> WorkList;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB) {
        unsigned Op = I.getOpcode();
        if (Op == Instruction::FRem || Op == Instruction::SRem ||
            Op == Instruction::URem)
          WorkList.push_back(&I);
      }

    if (WorkList.empty())
      return PreservedAnalyses::all();

    bool Changed = false;
    for (Instruction *I : WorkList) {
      if (!I->getParent() || I->getFunction() != &F)
        continue;

      Value *NewV = makeReplacement(I);
      if (!NewV)
        continue;

      // If the replacement is an instruction, copy debug location and metadata.
      if (Instruction *NI = dyn_cast<Instruction>(NewV)) {
        NI->setDebugLoc(I->getDebugLoc());
        SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
        I->getAllMetadata(MDs);
        for (auto &P : MDs)
          NI->setMetadata(P.first, P.second);
      }

      I->replaceAllUsesWith(NewV);
      I->eraseFromParent();
      Changed = true;
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "YurkinGDecomposeRemPlugin", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  // Register under the name expected by your tests/CI.
                  if (Name == "yurkin_g_lab2") {
                    FPM.addPass(YurkinGDecomposeRem());
                    return true;
                  }
                  return false;
                });
          }};
}
