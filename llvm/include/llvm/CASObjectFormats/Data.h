//===- llvm/CASObjectFormats/Data.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_DATA_H
#define LLVM_CASOBJECTFORMATS_DATA_H

#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/Support/Alignment.h"
#include <type_traits>

namespace llvm {
namespace casobjectformats {
namespace data {

/// Stable enum to model parts of sys::Memory::ProtectionFlags relevant for
/// sections in object files.
enum SectionProtectionFlags {
  Read = 1,
  Write = 2,
  Exec = 4,
};

jitlink::MemProt decodeProtectionFlags(SectionProtectionFlags Perms);

SectionProtectionFlags encodeProtectionFlags(jitlink::MemProt Perms);

/// The kind and offset of a fixup (e.g., for a relocation).
struct Fixup {
  jitlink::Edge::Kind Kind;
  jitlink::Edge::OffsetT Offset;

  bool operator==(const Fixup &RHS) const {
    return Kind == RHS.Kind && Offset == RHS.Offset;
  }
  bool operator!=(const Fixup &RHS) const { return !operator==(RHS); }
};

/// An encoded list of \a Fixup.
///
/// FIXME: Encode kinds separately from what jitlink has, since they're not
/// stable.
class FixupList {
public:
  class iterator
      : public iterator_facade_base<iterator, std::forward_iterator_tag,
                                    const Fixup> {
    friend class FixupList;

  public:
    const Fixup &operator*() const { return *F; }
    iterator &operator++() {
      decode();
      return *this;
    }

    bool operator==(const iterator &RHS) const {
      return F == RHS.F && Data.begin() == RHS.Data.begin() &&
             Data.end() == RHS.Data.end();
    }

  private:
    void decode(bool IsInit = false);

    struct EndTag {};
    iterator(EndTag, StringRef Data) : Data(Data.end(), 0) {}
    explicit iterator(StringRef Data) : Data(Data) { decode(/*IsInit=*/true); }

    StringRef Data;
    Optional<Fixup> F;
  };

  iterator begin() const { return iterator(Data); }
  iterator end() const { return iterator(iterator::EndTag{}, Data); }
  bool empty() const { return begin() == end(); }

  static void encode(ArrayRef<const jitlink::Edge *> Edges,
                     SmallVectorImpl<char> &Data);

  static void encode(ArrayRef<Fixup> Fixups, SmallVectorImpl<char> &Data);

  FixupList() = default;
  explicit FixupList(StringRef Data) : Data(Data) {}

private:
  StringRef Data;
};

/// Block data, including fixups but not targets. This embeds a \a FixupList
/// directly.
///
/// data   ::= header size content? fixup-list? alignment-offset?
/// header ::= 6-bit alignment | IsZeroFill | HasAlignmentOffset
class BlockData {
  enum : unsigned {
    NumAlignmentBits = 6,
    AlignmentMask = (1u << NumAlignmentBits) - 1u,
    HasAlignmentOffsetBit = NumAlignmentBits,
    IsZeroFillBit,
    AfterHeader = 1,
  };
  static_assert(IsZeroFillBit < 8, "Ran out of bits");

public:
  BlockData() = delete;
  explicit BlockData(StringRef Data) : Data(Data) {
    assert(Data.size() >= 2 && "Expected at least two bytes of data");
  }

  /// Encode block data.
  ///
  /// - Pass \a None for \p Content to make it zero-fill.
  /// - Pass \a None for \p Fixups to encode them externally.
  ///
  /// Uses minimum of 2B (1B + VBR8(Size)). \p AlignmentOffset, \p Content, and
  /// \p Fixups add no storage cost if they are not used.
  static void encode(uint64_t Size, uint64_t Alignment,
                     uint64_t AlignmentOffset, Optional<StringRef> Content,
                     ArrayRef<Fixup> Fixups, SmallVectorImpl<char> &Data);

  Error decode(uint64_t &Size, uint64_t &Alignment, uint64_t &AlignmentOffset,
               Optional<StringRef> &Content, FixupList &Fixups) const;

