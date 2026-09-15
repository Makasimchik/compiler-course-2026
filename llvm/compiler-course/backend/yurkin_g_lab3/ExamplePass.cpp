#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>

using namespace llvm;

namespace {
class ExamplePass : public MachineFunctionPass {
public:
  static char ID;
  ExamplePass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  unsigned selectAddSubOpcode(bool IsAdd, unsigned Width) {
    if (Width == 8) {
      return IsAdd ? X86::ADD8ri : X86::SUB8ri;
    } else if (Width == 64) {
      return IsAdd ? X86::ADD64ri32 : X86::SUB64ri32;
    } else {
      return IsAdd ? X86::ADD32ri : X86::SUB32ri;
    }
  }
};

char ExamplePass::ID = 0;

bool ExamplePass::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();

  auto opcodeName = [&](unsigned Opc) -> StringRef {
    return TII->getName(Opc);
  };

  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto MI = MBB.begin(); MI != MBB.end();) {
      MachineInstr &Instr = *MI;
      unsigned Opc = Instr.getOpcode();
      StringRef Name = opcodeName(Opc);
      if (Name.empty()) {
        ++MI;
        continue;
      }

      if (Name.find("INC") == StringRef::npos &&
          Name.find("DEC") == StringRef::npos) {
        ++MI;
        continue;
      }

      bool IsInc = (Name.find("INC") != StringRef::npos);

      unsigned Width = 32;
      if (Name.find("8") != StringRef::npos)
        Width = 8;
      else if (Name.find("64") != StringRef::npos)
        Width = 64;

      unsigned Reg = 0;
      for (const MachineOperand &MO : Instr.operands()) {
        if (MO.isReg() && MO.isDef()) {
          Reg = MO.getReg();
          break;
        }
      }
      if (!Reg) {
        for (const MachineOperand &MO : Instr.operands()) {
          if (MO.isReg()) {
            Reg = MO.getReg();
            break;
          }
        }
      }
      if (!Reg) {
        ++MI;
        continue;
      }

      int Count = 0;
      auto ScanIt = MI;
      while (ScanIt != MBB.end()) {
        MachineInstr &S = *ScanIt;
        StringRef SName = opcodeName(S.getOpcode());
        if (SName.empty())
          break;
        bool SIsInc = (SName.find("INC") != StringRef::npos);
        bool SIsDec = (SName.find("DEC") != StringRef::npos);
        if (!(SIsInc || SIsDec))
          break;

        unsigned SReg = 0;
        for (const MachineOperand &MO : S.operands()) {
          if (MO.isReg() && (MO.isDef() || MO.isUse())) {
            SReg = MO.getReg();
            break;
          }
        }
        if (SReg != Reg)
          break;

        if (SIsInc)
          Count += 1;
        else
          Count -= 1;

        ++ScanIt;
      }

      if (Count == 0) {
        auto EraseIt = MI;
        while (EraseIt != ScanIt) {
          EraseIt = MBB.erase(EraseIt);
        }
        MI = ScanIt;
        Changed = true;
        continue;
      }

      bool NetIsAdd = Count > 0;
      unsigned Imm = static_cast<unsigned>(std::abs(Count));
      unsigned NewOpc = selectAddSubOpcode(NetIsAdd, Width);
      DebugLoc DL = Instr.getDebugLoc();

      MachineInstrBuilder MIB = BuildMI(MBB, MI, DL, TII->get(NewOpc));
      MIB.addReg(Reg, RegState::Define);
      MIB.addImm(Imm);

      auto EraseIt = MI;
      while (EraseIt != ScanIt) {
        EraseIt = MBB.erase(EraseIt);
      }

      MI = ScanIt;
      Changed = true;
    }
  }

  return Changed;
}

static RegisterPass<ExamplePass>
    X("example-x86", "Replace INC/DEC with ADD/SUB", false, false);

} // namespace
