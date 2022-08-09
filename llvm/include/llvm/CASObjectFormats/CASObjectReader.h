//===- llvm/CASObjectFormats/CASObjectReader.h ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_CASOBJECTREADER_H
#define LLVM_CASOBJECTFORMATS_CASOBJECTREADER_H

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/JITLink/MemoryFlags.h"
#include "llvm/Support/Error.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/CASDB.h"

namespace llvm {

class Triple;

// FIXME: Replace the jitlink types with ones native to casobjectformats.
namespace jitlink {
enum class Linkage : uint8_t;
enum class Scope : uint8_t;
} // namespace jitlink

namespace casobjectformats {
namespace reader {

/// Only valid to be used with the same \p CASObjectReader instance it came
/// from.
struct CASSectionRef {
  uint64_t Idx;
};

/// Only valid to be used with the same \p CASObjectReader instance it came
/// from.
struct CASBlockRef {
  uint64_t Idx;
};

/// Only valid to be used with the same \p CASObjectReader instance it came
/// from.
struct CASSymbolRef {
  uint64_t Idx;
};

struct CASSection {
  StringRef Name;
  jitlink::MemProt Prot;
};

struct CASBlock {
  uint64_t Size;
  uint64_t Alignment;
  uint64_t AlignmentOffset;
  Optional<StringRef> Content;
  CASSectionRef SectionRef;
  llvm::cas::CASID BlockContentID;
  CASBlock(uint64_t Size, uint64_t Alignment, uint64_t AlignmentOffset,
           Optional<StringRef> Content, CASSectionRef SectionRef,
           cas::CASID BlockContentID)
      : Size(Size), Alignment(Alignment), AlignmentOffset(AlignmentOffset),
        Content(Content), SectionRef(SectionRef),
        BlockContentID(BlockContentID) {}

  bool isZeroFill() const { return !Content.has_value(); }
};

struct CASBlockFixup {
  uint32_t Offset;
  /// A jitlink::Edge::Kind value.
  uint8_t Kind;
  int64_t Addend;
  CASSymbolRef TargetRef;
};

struct CASSymbol {
  StringRef Name;
  unsigned Offset;
  jitlink::Linkage Linkage;
  jitlink::Scope Scope;
  bool IsLive;
  bool IsCallable;
  bool IsAutoHide;
  Optional<CASBlockRef> BlockRef;

  bool isDefined() const { return BlockRef.has_value(); }
};

/// Provides information about the symbols/blocks/sections that a CAS schema
/// object encoded. The returned \p StringRefs have the same lifetime as the CAS
/// database instance. Deterministic and thread-safe.
class CASObjectReader {
public:
  virtual ~CASObjectReader() = default;

  virtual Triple getTargetTriple() const = 0;

  /// Get all symbols in the translation unit.
  /// FIXME: Nestedv1 schema doesn't provide all the symbols that can be
  /// referenced. More can be discovered via fixups.
  virtual Error forEachSymbol(
      function_ref<Error(CASSymbolRef, CASSymbol)> Callback) const = 0;

  virtual Expected<CASSection> materialize(CASSectionRef Ref) const = 0;
  virtual Expected<CASBlock> materialize(CASBlockRef Ref) const = 0;
  virtual Expected<CASSymbol> materialize(CASSymbolRef Ref) const = 0;

  virtual Error materializeFixups(
      CASBlockRef Ref,
      function_ref<Error(const CASBlockFixup &)> Callback) const = 0;

protected:
  CASObjectReader() = default;
};

} // namespace reader
} // namespace casobjectformats
} // namespace llvm

namespace llvm {

template <> struct DenseMapInfo<casobjectformats::reader::CASSectionRef> {
  static casobjectformats::reader::CASSectionRef getEmptyKey() {
    return casobjectformats::reader::CASSectionRef{
        DenseMapInfo<uint64_t>::getEmptyKey()};
  }

  static casobjectformats::reader::CASSectionRef getTombstoneKey() {
    return casobjectformats::reader::CASSectionRef{
        DenseMapInfo<uint64_t>::getTombstoneKey()};
  }

  static unsigned getHashValue(casobjectformats::reader::CASSectionRef ID) {
    return DenseMapInfo<uint64_t>::getHashValue(ID.Idx);
  }

  static bool isEqual(casobjectformats::reader::CASSectionRef LHS,
                      casobjectformats::reader::CASSectionRef RHS) {
    return LHS.Idx == RHS.Idx;
  }
};

template <> struct DenseMapInfo<casobjectformats::reader::CASBlockRef> {
  static casobjectformats::reader::CASBlockRef getEmptyKey() {
    return casobjectformats::reader::CASBlockRef{
        DenseMapInfo<uint64_t>::getEmptyKey()};
  }

  static casobjectformats::reader::CASBlockRef getTombstoneKey() {
    return casobjectformats::reader::CASBlockRef{
        DenseMapInfo<uint64_t>::getTombstoneKey()};
  }

  static unsigned getHashValue(casobjectformats::reader::CASBlockRef ID) {
    return DenseMapInfo<uint64_t>::getHashValue(ID.Idx);
  }

  static bool isEqual(casobjectformats::reader::CASBlockRef LHS,
                      casobjectformats::reader::CASBlockRef RHS) {
    return LHS.Idx == RHS.Idx;
  }
};

template <> struct DenseMapInfo<casobjectformats::reader::CASSymbolRef> {
  static casobjectformats::reader::CASSymbolRef getEmptyKey() {
    return casobjectformats::reader::CASSymbolRef{
        DenseMapInfo<uint64_t>::getEmptyKey()};
  }

  static casobjectformats::reader::CASSymbolRef getTombstoneKey() {
    return casobjectformats::reader::CASSymbolRef{
        DenseMapInfo<uint64_t>::getTombstoneKey()};
  }

  static unsigned getHashValue(casobjectformats::reader::CASSymbolRef ID) {
    return DenseMapInfo<uint64_t>::getHashValue(ID.Idx);
  }

  static bool isEqual(casobjectformats::reader::CASSymbolRef LHS,
                      casobjectformats::reader::CASSymbolRef RHS) {
    return LHS.Idx == RHS.Idx;
  }
};

} // namespace llvm

#endif // LLVM_CASOBJECTFORMATS_CASOBJECTREADER_H
