//===- Xtensa.td - Describe the Xtensa Target Machine -----------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Subtarget Features.
//===----------------------------------------------------------------------===//
def FeatureDensity : SubtargetFeature<"density", "HasDensity", "true",
                    "Enable Density instructions">;
def HasDensity : Predicate<"Subtarget->hasDensity()">,
                     AssemblerPredicate<"FeatureDensity">;

def FeatureSingleFloat      : SubtargetFeature<"fp", "HasSingleFloat", "true",
                                               "Enable Xtensa Single FP instructions">;
def HasSingleFloat          : Predicate<"Subtarget->hasSingleFloat()">,
                                        AssemblerPredicate<"FeatureSingleFloat">;

def FeatureWindowed         : SubtargetFeature<"windowed", "HasWindowed", "true",
                                               "Enable Xtensa Windowed Register option">;
def HasWindowed             : Predicate<"Subtarget->hasWindowed()">,
                                         AssemblerPredicate<"FeatureWindowed">;

def FeatureBoolean          : SubtargetFeature<"bool", "HasBoolean", "true",
                                               "Enable Xtensa Boolean extension">;
def HasBoolean              : Predicate<"Subtarget->hasBoolean()">,
                                         AssemblerPredicate<"FeatureBoolean">;

def FeatureLoop             : SubtargetFeature<"loop", "HasLoop", "true",
                                               "Enable Xtensa Loop extension">;
def HasLoop                 : Predicate<"Subtarget->hasLoop()">,
                                         AssemblerPredicate<"FeatureLoop">;

def FeatureSEXT             : SubtargetFeature<"sext", "HasSEXT", "true",
                                              "Enable Xtensa Sign Extend option">;
def HasSEXT                 : Predicate<"Subtarget->hasSEXT()">,
                                         AssemblerPredicate<"FeatureSEXT">;

def FeatureNSA              : SubtargetFeature<"nsa", "HasNSA", "true",
                                               "Enable Xtensa NSA option">;
def HasNSA                  : Predicate<"Subtarget->hasNSA()">,
                                         AssemblerPredicate<"FeatureNSA">;

def FeatureMul32            : SubtargetFeature<"mul32", "HasMul32", "true",
                                               "Enable Xtensa Mul32 option">;
def HasMul32                : Predicate<"Subtarget->hasMul32()">,
                                         AssemblerPredicate<"FeatureMul32">;

def FeatureMul32High        : SubtargetFeature<"mul32high", "HasMul32High", "true",
                                               "Enable Xtensa Mul32High option">;
def HasMul32High            : Predicate<"Subtarget->hasMul32High()">,
                                         AssemblerPredicate<"FeatureMul32High">;

def FeatureDiv32            : SubtargetFeature<"div32", "HasDiv32", "true",
                                               "Enable Xtensa Div32 option">;
def HasDiv32                : Predicate<"Subtarget->hasDiv32()">,
                                         AssemblerPredicate<"FeatureDiv32">;

def FeatureMAC16            : SubtargetFeature<"mac16", "HasMAC16", "true",
                                               "Enable Xtensa MAC16 instructions">;
def HasMAC16                : Predicate<"Subtarget->hasMAC16()">,
                                         AssemblerPredicate<"FeatureMAC16">;

def FeatureDFPAccel         : SubtargetFeature<"dfpaccel", "HasDFPAccel", "true",
                                               "Enable Xtensa Double Precision FP acceleration">;
def HasDFPAccel             : Predicate<"Subtarget->hasDFPAccel()">,
                                        AssemblerPredicate<"FeatureDFPAccel">;

def FeatureS32C1I           : SubtargetFeature<"s32c1i", "HasS32C1I", "true",
                                               "Enable Xtensa S32C1I option">;
def HasS32C1I               : Predicate<"Subtarget->hasS32C1I()">,
                                         AssemblerPredicate<"FeatureS32C1I">;

def FeatureTHREADPTR         : SubtargetFeature<"threadptr", "HasTHREADPTR", "true",
                                                "Enable Xtensa THREADPTR option">;
def HasTHREADPTR             : Predicate<"Subtarget->hasTHREADPTR()">,
                                          AssemblerPredicate<"FeatureTHREADPTR">;

def FeatureExtendedL32R      : SubtargetFeature<"extendedl32r", "HasExtendedL32R", "true",
                                                "Enable Xtensa Extended L32R option">;
def HasExtendedL32R          : Predicate<"Subtarget->hasExtendedL32R()">,
                                          AssemblerPredicate<"FeatureExtendedL32R">;

def FeatureATOMCTL           : SubtargetFeature<"atomctl", "HasATOMCTL", "true",
                                                "Enable Xtensa ATOMCTL option">;
def HasATOMCTL               : Predicate<"Subtarget->hasATOMCTL()">,
                                          AssemblerPredicate<"FeatureATOMCTL">;

def FeatureMEMCTL           : SubtargetFeature<"atomctl", "HasMEMCTL", "true",
                                                "Enable Xtensa MEMCTL option">;
def HasMEMCTL               : Predicate<"Subtarget->hasMEMCTL()">,
                                          AssemblerPredicate<"FeatureMEMCTL">;

