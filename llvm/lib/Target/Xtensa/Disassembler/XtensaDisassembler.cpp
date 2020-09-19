//===-- XtensaDisassembler.cpp - Disassembler for Xtensa ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the XtensaDisassembler class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/XtensaMCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCFixedLenDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "Xtensa-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {

class XtensaDisassembler : public MCDisassembler {
  bool IsLittleEndian;

public:
  XtensaDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx, bool isLE)
      : MCDisassembler(STI, Ctx), IsLittleEndian(isLE) {}

  bool hasDensity() const {
    return STI.getFeatureBits()[Xtensa::FeatureDensity];
  }

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createXtensaDisassembler(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                MCContext &Ctx) {
  return new XtensaDisassembler(STI, Ctx, true);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeXtensaDisassembler() {
  TargetRegistry::RegisterMCDisassembler(TheXtensaTarget,
                                         createXtensaDisassembler);
}

static const unsigned ARDecoderTable[] = {
    Xtensa::A0,  Xtensa::SP,  Xtensa::A2,  Xtensa::A3, Xtensa::A4,  Xtensa::A5,
    Xtensa::A6,  Xtensa::A7,  Xtensa::A8,  Xtensa::A9, Xtensa::A10, Xtensa::A11,
    Xtensa::A12, Xtensa::A13, Xtensa::A14, Xtensa::A15};

static DecodeStatus DecodeARRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (RegNo >= array_lengthof(ARDecoderTable))
    return MCDisassembler::Fail;

  unsigned Reg = ARDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static const unsigned FPRDecoderTable[] = {
    Xtensa::F0,  Xtensa::F1,  Xtensa::F2,  Xtensa::F3, Xtensa::F4,  Xtensa::F5,
    Xtensa::F6,  Xtensa::F7,  Xtensa::F8,  Xtensa::F9, Xtensa::F10, Xtensa::F11,
    Xtensa::F12, Xtensa::F13, Xtensa::F14, Xtensa::F15};

static DecodeStatus DecodeFPRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                           uint64_t Address,
                                           const void *Decoder) {
  if (RegNo >= array_lengthof(FPRDecoderTable))
    return MCDisassembler::Fail;

  unsigned Reg = FPRDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static const unsigned BRDecoderTable[] = {
    Xtensa::B0,  Xtensa::B1,  Xtensa::B2,  Xtensa::B3, Xtensa::B4,  Xtensa::B5,
    Xtensa::B6,  Xtensa::B7,  Xtensa::B8,  Xtensa::B9, Xtensa::B10, Xtensa::B11,
    Xtensa::B12, Xtensa::B13, Xtensa::B14, Xtensa::B15};

static DecodeStatus DecodeBRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (RegNo >= array_lengthof(BRDecoderTable))
    return MCDisassembler::Fail;

  unsigned Reg = BRDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static const unsigned MRDecoderTable[] = {Xtensa::M0, Xtensa::M1, Xtensa::M2,
                                          Xtensa::M3};

static DecodeStatus DecodeMRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  if (RegNo >= array_lengthof(MRDecoderTable))
    return MCDisassembler::Fail;

  unsigned Reg = MRDecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static const unsigned MR01DecoderTable[] = {Xtensa::M0, Xtensa::M1};

static DecodeStatus DecodeMR01RegisterClass(MCInst &Inst, uint64_t RegNo,
                                            uint64_t Address,
                                            const void *Decoder) {
  if (RegNo > 2)
    return MCDisassembler::Fail;

  unsigned Reg = MR01DecoderTable[RegNo];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static const unsigned MR23DecoderTable[] = {Xtensa::M2, Xtensa::M3};

static DecodeStatus DecodeMR23RegisterClass(MCInst &Inst, uint64_t RegNo,
                                            uint64_t Address,
                                            const void *Decoder) {
  if ((RegNo < 2) || (RegNo > 3))
    return MCDisassembler::Fail;

  unsigned Reg = MR23DecoderTable[RegNo - 2];
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

// Verify SR and UR
bool CheckRegister(unsigned RegNo, MCSubtargetInfo STI) {
  StringRef CPU = STI.getCPU();
  unsigned NumIntLevels = 0;
  unsigned NumTimers = 0;
  unsigned NumMiscSR = 0;
  bool IsESP32 = false;
  bool IsESP32_S2 = false;
  bool Res = true;

  // Assume that CPU is esp32 by default
  if ((CPU == "esp32") || (CPU == "")) {
    NumIntLevels = 6;
    NumTimers = 3;
    NumMiscSR = 4;
    IsESP32 = true;
  } else if (CPU == "esp32-s2") {
    NumIntLevels = 6;
    NumTimers = 3;
    NumMiscSR = 4;
    IsESP32_S2 = true;
  } else if (CPU == "esp8266") {
    NumIntLevels = 2;
    NumTimers = 1;
  }

  switch (RegNo) {
  case Xtensa::LBEG:
  case Xtensa::LEND:
  case Xtensa::LCOUNT:
    Res = STI.getFeatureBits()[Xtensa::FeatureLoop];
    break;
  case Xtensa::BREG:
    Res = STI.getFeatureBits()[Xtensa::FeatureBoolean];
    break;
  case Xtensa::LITBASE:
    Res = STI.getFeatureBits()[Xtensa::FeatureExtendedL32R];
    break;
  case Xtensa::SCOMPARE1:
    Res = STI.getFeatureBits()[Xtensa::FeatureS32C1I];
    break;
  case Xtensa::ACCLO:
  case Xtensa::ACCHI:
  case Xtensa::M0:
  case Xtensa::M1:
  case Xtensa::M2:
  case Xtensa::M3:
    Res = STI.getFeatureBits()[Xtensa::FeatureMAC16];
    break;
  case Xtensa::WINDOWBASE:
  case Xtensa::WINDOWSTART:
    Res = STI.getFeatureBits()[Xtensa::FeatureWindowed];
    break;
  case Xtensa::IBREAKENABLE:
  case Xtensa::IBREAKA0:
  case Xtensa::IBREAKA1:
  case Xtensa::DBREAKA0:
  case Xtensa::DBREAKA1:
  case Xtensa::DBREAKC0:
  case Xtensa::DBREAKC1:
  case Xtensa::DEBUGCAUSE:
  case Xtensa::ICOUNT:
  case Xtensa::ICOUNTLEVEL:
    Res = STI.getFeatureBits()[Xtensa::FeatureDebug];
    break;
  case Xtensa::ATOMCTL:
    Res = STI.getFeatureBits()[Xtensa::FeatureATOMCTL];
    break;
  case Xtensa::MEMCTL:
    Res = STI.getFeatureBits()[Xtensa::FeatureMEMCTL];
    break;
  case Xtensa::EPC1:
    Res = STI.getFeatureBits()[Xtensa::FeatureException];
    break;
  case Xtensa::EPC2:
  case Xtensa::EPC3:
  case Xtensa::EPC4:
  case Xtensa::EPC5:
  case Xtensa::EPC6:
  case Xtensa::EPC7:
    Res = STI.getFeatureBits()[Xtensa::FeatureHighPriInterrupts];
    Res = Res & (NumIntLevels >= (RegNo - Xtensa::EPC1));
    break;
  case Xtensa::EPS2:
  case Xtensa::EPS3:
  case Xtensa::EPS4:
  case Xtensa::EPS5:
  case Xtensa::EPS6:
  case Xtensa::EPS7:
    Res = STI.getFeatureBits()[Xtensa::FeatureHighPriInterrupts];
    Res = Res & (NumIntLevels > (RegNo - Xtensa::EPS2));
    break;
  case Xtensa::EXCSAVE1:
    Res = STI.getFeatureBits()[Xtensa::FeatureException];
    break;
  case Xtensa::EXCSAVE2:
  case Xtensa::EXCSAVE3:
  case Xtensa::EXCSAVE4:
  case Xtensa::EXCSAVE5:
  case Xtensa::EXCSAVE6:
  case Xtensa::EXCSAVE7:
    Res = STI.getFeatureBits()[Xtensa::FeatureHighPriInterrupts];
    Res = Res & (NumIntLevels >= (RegNo - Xtensa::EXCSAVE1));
    break;
  case Xtensa::DEPC:
  case Xtensa::EXCCAUSE:
  case Xtensa::EXCVADDR:
    Res = STI.getFeatureBits()[Xtensa::FeatureException];
    break;
  case Xtensa::CPENABLE:
    Res = STI.getFeatureBits()[Xtensa::FeatureCoprocessor];
    break;
  case Xtensa::VECBASE:
    Res = STI.getFeatureBits()[Xtensa::FeatureRelocatableVector];
    break;
  case Xtensa::CCOUNT:
    Res = STI.getFeatureBits()[Xtensa::FeatureTimerInt];
    Res &= (NumTimers > 0);
    break;
  case Xtensa::CCOMPARE0:
  case Xtensa::CCOMPARE1:
  case Xtensa::CCOMPARE2:
    Res = STI.getFeatureBits()[Xtensa::FeatureTimerInt];
    Res &= (NumTimers > (RegNo - Xtensa::CCOMPARE0));
    break;
  case Xtensa::PRID:
    Res = STI.getFeatureBits()[Xtensa::FeaturePRID];
    break;
  case Xtensa::INTSET:
  case Xtensa::INTCLEAR:
  case Xtensa::INTENABLE:
    Res = STI.getFeatureBits()[Xtensa::FeatureInterrupt];
    break;
  case Xtensa::MISC0:
  case Xtensa::MISC1:
  case Xtensa::MISC2:
  case Xtensa::MISC3:
    Res = STI.getFeatureBits()[Xtensa::FeatureMiscSR];
    Res &= (NumMiscSR > (RegNo - Xtensa::MISC0));
    break;
  case Xtensa::THREADPTR:
    Res = STI.getFeatureBits()[Xtensa::FeatureTHREADPTR];
    break;
  case Xtensa::GPIO_OUT:
    Res = IsESP32_S2;
    break;
  case Xtensa::EXPSTATE:
    Res = IsESP32;
    break;
  case Xtensa::FCR:
  case Xtensa::FSR:
    Res = STI.getFeatureBits()[Xtensa::FeatureSingleFloat];
    break;
  case Xtensa::F64R_LO:
  case Xtensa::F64R_HI:
  case Xtensa::F64S:
    Res = STI.getFeatureBits()[Xtensa::FeatureDFPAccel];
    break;
  }

  return Res;
}

static const unsigned SRDecoderTable[] = {
    Xtensa::LBEG,        0,   Xtensa::LEND,         1,
    Xtensa::LCOUNT,      2,   Xtensa::SAR,          3,
    Xtensa::BREG,        4,   Xtensa::LITBASE,      5,
    Xtensa::SCOMPARE1,   12,  Xtensa::ACCLO,        16,
    Xtensa::ACCHI,       17,  Xtensa::M0,           32,
    Xtensa::M1,          33,  Xtensa::M2,           34,
    Xtensa::M3,          35,  Xtensa::WINDOWBASE,   72,
    Xtensa::WINDOWSTART, 73,  Xtensa::IBREAKENABLE, 96,
    Xtensa::MEMCTL,      97,  Xtensa::ATOMCTL,      99,
    Xtensa::DDR,         104, Xtensa::IBREAKA0,     128,
    Xtensa::IBREAKA1,    129, Xtensa::DBREAKA0,     144,
    Xtensa::DBREAKA1,    145, Xtensa::DBREAKC0,     160,
    Xtensa::DBREAKC1,    161, Xtensa::CONFIGID0,    176,
    Xtensa::EPC1,        177, Xtensa::EPC2,         178,
    Xtensa::EPC3,        179, Xtensa::EPC4,         180,
    Xtensa::EPC5,        181, Xtensa::EPC6,         182,
    Xtensa::EPC7,        183, Xtensa::DEPC,         192,
    Xtensa::EPS2,        194, Xtensa::EPS3,         195,
    Xtensa::EPS4,        196, Xtensa::EPS5,         197,
    Xtensa::EPS6,        198, Xtensa::EPS7,         199,
    Xtensa::CONFIGID1,   208, Xtensa::EXCSAVE1,     209,
    Xtensa::EXCSAVE2,    210, Xtensa::EXCSAVE3,     211,
    Xtensa::EXCSAVE4,    212, Xtensa::EXCSAVE5,     213,
    Xtensa::EXCSAVE6,    214, Xtensa::EXCSAVE7,     215,
    Xtensa::CPENABLE,    224, Xtensa::INTSET,       226,
    Xtensa::INTCLEAR,    227, Xtensa::INTENABLE,    228,
    Xtensa::PS,          230, Xtensa::VECBASE,      231,
    Xtensa::EXCCAUSE,    232, Xtensa::DEBUGCAUSE,   233,
    Xtensa::CCOUNT,      234, Xtensa::PRID,         235,
    Xtensa::ICOUNT,      236, Xtensa::ICOUNTLEVEL,  237,
    Xtensa::EXCVADDR,    238, Xtensa::CCOMPARE0,    240,
    Xtensa::CCOMPARE1,   241, Xtensa::CCOMPARE2,    242,
    Xtensa::MISC0,       244, Xtensa::MISC1,        245,
    Xtensa::MISC2,       246, Xtensa::MISC3,        247};

static DecodeStatus DecodeSRRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  const llvm::MCSubtargetInfo STI =
      ((const MCDisassembler *)Decoder)->getSubtargetInfo();

  if (RegNo > 255)
    return MCDisassembler::Fail;

  for (unsigned i = 0; i < array_lengthof(SRDecoderTable); i += 2) {
    if (SRDecoderTable[i + 1] == RegNo) {
      unsigned Reg = SRDecoderTable[i];

      if (!CheckRegister(Reg, STI))
        return MCDisassembler::Fail;

      Inst.addOperand(MCOperand::createReg(Reg));
      return MCDisassembler::Success;
    }
  }

  return MCDisassembler::Fail;
}

static const unsigned URDecoderTable[] = {
    Xtensa::GPIO_OUT, 0,   Xtensa::EXPSTATE, 230, Xtensa::THREADPTR, 231,
    Xtensa::FCR,      232, Xtensa::FSR,      233, Xtensa::F64R_LO,   234,
    Xtensa::F64R_HI,  235, Xtensa::F64S,     236};

static DecodeStatus DecodeURRegisterClass(MCInst &Inst, uint64_t RegNo,
                                          uint64_t Address,
                                          const void *Decoder) {
  const llvm::MCSubtargetInfo STI =
      ((const MCDisassembler *)Decoder)->getSubtargetInfo();

  if (RegNo > 255)
    return MCDisassembler::Fail;

  for (unsigned i = 0; i < array_lengthof(URDecoderTable); i += 2) {
    if (URDecoderTable[i + 1] == RegNo) {
      unsigned Reg = URDecoderTable[i];

      if (!CheckRegister(Reg, STI))
        return MCDisassembler::Fail;

      Inst.addOperand(MCOperand::createReg(Reg));
      return MCDisassembler::Success;
    }
  }

  return MCDisassembler::Fail;
}

static bool tryAddingSymbolicOperand(int64_t Value, bool isBranch,
                                     uint64_t Address, uint64_t Offset,
                                     uint64_t Width, MCInst &MI,
                                     const void *Decoder) {
  const MCDisassembler *Dis = static_cast<const MCDisassembler *>(Decoder);
  return Dis->tryAddingSymbolicOperand(MI, Value, Address, isBranch, Offset,
                                       Width);
}

static DecodeStatus decodeCallOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder) {
  assert(isUInt<18>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(SignExtend64<20>(Imm << 2)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeJumpOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder) {
  assert(isUInt<18>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(SignExtend64<18>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeBranchOperand(MCInst &Inst, uint64_t Imm,
                                        int64_t Address, const void *Decoder) {
  switch (Inst.getOpcode()) {
  case Xtensa::BEQZ:
  case Xtensa::BGEZ:
  case Xtensa::BLTZ:
  case Xtensa::BNEZ:
    assert(isUInt<12>(Imm) && "Invalid immediate");
    if (!tryAddingSymbolicOperand(SignExtend64<12>(Imm) + 4 + Address, true,
                                  Address, 0, 3, Inst, Decoder))
      Inst.addOperand(MCOperand::createImm(SignExtend64<12>(Imm)));
    break;
  default:
    assert(isUInt<8>(Imm) && "Invalid immediate");
    if (!tryAddingSymbolicOperand(SignExtend64<8>(Imm) + 4 + Address, true,
                                  Address, 0, 3, Inst, Decoder))
      Inst.addOperand(MCOperand::createImm(SignExtend64<8>(Imm)));
  }
  return MCDisassembler::Success;
}

static DecodeStatus decodeL32ROperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder) {

  assert(isUInt<16>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(
      SignExtend64<17>((Imm << 2) + 0x40000 + (Address & 0x3))));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm8Operand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder) {
  assert(isUInt<8>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(SignExtend64<8>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm8_sh8Operand(MCInst &Inst, uint64_t Imm,
                                          int64_t Address,
                                          const void *Decoder) {
  assert(isUInt<8>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(SignExtend64<16>(Imm << 8)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm12Operand(MCInst &Inst, uint64_t Imm,
                                       int64_t Address, const void *Decoder) {
  assert(isUInt<12>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(SignExtend64<12>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeUimm4Operand(MCInst &Inst, uint64_t Imm,
                                       int64_t Address, const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus decodeUimm5Operand(MCInst &Inst, uint64_t Imm,
                                       int64_t Address, const void *Decoder) {
  assert(isUInt<5>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm1_16Operand(MCInst &Inst, uint64_t Imm,
                                         int64_t Address, const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm + 1));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm1n_15Operand(MCInst &Inst, uint64_t Imm,
                                          int64_t Address,
                                          const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");
  if (!Imm)
    Inst.addOperand(MCOperand::createImm(-1));
  else
    Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm32n_95Operand(MCInst &Inst, uint64_t Imm,
                                           int64_t Address,
                                           const void *Decoder) {
  assert(isUInt<7>(Imm) && "Invalid immediate");
  if ((Imm & 0x60) == 0x60)
    Inst.addOperand(MCOperand::createImm((~0x1f) | Imm));
  else
    Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm8n_7Operand(MCInst &Inst, uint64_t Imm,
                                         int64_t Address, const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");
  if (Imm > 7)
    Inst.addOperand(MCOperand::createImm(Imm - 16));
  else
    Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

static DecodeStatus decodeImm64n_4nOperand(MCInst &Inst, uint64_t Imm,
                                           int64_t Address,
                                           const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm((~0x3f) | (Imm << 2)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeEntry_Imm12OpValue(MCInst &Inst, uint64_t Imm,
                                             int64_t Address,
                                             const void *Decoder) {
  assert(isUInt<12>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm << 3));
  return MCDisassembler::Success;
}

static DecodeStatus decodeShimm1_31Operand(MCInst &Inst, uint64_t Imm,
                                           int64_t Address,
                                           const void *Decoder) {
  assert(isUInt<5>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(32 - Imm));
  return MCDisassembler::Success;
}

static DecodeStatus decodeSeimm7_22Operand(MCInst &Inst, uint64_t Imm,
                                           int64_t Address,
                                           const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm + 7));
  return MCDisassembler::Success;
}

static int64_t TableB4const[16] = {-1, 1,  2,  3,  4,  5,  6,   7,
                                   8,  10, 12, 16, 32, 64, 128, 256};
static DecodeStatus decodeB4constOperand(MCInst &Inst, uint64_t Imm,
                                         int64_t Address, const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");

  Inst.addOperand(MCOperand::createImm(TableB4const[Imm]));
  return MCDisassembler::Success;
}

static int64_t TableB4constu[16] = {32768, 65536, 2,  3,  4,  5,  6,   7,
                                    8,     10,    12, 16, 32, 64, 128, 256};
static DecodeStatus decodeB4constuOperand(MCInst &Inst, uint64_t Imm,
                                          int64_t Address,
                                          const void *Decoder) {
  assert(isUInt<4>(Imm) && "Invalid immediate");

  Inst.addOperand(MCOperand::createImm(TableB4constu[Imm]));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMem8Operand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address, const void *Decoder) {
  assert(isUInt<12>(Imm) && "Invalid immediate");
  DecodeARRegisterClass(Inst, Imm & 0xf, Address, Decoder);
  Inst.addOperand(MCOperand::createImm((Imm >> 4) & 0xff));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMem16Operand(MCInst &Inst, uint64_t Imm,
                                       int64_t Address, const void *Decoder) {
  assert(isUInt<12>(Imm) && "Invalid immediate");
  DecodeARRegisterClass(Inst, Imm & 0xf, Address, Decoder);
  Inst.addOperand(MCOperand::createImm((Imm >> 3) & 0x1fe));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMem32Operand(MCInst &Inst, uint64_t Imm,
                                       int64_t Address, const void *Decoder) {
  assert(isUInt<12>(Imm) && "Invalid immediate");
  DecodeARRegisterClass(Inst, Imm & 0xf, Address, Decoder);
  Inst.addOperand(MCOperand::createImm((Imm >> 2) & 0x3fc));
  return MCDisassembler::Success;
}

static DecodeStatus decodeMem32nOperand(MCInst &Inst, uint64_t Imm,
                                        int64_t Address, const void *Decoder) {
  assert(isUInt<8>(Imm) && "Invalid immediate");
  DecodeARRegisterClass(Inst, Imm & 0xf, Address, Decoder);
  Inst.addOperand(MCOperand::createImm((Imm >> 2) & 0x3c));
  return MCDisassembler::Success;
}

/// Read two bytes from the ArrayRef and return 16 bit data sorted
/// according to the given endianness.
static DecodeStatus readInstruction16(ArrayRef<uint8_t> Bytes, uint64_t Address,
                                      uint64_t &Size, uint32_t &Insn,
                                      bool IsLittleEndian) {
  // We want to read exactly 2 Bytes of data.
  if (Bytes.size() < 2) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  if (!IsLittleEndian) {
    llvm_unreachable("Big-endian mode currently is not supported!");
  } else {
    Insn = (Bytes[1] << 8) | Bytes[0];
  }

  return MCDisassembler::Success;
}

/// Read four bytes from the ArrayRef and return 24 bit data sorted
/// according to the given endianness.
static DecodeStatus readInstruction24(ArrayRef<uint8_t> Bytes, uint64_t Address,
                                      uint64_t &Size, uint32_t &Insn,
                                      bool IsLittleEndian) {
  // We want to read exactly 3 Bytes of data.
  if (Bytes.size() < 3) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  if (!IsLittleEndian) {
    llvm_unreachable("Big-endian mode currently is not supported!");
  } else {
    Insn = (Bytes[2] << 16) | (Bytes[1] << 8) | (Bytes[0] << 0);
  }

  return MCDisassembler::Success;
}

#include "XtensaGenDisassemblerTables.inc"

DecodeStatus XtensaDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CS) const {
  uint32_t Insn;
  DecodeStatus Result;

  if (hasDensity()) {
    Result = readInstruction16(Bytes, Address, Size, Insn, IsLittleEndian);
    if (Result == MCDisassembler::Fail)
      return MCDisassembler::Fail;
    LLVM_DEBUG(dbgs() << "Trying Xtensa 16-bit instruction table :\n");
    Result = decodeInstruction(DecoderTable16, MI, Insn, Address, this, STI);
    if (Result != MCDisassembler::Fail) {
      Size = 2;
      return Result;
    }
  }

  Result = readInstruction24(Bytes, Address, Size, Insn, IsLittleEndian);
  if (Result == MCDisassembler::Fail)
    return MCDisassembler::Fail;
  LLVM_DEBUG(dbgs() << "Trying Xtensa 24-bit instruction table :\n");
  Result = decodeInstruction(DecoderTable24, MI, Insn, Address, this, STI);
  Size = 3;
  return Result;
}
