#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

namespace {

struct IncDecInfo {
  int Delta;
  unsigned Size;
};

class X86IncDecPass : public MachineFunctionPass {
public:
  static char ID;
  X86IncDecPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    bool Changed = false;

    const DenseMap<unsigned, IncDecInfo> IncDecMap = {
        {X86::INC8r, {1, 8}},    {X86::INC16r, {1, 16}},
        {X86::INC32r, {1, 32}},  {X86::INC64r, {1, 64}},
        {X86::DEC8r, {-1, 8}},   {X86::DEC16r, {-1, 16}},
        {X86::DEC32r, {-1, 32}}, {X86::DEC64r, {-1, 64}},
    };

    const DenseMap<unsigned, unsigned> AddMap = {
        {8, X86::ADD8ri},
        {16, X86::ADD16ri8},
        {32, X86::ADD32ri8},
        {64, X86::ADD64ri8},
    };

    const DenseMap<unsigned, unsigned> SubMap = {
        {8, X86::SUB8ri},
        {16, X86::SUB16ri8},
        {32, X86::SUB32ri8},
        {64, X86::SUB64ri8},
    };

    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {

        auto Start = It;
        MachineInstr &MI = *It;

        auto InfoIt = IncDecMap.find(MI.getOpcode());
        if (InfoIt == IncDecMap.end()) {
          ++It;
          continue;
        }

        unsigned Reg = MI.getOperand(0).getReg();
        unsigned Size = InfoIt->second.Size;
        int TotalDelta = 0;
        DebugLoc DL = MI.getDebugLoc();

        auto Scan = It;
        while (Scan != MBB.end()) {
          MachineInstr &Cur = *Scan;

          auto CurIt = IncDecMap.find(Cur.getOpcode());
          if (CurIt == IncDecMap.end())
            break;

          if (CurIt->second.Size != Size)
            break;

          if (Cur.getOperand(0).getReg() != Reg)
            break;

          TotalDelta += CurIt->second.Delta;
          ++Scan;
        }

        if (TotalDelta == 0) {
          It = MBB.erase(Start, Scan);
          Changed = true;
          continue;
        }

        const auto &OpcodeMap = (TotalDelta > 0) ? AddMap : SubMap;
        unsigned NewOpc = OpcodeMap.lookup(Size);

        BuildMI(MBB, Start, DL, TII->get(NewOpc), Reg)
            .addReg(Reg)
            .addImm(std::abs(TotalDelta));

        It = MBB.erase(Start, Scan);
        Changed = true;
      }
    }

    return Changed;
  }
};

char X86IncDecPass::ID = 0;

} // namespace

static RegisterPass<X86IncDecPass>
    X("x86-incdec-opt", "Combine INC/DEC into ADD/SUB", false, false);
