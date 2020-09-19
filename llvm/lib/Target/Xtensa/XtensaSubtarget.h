//===-- XtensaSubtarget.h - Define Subtarget for the Xtensa ----*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Xtensa specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XTENSA_XTENSASUBTARGET_H
#define LLVM_LIB_TARGET_XTENSA_XTENSASUBTARGET_H

#include "XtensaFrameLowering.h"
#include "XtensaISelLowering.h"
#include "XtensaInstrInfo.h"
#include "XtensaRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "XtensaGenSubtargetInfo.inc"

namespace llvm {
class StringRef;

class XtensaSubtarget : public XtensaGenSubtargetInfo {
private:
  Triple TargetTriple;
  XtensaInstrInfo InstrInfo;
  XtensaTargetLowering TLInfo;
  SelectionDAGTargetInfo TSInfo;
  XtensaFrameLowering FrameLowering;

  // Enabled Xtensa Density extension
  bool HasDensity;

  // Enabled Xtensa Single FP instructions
  bool HasSingleFloat;

  // Enabled Xtensa Windowed Register option
  bool HasWindowed;

  // Enabled Xtensa Boolean extension
  bool HasBoolean;

  // Enabled Xtensa Loop extension
  bool HasLoop;

  // Enable Xtensa Sign Extend option
  bool HasSEXT;

  // Enable Xtensa NSA option
  bool HasNSA;

  // Enable Xtensa Mul32 option
  bool HasMul32;

  // Enable Xtensa Mul32High option
  bool HasMul32High;

  // Enable Xtensa Div32 option
  bool HasDiv32;

  // Enabled Xtensa MAC16 instructions
  bool HasMAC16;

  // Enable Xtensa Xtensa Double Precision FP acceleration
  bool HasDFPAccel;

  // Enable Xtensa S32C1I option
  bool HasS32C1I;

  // Enable Xtensa THREADPTR option
  bool HasTHREADPTR;

  // Enable Xtensa Extended L32R option
  bool HasExtendedL32R;

  // Enable Xtensa ATOMCTL option
  bool HasATOMCTL;

  // Enable Xtensa ATOMCTL option
  bool HasMEMCTL;

  // Enable Xtensa Debug option
  bool HasDebug;

  // Enable Xtensa Exceptions option
  bool HasException;

  // Enable Xtensa High Priority Interrupt option
  bool HasHighPriInterrupts;

  // Enable Xtensa Coprocessor option
  bool HasCoprocessor;

  // Enable Xtensa Interrupt option
  bool HasInterrupt;

  // Enable Xtensa Relocatable Vector option
  bool HasRelocatableVector;

  // Enable Xtensa Timer Interrupt option
  bool HasTimerInt;

  // Enable Xtensa Processor ID option
  bool HasPRID;

  // Enable Xtensa Region Protection option
  bool HasRegionProtection;

  // Enable Xtensa Miscellaneous Special Reigsiters option
  bool HasMiscSR;

  XtensaSubtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS);

public:
  XtensaSubtarget(const Triple &TT, const std::string &CPU,
                  const std::string &FS, const TargetMachine &TM);

  const TargetFrameLowering *getFrameLowering() const { return &FrameLowering; }
  const XtensaInstrInfo *getInstrInfo() const { return &InstrInfo; }
  const XtensaRegisterInfo *getRegisterInfo() const {
    return &InstrInfo.getRegisterInfo();
  }

  const XtensaTargetLowering *getTargetLowering() const { return &TLInfo; }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const { return &TSInfo; }

  bool isWinABI() const { return hasWindowed(); }

  bool hasDensity() const { return HasDensity; }

  bool hasSingleFloat() const { return HasSingleFloat; }

  bool hasWindowed() const { return HasWindowed; }

  bool hasBoolean() const { return HasBoolean; }

  bool hasLoop() const { return HasLoop; }

  bool hasSEXT() const { return HasSEXT; }

  bool hasNSA() const { return HasNSA; }

  bool hasMul32() const { return HasMul32; }

  bool hasMul32High() const { return HasMul32High; }

  bool hasDiv32() const { return HasDiv32; }

  bool hasMAC16() const { return HasMAC16; }

  bool hasDFPAccel() const { return HasDFPAccel; }

  bool hasS32C1I() const { return HasS32C1I; }

  bool hasTHREADPTR() const { return HasTHREADPTR; }

  bool hasExtendedL32R() const { return HasExtendedL32R; }

  bool hasATOMCTL() const { return HasATOMCTL; }

  bool hasMEMCTL() const { return HasMEMCTL; }

  bool hasDebug() const { return HasDebug; }

  bool hasException() const { return HasException; }

  bool hasHighPriInterrupts() const { return HasHighPriInterrupts; }

  bool hasCoprocessor() const { return HasCoprocessor; }

  bool hasInterrupt() const { return HasInterrupt; }

  bool hasRelocatableVector() const { return HasRelocatableVector; }

  bool hasTimerInt() const { return HasTimerInt; }

  bool hasPRID() const { return HasPRID; }

  bool hasRegionProtection() const { return HasRegionProtection; }

  bool hasMiscSR() const { return HasMiscSR; }

  // Automatically generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);
};
} // end namespace llvm

#endif /* LLVM_LIB_TARGET_XTENSA_XTENSASUBTARGET_H */
