//===-----------------------------------------------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_CAS_DUMP_CASDWARFOBJECT_H
#define LLVM_TOOLS_LLVM_CAS_DUMP_CASDWARFOBJECT_H

#include "llvm/DebugInfo/DWARF/DWARFCompileUnit.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFDataExtractor.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugAbbrev.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugLine.h"
#include "llvm/DebugInfo/DWARF/DWARFObject.h"
#include "llvm/MC/CAS/MCCASObjectV1.h"
#include <unordered_map>
#include <unordered_set>

namespace llvm {

/// A DWARFObject implementation that just supports enough to
/// dwarfdump section contents.
class CASDWARFObject : public DWARFObject {
  bool Is64Bit = true;
  bool IsLittleEndian = true;
  Triple Target;
  SmallVector<uint8_t> DebugStringSection;
  SmallVector<uint8_t> DebugAbbrevSection;
  SmallVector<size_t> DebugAbbrevOffsets;
  SmallVector<char> DebugInfoSection;
  SmallVector<StringRef> CUDataVec;
  SmallVector<uint32_t> SecOffsetVals;
  unsigned LineTableOffset = 0;
  DenseMap<cas::ObjectRef, unsigned> MapOfStringOffsets;
  static std::unordered_map<std::string, std::unordered_set<std::string>>
      MapOfLinkageNames;
  unsigned CompileUnitIndex = 0;

  const mccasformats::v1::MCSchema &Schema;

  /// Function to get the MCObjectProxy for a ObjectRef \p CASObj and pass it to
  /// discoverDwarfSections(mccasformats::v1::MCObjectProxy MCObj);
  Error discoverDwarfSections(cas::ObjectRef CASObj);

  /// Function to get the MCObjectProxy for a ObjectRef \p CASObj and pass it to
  /// discoverDebugInfoSection(mccasformats::v1::MCObjectProxy MCObj,
  /// raw_ostream &OS);
  Error discoverDebugInfoSection(cas::ObjectRef CASObj, raw_ostream &OS);

  void addLinkageNameAndObjectRefToMap(DWARFDie CUDie,
                                       mccasformats::v1::MCObjectProxy MCObj,
                                       bool &LinkageFound,
                                       std::string InputStr);

public:
  CASDWARFObject(const mccasformats::v1::MCSchema &Schema) : Schema(Schema) {}

  /// Copy contents of any dwarf sections that are interesting for dwarfdump to
  /// work.
  Error discoverDwarfSections(mccasformats::v1::MCObjectProxy MCObj);

  /// Takes an MCObjectProxy \p MCObj and if it is a DebugInfoSectionRef, writes
  /// its materialized contents into the \p OS.
  Error discoverDebugInfoSection(mccasformats::v1::MCObjectProxy MCObj,
                                 raw_ostream &OS);

  void addLinkageNameAndObjectRefToMap(DWARFDie CUDie,
                                       mccasformats::v1::MCObjectProxy MCObj,
                                       bool &LinkageFound);

  /// Dump MCObj as textual DWARF output.
  Error dump(raw_ostream &OS, int Indent, DWARFContext &DWARFCtx,
             mccasformats::v1::MCObjectProxy MCObj, bool ShowForm, bool Verbose,
             bool DumpSameLinkageDifferentCU, std::string InputStr);

  StringRef getFileName() const override { return "CAS"; }
  ArrayRef<SectionName> getSectionNames() const override { return {}; }
  bool isLittleEndian() const override { return IsLittleEndian; }
  uint8_t getAddressSize() const override { return Is64Bit ? 8 : 4; }
  StringRef getAbbrevSection() const override {
    return toStringRef(DebugAbbrevSection);
  }
  StringRef getStrSection() const override {
    return llvm::toStringRef(DebugStringSection);
  }
  Optional<RelocAddrEntry> find(const DWARFSection &Sec,
                                uint64_t Pos) const override {
    return {};
  };
  ArrayRef<uint32_t> getSecOffsetVals() { return SecOffsetVals; }

  Error dumpSimilarCUs(llvm::mccasformats::v1::MCSchema &MCSchema);
};
} // namespace llvm

#endif