  bool isZeroFill() const { return front() & (1u << IsZeroFillBit); }
  uint64_t getSize() const {
    StringRef Remaining = Data.drop_front(AfterHeader);
    return consumeSizeFatal(Remaining);
  }
  uint64_t getAlignment() const {
    return llvm::decodeMaybeAlign(front() & AlignmentMask).valueOrOne().value();
  }
  uint64_t getAlignmentOffset() const {
    return hasAlignmentOffset() ? decodeAlignmentOffset() : 0;
  }
  Optional<ArrayRef<char>> getContentArray() const {
    if (Optional<StringRef> Content = getContent())
      return makeArrayRef(Content->begin(), Content->end());
    return None;
  }
  Optional<StringRef> getContent() const;
  FixupList getFixups() const;

private:
  static size_t getNumAlignmentOffsetBytes(uint64_t Alignment) {
    return Alignment < (1ULL << 8)
               ? 1
               : Alignment < (1ULL << 16) ? 2
                                          : Alignment < (1ULL << 32) ? 4 : 8;
  }
  static Error consumeContent(StringRef &Remaining, uint64_t Size,
                              Optional<StringRef> &Content);
  static Error consumeSize(StringRef &Remaining, uint64_t &Size);
  static uint64_t consumeSizeFatal(StringRef &Remaining);
  bool hasAlignmentOffset() const {
    return front() & (1u << HasAlignmentOffsetBit);
  }
  uint64_t decodeAlignmentOffset() const;
  uint8_t front() const { return Data.front(); }

  StringRef Data;
};

/// Information about how to apply a \a Fixup to a target, including the addend
/// and an index into the \a TargetList.
struct TargetInfo {
  /// Addend to apply to the target address.
  jitlink::Edge::AddendT Addend;

  /// Index into the list of targets.
  size_t Index;

  bool operator==(const TargetInfo &RHS) const {
    return Addend == RHS.Addend && Index == RHS.Index;
  }
  bool operator!=(const TargetInfo &RHS) const { return !operator==(RHS); }
};

/// An encoded list of \a TargetInfo, parallel with \a FixupList.
///
/// FIXME: Optimize the encoding further. Currently the index is stored as VBR8,
/// but we know that the indexes are all smaller than the TargetList. For lists
/// with 128 or fewer targets, we could get smaller.
///
/// There are very few instances of \a TargetInfoListRef (they dedup
/// surprisingly well), so no benefit there, but the ones embedded in \a
/// BlockRef can't be deduped. The statistics for "cas.o:block"'s inline data
/// should show the headroom available there.
class TargetInfoList {
public:
  class iterator
      : public iterator_facade_base<iterator, std::forward_iterator_tag,
                                    const TargetInfo> {
    friend class TargetInfoList;

  public:
    const TargetInfo &operator*() const { return *TI; }
    iterator &operator++() {
      decode();
      return *this;
    }

    bool operator==(const iterator &RHS) const {
      return TI == RHS.TI && Data.begin() == RHS.Data.begin() &&
             Data.end() == RHS.Data.end();
    }

  private:
    void decode(bool IsInit = false);

    struct EndTag {};
    iterator(EndTag, StringRef Data) : Data(Data.end(), 0) {}
    explicit iterator(StringRef Data) : Data(Data) { decode(/*IsInit=*/true); }

    StringRef Data;
    Optional<TargetInfo> TI;
  };

  iterator begin() const { return iterator(Data); }
  iterator end() const { return iterator(iterator::EndTag{}, Data); }

  static void encode(ArrayRef<TargetInfo> TIs, SmallVectorImpl<char> &Data);

  TargetInfoList() = default;
  explicit TargetInfoList(StringRef Data) : Data(Data) {}

private:
  StringRef Data;
};

} // end namespace data
} // end namespace casobjectformats
} // end namespace llvm

#endif // LLVM_CASOBJECTFORMATS_DATA_H
