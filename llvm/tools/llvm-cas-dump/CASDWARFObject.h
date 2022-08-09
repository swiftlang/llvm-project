//===-----------------------------------------------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_CAS_DUMP_CASDWARFOBJECT_H
#define LLVM_TOOLS_LLVM_CAS_DUMP_CASDWARFOBJECT_H

#include "MCCASPrinter.h"
#include "llvm/DebugInfo/DWARF/DWARFObject.h"
#include "llvm/MC/CAS/MCCASObjectV1.h"

namespace llvm {

/// A DWARFObject implementation that just supports enough to
/// dwarfdump section contents.
class CASDWARFObject : public DWARFObject {
  bool Is64Bit = true;
  bool IsLittleEndian = true;
  SmallVector<uint8_t> DebugStringSection;

  SmallVector<uint8_t> DebugAbbrevSection;

  const mccasformats::v1::MCSchema &Schema;

  Error discoverDwarfSections(cas::ObjectRef CASObj);

public:
  CASDWARFObject(const mccasformats::v1::MCSchema &Schema) : Schema(Schema) {}

  Error discoverDwarfSections(mccasformats::v1::MCObjectProxy MCObj);

  /// Dump MCObj as textual DWARF output.
  Error dump(raw_ostream &OS, int Indent, DWARFContext &DWARFCtx,
             mccasformats::v1::MCObjectProxy MCObj) const;

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
};
} // namespace llvm

#endif
