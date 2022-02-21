//===- llvm/MC/MCMachOCASWriter.h - Mach CAS Object Writer ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCMACHOCASWRITER_H
#define LLVM_MC_MCMACHOCASWRITER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCMachObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/StringTableBuilder.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetOptions.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace llvm {

namespace cas {
class CASDB;
} // namespace cas

class MachOCASWriter : public MCObjectWriter {
  /// CASDB
  cas::CASDB &CAS;
  CASObjectFormat Format;

  SmallString<0> ObjectData;
  raw_svector_ostream ObjectOS;
  raw_pwrite_stream &OS;

  MachObjectWriter MOW;

public:
  MachOCASWriter(std::unique_ptr<MCMachObjectTargetWriter> MOTW,
                 cas::CASDB &CAS, CASObjectFormat Format, raw_pwrite_stream &OS,
                 bool IsLittleEndian);

  /// Forward everything to underlying MachOObjectWriter.
  void reset() override {
    MOW.reset();
  }

  void executePostLayoutBinding(MCAssembler &Asm,
                                const MCAsmLayout &Layout) override {
    MOW.executePostLayoutBinding(Asm, Layout);
  }

  void recordRelocation(MCAssembler &Asm, const MCAsmLayout &Layout,
                        const MCFragment *Fragment, const MCFixup &Fixup,
                        MCValue Target, uint64_t &FixedValue) override {
    MOW.recordRelocation(Asm, Layout, Fragment, Fixup, Target, FixedValue);
  }

  bool isSymbolRefDifferenceFullyResolvedImpl(const MCAssembler &Asm,
                                              const MCSymbol &A,
                                              const MCSymbol &B,
                                              bool InSet) const override {
    return MOW.isSymbolRefDifferenceFullyResolvedImpl(Asm, A, B, InSet);
  }

  bool isSymbolRefDifferenceFullyResolvedImpl(const MCAssembler &Asm,
                                              const MCSymbol &SymA,
                                              const MCFragment &FB, bool InSet,
                                              bool IsPCRel) const override {
    return MOW.isSymbolRefDifferenceFullyResolvedImpl(Asm, SymA, FB, InSet,
                                                      IsPCRel);
  }

  uint64_t writeObject(MCAssembler &Asm, const MCAsmLayout &Layout) override;
};

/// Construct a new Mach-O CAS writer instance.
///
/// This routine takes ownership of the target writer subclass.
///
/// \param MOTW - The target specific Mach-O writer subclass.
/// \param TT - The target triple.
/// \param CAS - The CASDB instance.
/// \param OS - The stream to write to.
/// \returns The constructed object writer.
std::unique_ptr<MCObjectWriter>
createMachOCASWriter(std::unique_ptr<MCMachObjectTargetWriter> MOTW,
                     cas::CASDB &CAS, CASObjectFormat Format,
                     raw_pwrite_stream &OS, bool IsLittleEndian);

} // end namespace llvm

#endif // LLVM_MC_MCMACHOCASWRITER_H
