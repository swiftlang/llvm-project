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
#include <type_traits>

namespace llvm {
namespace casobjectformats {
namespace data {

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

  static void encode(ArrayRef<const jitlink::Edge *> Edges,
                     SmallVectorImpl<char> &Data);

  static void encode(ArrayRef<Fixup> Fixups, SmallVectorImpl<char> &Data);

  FixupList() = default;
  explicit FixupList(StringRef Data) : Data(Data) {}

private:
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
