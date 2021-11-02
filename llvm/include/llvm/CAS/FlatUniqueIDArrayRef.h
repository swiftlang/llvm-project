//===- llvm/CAS/FlatUniqueIDArrayRef.h --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_FLATUNIQUEIDARRAYREF_H
#define LLVM_CAS_FLATUNIQUEIDARRAYREF_H

#include "llvm/CAS/CASUniqueID.h"
#include "llvm/ADT/iterator.h"
#include "llvm/CAS/CASNamespace.h"

namespace llvm {
namespace cas {

/// Provide an array-like view over \a UniqueIDRef using a flattened array of
/// hashes stored consecutively in memory.
class FlatUniqueIDArrayRef {
public:
  class iterator : iterator_facade_base<iterator, std::random_access_iterator_tag, const UniqueIDRef,
                                        ptrdiff_t, const UniqueIDRef *, UniqueIDRef> {
    // FIXME: Merge from main, which already has this, and remove this copy's
    // operator->() in favour of the default implementation in
    // iterator_facade_base.
    class PointerProxy {
      friend iterator;
      UniqueIDRef ID;

    public:
      const UniqueIDRef *operator->() const { return &ID; }
    };

  public:
    UniqueIDRef operator*() const { return UniqueIDRef(makeArrayRef(I, I + Size)); }
    PointerProxy operator->() const { return PointerProxy{**this}; }
    iterator &operator++() { return *this += 1; }
    iterator &operator--() { return *this -= 1; }
    iterator &operator+=(ptrdiff_t Diff) {
      I += Diff * HashSize;
      return *this;
    }
    iterator &operator-=(ptrdiff_t Diff) {
      I -= Diff * HashSize;
      return *this;
    }
    bool operator<(iterator RHS) const { return I < RHS.I; }
    ptrdiff_t operator-(iterator RHS) const { return (I - RHS.I) / HashSize; }

  private:
    friend FlatUniqueIDArrayRef;
    iterator() = delete;
    iterator(size_t HashSize, const uint8_t *I) : HashSize(HashSize), I(I) {}
    size_t HashSize;
    const uint8_t *I;
  };

  iterator begin() const { return iterator(HashSize, Hashes.begin()); }
  iterator end() const { return iterator(HashSize, Hashes.end()); }
  size_t size() const { return Hashes.size() / HashSize; }
  bool empty() const { return Hashes.empty(); }
  UniqueIDRef operator[](ptrdiff_t I) const { return begin()[I]; }

  const Namespace &getNamespace() const { return Namespace; }
  size_t getHashSize() const { return HashSize; }
  ArrayRef<uint8_t> getFlatHashes() const { return FlatHashes; }

  FlatUniqueIDArrayRef(const Namespace &NS, ArrayRef<uint8_t> FlatHashes)
      : NS(&NS), FlatHashes(FlatHashes) {
    size_t HashSize = NS.getHashSize();
    assert(FlatHashes.size() / HashSize * HashSize == FlatHashes.size() &&
           "Expected contiguous hashes");
  }

private:
  const Namespace *NS = nullptr;
  ArrayRef<uint8_t> FlatHashes;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_FLATUNIQUEIDARRAYREF_H