def FeatureDebug             : SubtargetFeature<"debug", "HasDebug", "true",
                                                "Enable Xtensa Debug option">;
def HasDebug                 : Predicate<"Subtarget->hasDebug()">,
                                          AssemblerPredicate<"FeatureDebug">;

def FeatureException         : SubtargetFeature<"exception", "HasException", "true",
                                                "Enable Xtensa Exception option">;
def HasException             : Predicate<"Subtarget->hasException()">,
                                          AssemblerPredicate<"FeatureException">;

def FeatureHighPriInterrupts : SubtargetFeature<"exception",
                                                "HasHighPriInterrupts", "true",
                                                "Enable Xtensa HighPriInterrupts option">;
def HasHighPriInterrupts     : Predicate<"Subtarget->hasHighPriInterrupts()">,
                                          AssemblerPredicate<"FeatureHighPriInterrupts">;

def FeatureCoprocessor       : SubtargetFeature<"coprocessor", "HasCoprocessor", "true",
                                                "Enable Xtensa Coprocessor option">;
def HasCoprocessor           : Predicate<"Subtarget->hasCoprocessor()">,
                                          AssemblerPredicate<"FeatureCoprocessor">;

def FeatureInterrupt         : SubtargetFeature<"interrupt", "HasInterrupt", "true",
                                                "Enable Xtensa Interrupt option">;
def HasInterrupt             : Predicate<"Subtarget->hasInterrupt()">,
                                          AssemblerPredicate<"FeatureInterrupt">;

def FeatureRelocatableVector : SubtargetFeature<"rvector", "HasRelocatableVector", "true",
                                                "Enable Xtensa Relocatable Vector option">;
def HasRelocatableVector     : Predicate<"Subtarget->hasRelocatableVector()">,
                                          AssemblerPredicate<"FeatureRelocatableVector">;

def FeatureTimerInt          : SubtargetFeature<"timerint", "HasTimerInt", "true",
                                                "Enable Xtensa Timer Interrupt option">;
def HasTimerInt              : Predicate<"Subtarget->hasTimerInt()">,
                                          AssemblerPredicate<"FeatureTimerInt">;

def FeaturePRID              : SubtargetFeature<"prid", "HasPRID", "true",
                                                "Enable Xtensa Processor ID option">;
def HasPRID                  : Predicate<"Subtarget->hasPRID()">,
                                          AssemblerPredicate<"FeaturePRID">;

def FeatureRegionProtection  : SubtargetFeature<"regprotect", "HasRegionProtection", "true",
                                                "Enable Xtensa Region Protection option">;
def HasRegionProtection      : Predicate<"Subtarget->hasRegionProtection()">,
                                          AssemblerPredicate<"FeatureRegionProtection">;

def FeatureMiscSR            : SubtargetFeature<"miscsr", "HasMiscSR", "true",
                                                "Enable Xtensa Miscellaneous SR option">;
def HasMiscSR                : Predicate<"Subtarget->hasMiscSR()">,
                                          AssemblerPredicate<"FeatureMiscSR">;

//===----------------------------------------------------------------------===//
// Xtensa supported processors.
//===----------------------------------------------------------------------===//
class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;

def : Proc<"generic", []>;

def : Proc<"esp32", [FeatureDensity, FeatureSingleFloat, FeatureLoop, FeatureMAC16, FeatureWindowed, FeatureBoolean,
                     FeatureSEXT, FeatureNSA, FeatureMul32, FeatureMul32High, FeatureDFPAccel, FeatureS32C1I, FeatureTHREADPTR, FeatureDiv32,
                     FeatureATOMCTL, FeatureMEMCTL, FeatureDebug, FeatureException, FeatureHighPriInterrupts, FeatureCoprocessor,
                     FeatureInterrupt, FeatureRelocatableVector, FeatureTimerInt, FeaturePRID, FeatureRegionProtection, FeatureMiscSR]>;

def : Proc<"esp8266", [FeatureDensity, FeatureNSA, FeatureMul32, FeatureExtendedL32R, FeatureDebug, FeatureException, FeatureHighPriInterrupts,
                       FeatureInterrupt, FeatureRelocatableVector, FeatureTimerInt, FeatureRegionProtection, FeaturePRID]>;

def : Proc<"esp32-s2", [FeatureDensity, FeatureWindowed, FeatureSEXT, FeatureNSA, FeatureMul32, FeatureMul32High, FeatureTHREADPTR, FeatureDiv32,
                        FeatureDebug, FeatureException, FeatureHighPriInterrupts, FeatureCoprocessor, FeatureInterrupt, FeatureRelocatableVector,
						FeatureTimerInt, FeaturePRID, FeatureRegionProtection, FeatureMiscSR]>;

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "XtensaRegisterInfo.td"

//===----------------------------------------------------------------------===//
// Calling Convention Description
//===----------------------------------------------------------------------===//

include "XtensaCallingConv.td"

//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "XtensaInstrInfo.td"

def XtensaInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def XtensaAsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = 1;
}

def XtensaInstPrinter : AsmWriter
{
  string AsmWriterClassName  = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def Xtensa : Target
{
  let InstructionSet = XtensaInstrInfo;
  let AssemblyWriters = [XtensaInstPrinter];
  let AssemblyParsers = [XtensaAsmParser];
}
