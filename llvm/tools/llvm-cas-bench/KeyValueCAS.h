//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Adapts a plain "32-byte digest -> opaque blob" store into a full
/// llvm::cas::ObjectStore, so the benchmark can drive every backend through
/// the same public CAS API.
///
/// Deriving from BuiltinCAS means BLAKE3 hashing, CASID parsing and printing,
/// object identity and the error vocabulary are all the shared LLVMCAS code
/// rather than reimplementations. A subclass supplies only get/put/contains.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_CAS_BENCH_KEYVALUECAS_H
#define LLVM_TOOLS_LLVM_CAS_BENCH_KEYVALUECAS_H

#include "BuiltinCAS.h" // llvm/lib/CAS, on the include path via CMake.
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CAS/BuiltinCASContext.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Errc.h"

#include <array>
#include <optional>
#include <vector>

namespace llvm {
namespace casbench {

/// A BLAKE3 digest: the same 32-byte hash type LLVMCAS uses.
using Digest = cas::builtin::HashType;
static_assert(std::tuple_size<Digest>::value == 32, "expected 32-byte BLAKE3");

/// On-the-wire form of one object in a key/value backend:
///
///   <LE u32 num refs><num refs * 32 raw digest bytes><data...><NUL>
///
/// The data length is implied by the blob length, so the overhead is 4 bytes
/// plus the terminator. The terminator is not counted in the data and exists
/// only because BuiltinCAS promises null-terminated object data.
///
/// Note the 32 bytes per reference: LLVMCAS names a child by a compact
/// internal offset, but a digest-keyed store has no such handle and has to
/// spell the whole digest out. On a real build graph that can outweigh the
/// object data itself.
namespace wire {

constexpr size_t HeaderSize = sizeof(uint32_t);

inline size_t sizeFor(uint32_t NumRefs, size_t DataSize) {
  return HeaderSize + size_t(NumRefs) * sizeof(Digest) + DataSize + 1;
}

inline uint32_t getNumRefs(ArrayRef<uint8_t> Blob) {
  return support::endian::read32le(Blob.data());
}

inline const Digest *getRefs(ArrayRef<uint8_t> Blob) {
  return reinterpret_cast<const Digest *>(Blob.data() + HeaderSize);
}

inline ArrayRef<char> getData(ArrayRef<uint8_t> Blob) {
  size_t Offset = HeaderSize + size_t(getNumRefs(Blob)) * sizeof(Digest);
  return ArrayRef<char>(reinterpret_cast<const char *>(Blob.data()) + Offset,
                        Blob.size() - Offset - 1);
}

/// Verifies that \p Blob is long enough for the reference count it declares.
inline bool isWellFormed(ArrayRef<uint8_t> Blob) {
  if (Blob.size() < HeaderSize + 1)
    return false;
  return Blob.size() >= sizeFor(getNumRefs(Blob), 0);
}

} // namespace wire

} // namespace casbench

/// Must precede any DenseMap<Digest, ...> below.
template <> struct DenseMapInfo<casbench::Digest> {
  static inline casbench::Digest getEmptyKey() {
    casbench::Digest D;
    D.fill(0xFF);
    return D;
  }
  static inline casbench::Digest getTombstoneKey() {
    casbench::Digest D;
    D.fill(0xFE);
    return D;
  }
  // A BLAKE3 digest is already uniformly distributed.
  static unsigned getHashValue(const casbench::Digest &D) {
    return support::endian::read32le(D.data());
  }
  static bool isEqual(const casbench::Digest &LHS,
                      const casbench::Digest &RHS) {
    return LHS == RHS;
  }
};

namespace casbench {

/// ObjectStore over a digest-keyed blob store.
///
/// ObjectRef and ObjectHandle are both indices into a table that interns
/// every digest this store has seen. Interning is what lets a digest-keyed
/// backend hand out the cheap opaque handles the ObjectStore API expects.
///
/// Nothing a lookup returns is kept: every load goes to the backend, just as
/// every LLVMCAS load reads its index. A handle therefore names the blob this
/// store loaded last and is good until the next load, which is enough for a
/// walk that loads an object and reads its data and references before
/// loading another. ObjectStore promises more than that in general.
///
/// An instance is not thread-safe. A parallel traversal gives each thread its
/// own, as separate processes sharing a store would have.
class KeyValueCAS : public cas::builtin::BuiltinCAS {
public:
  //===--------------------------------------------------------------------===//
  // Subclass interface: the whole of what a backend has to provide.
  //===--------------------------------------------------------------------===//

  /// Store \p Blob under \p Key. Must be idempotent: the caller may offer a
  /// key that is already present.
  virtual Error putBlob(ArrayRef<uint8_t> Key, ArrayRef<uint8_t> Blob) = 0;

  /// Look up \p Key. Returns \c std::nullopt when absent.
  ///
  /// The returned bytes need only stay valid until the next call. Backends
  /// that cannot point into storage of their own read into \a scratch().
  virtual Expected<std::optional<ArrayRef<uint8_t>>>
  getBlob(ArrayRef<uint8_t> Key) = 0;

  virtual Expected<bool> containsBlob(ArrayRef<uint8_t> Key) = 0;

  //===--------------------------------------------------------------------===//
  // ObjectStore, implemented once for every key/value backend.
  //===--------------------------------------------------------------------===//

  Expected<cas::ObjectRef> storeImpl(ArrayRef<uint8_t> ComputedHash,
                                     ArrayRef<cas::ObjectRef> Refs,
                                     ArrayRef<char> Data) final;

  Expected<std::optional<cas::ObjectHandle>>
  loadIfExists(cas::ObjectRef Ref) final;

  cas::CASID getID(cas::ObjectRef Ref) const final {
    return cas::CASID::create(&getContext(),
                              toStringRef(ArrayRef<uint8_t>(digestOf(Ref))));
  }

  std::optional<cas::ObjectRef> getReference(const cas::CASID &ID) const final;

  Expected<bool> isMaterialized(cas::ObjectRef Ref) const final {
    return const_cast<KeyValueCAS *>(this)->containsBlob(digestOf(Ref));
  }

  ArrayRef<char> getDataConst(cas::ObjectHandle) const final {
    return wire::getData(LastBlob);
  }

  size_t getNumRefs(cas::ObjectHandle) const final {
    return wire::getNumRefs(LastBlob);
  }

  cas::ObjectRef readRef(cas::ObjectHandle, size_t I) const final {
    return intern(wire::getRefs(LastBlob)[I]);
  }

  Error forEachRef(cas::ObjectHandle Node,
                   function_ref<Error(cas::ObjectRef)> Callback) const final;

  Error validate(bool CheckHash) const final;

protected:
  /// A buffer of \p Size bytes owned by this store, reused by the next load.
  MutableArrayRef<uint8_t> scratch(size_t Size) {
    Scratch.resize_for_overwrite(Size);
    return Scratch;
  }

  /// \p Blob copied into \a scratch(), for backends whose own buffer does not
  /// survive the call that produced it.
  ArrayRef<uint8_t> copyToScratch(ArrayRef<uint8_t> Blob) {
    MutableArrayRef<uint8_t> P = scratch(Blob.size());
    llvm::copy(Blob, P.begin());
    return P;
  }

private:
  /// Index of \p Key, adding it if this is the first time it has been seen.
  /// Interning a digest says nothing about whether the object is present.
  uint32_t internIndex(const Digest &Key) const;

  cas::ObjectRef intern(const Digest &Key) const {
    return makeObjectRef(internIndex(Key));
  }

  uint32_t indexOf(cas::ObjectRef Ref) const {
    return uint32_t(Ref.getInternalRef(*this));
  }

  const Digest &digestOf(cas::ObjectRef Ref) const {
    return Digests[indexOf(Ref)];
  }

  // Mutable because ObjectStore hands out refs from const accessors, and
  // interning a previously unseen child digest is how that has to work here.
  mutable std::vector<Digest> Digests;
  mutable DenseMap<Digest, uint32_t> Indices;
  /// What the last load found, which every handle refers to.
  ArrayRef<uint8_t> LastBlob;
  SmallVector<uint8_t, 4096> Scratch;
  /// Reused across storeImpl() calls so encoding never allocates.
  SmallVector<uint8_t, 4096> BlobBuf;
};

} // namespace casbench

} // namespace llvm

#endif // LLVM_TOOLS_LLVM_CAS_BENCH_KEYVALUECAS_H
