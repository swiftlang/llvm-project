//===- llvm/CAS/UniqueID.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_UNIQUEID_H
#define LLVM_CAS_UNIQUEID_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/CAS/Namespace.h"

namespace llvm {

class raw_ostream;

namespace cas {

/// A unique ID for the CAS that does not own storage for the hash. Like a \a
/// StringRef, users must be careful of the lifetime of the underlying storage.
///
/// There are two components:
///
/// - The \a Namespace returned by \a getNamespace() is the domain this
///   identifier, which implicitly determines the hash function and schema for
///   objects in the CAS.
/// - The hash returned by \a getHash() is the hash that idnetifies an object
///   in the namespace.
///
/// \a UniqueIDRef is valid by construction (barring lifetime issues) and does
/// not support an "uninitialized" state. Delayed initialization patterns
/// require an \a Optional. However, it does support sentinel values for use
/// with \a DenseMapInfo<UniqueIDRef>.
class UniqueIDRef {
  /// Fixed set of expected sizes for hashes. Allows comparing hashes without
  /// chasing the Namespace pointer. Other sizes still work, but may be slower.
  ///
  /// If it's useful, we can add more values here. Or, maybe chasing the
  /// Namespace::getHashSize() pointer is not so bad.
  enum HashSize {
    NotEmbedded,
    Embedded20B, // 160 bits
    Embedded32B, // 256 bits
    // Can fill common values can go here.
  };
  enum : size_t {
    // Expect hashes to be at least 8B. This enables comparing hash prefixes
    // without knowing their size.
    MinHashSize = 8,
  };

public:
  size_t getHashSize() const {
    assert(NamespaceAndHashSize.getPointer() && "Expected valid namespace");

    HashSize HS = NamespaceAndHashSize.getInt();
    if (HS == Embedded20B)
      return 20;
    if (HS == Embedded32B)
      return 32;
    return NamespaceAndHashSize.getPointer()->getHashSize();
  }

  const Namespace &getNamespace() const {
    assert(NamespaceAndHashSize.getPointer() && "Expected valid namespace");
    return *NamespaceAndHashSize.getPointer();
  }

  void dump() const;
  void print(raw_ostream &OS) const { getNamespace().printID(*this, OS); }
  friend raw_ostream &operator<<(raw_ostream &OS, UniqueIDRef ID) {
    ID.print(OS);
    return OS;
  }

  ArrayRef<uint8_t> getHash() const {
    return makeArrayRef(Hash, Hash + getHashSize());
  }

  /// Get the embedded sentinel pointer for \a DenseMapInfo<UniqueIDRef>.
  const void *getSentinelPointer() const {
    return NamespaceAndHashSize.getPointer() ? nullptr : Hash;
  }

  friend bool operator==(UniqueIDRef LHS, UniqueIDRef RHS) {
    if (LHS.NamespaceAndHashSize != RHS.NamespaceAndHashSize)
      return false;

    // Check the prefix that's guaranteed by MinHashSize since this doesn't
    // require loading the hash size from the namespace.
    if (makeArrayRef(LHS.Hash, MinHashSize) !=
        makeArrayRef(RHS.Hash, MinHashSize))
      return false;

    // Avoid loading the hash size from the namespace if the pointers happen to
    // be equal.
    if (LHS.Hash == RHS.Hash)
      return true;

    // Compare the full hash.
    return LHS.getHash().drop_front(MinHashSize) ==
           RHS.getHash().drop_front(MinHashSize);
  }
  friend bool operator!=(UniqueIDRef LHS, UniqueIDRef RHS) {
    return !(LHS == RHS);
  }

  /// Get a 64-bit hash value. \a getHash() is assumed to be a strong hash
  /// value; this returns its 64-bit prefix.
  uint64_t getHashValue() const {
    uint64_t Value = 0;
    for (size_t I = 0, E = sizeof(uint64_t); I != E; ++I)
      Value |= Hash[I] << (I * 8);
    return Value;
  }
  friend hash_code hash_value(UniqueIDRef ID) {
    return hash_code(ID.getHashValue());
  }

  UniqueIDRef() = delete;
  UniqueIDRef(UniqueIDRef &&) = default;
  UniqueIDRef(const UniqueIDRef &) = default;
  UniqueIDRef &operator=(UniqueIDRef &&) = default;
  UniqueIDRef &operator=(const UniqueIDRef &) = default;

  UniqueIDRef(const Namespace &NS, ArrayRef<uint8_t> Hash)
      : NamespaceAndHashSize(&NS), Hash(Hash.begin()) {
    assert(NS.getHashSize() >= MinHashSize && "Expected hash not to be small");
    assert(NS.getHashSize() == Hash.size() && "Expected valid hash for namespace");
    if (NS.getHashSize() == 20)
      NamespaceAndHashSize.setInt(Embedded20B);
    else if (NS.getHashSize() == 32)
      NamespaceAndHashSize.setInt(Embedded32B);
  }

  struct DenseMapTombstoneTag {};
  struct DenseMapEmptyTag {};

  explicit UniqueIDRef(DenseMapTombstoneTag)
      : Hash(DenseMapInfo<const uint8_t *>::getTombstoneKey()) {}
  explicit UniqueIDRef(DenseMapEmptyTag)
      : Hash(DenseMapInfo<const uint8_t *>::getEmptyKey()) {}

private:
  PointerIntPair<const Namespace *, 3, HashSize> NamespaceAndHashSize;
  const uint8_t *Hash = nullptr;
};

} // end namespace cas

template <> struct DenseMapInfo<cas::UniqueIDRef> {
  static cas::UniqueIDRef getEmptyKey() {
    return cas::UniqueIDRef(cas::UniqueIDRef::DenseMapEmptyTag{});
  }

  static cas::UniqueIDRef getTombstoneKey() {
    return cas::UniqueIDRef(cas::UniqueIDRef::DenseMapTombstoneTag{});
  }

  static unsigned getHashValue(cas::UniqueIDRef ID) { return ID.getHashValue(); }

  static bool isEqual(cas::UniqueIDRef LHS, cas::UniqueIDRef RHS) {
    const void *SentinelLHS = LHS.getSentinelPointer();
    const void *SentinelRHS = RHS.getSentinelPointer();
    if (SentinelLHS || SentinelRHS)
      return SentinelLHS == SentinelRHS;
    return LHS == RHS;
  }
};

namespace cas {

/// A unique ID for the CAS that owns storage for a hash. Uses small string
/// optimization for hashes within 32B.
///
/// \a UniqueID supports an "uninitialized" state. This falls out from owning
/// movable storage. When uninitialized, \a isValid() returns \c false and
/// \a getNamespace() and \a getHash() assert.
class UniqueID {
public:
  /// Check if this ID is valid.
  bool isValid() const { return NS; }
  explicit operator bool() const { return isValid(); }

  const Namespace &getNamespace() const {
    assert(isValid() && "Cannot get namespace from invalid ID");
    return *NS;
  }

  /// Get the raw bytes of the hash.
  ArrayRef<uint8_t> getHash() const {
    assert(isValid() && "Cannot get hash from invalid ID");
    const uint8_t *HashPtr = isSmall() ? Storage.Hash : Storage.Mem;
    return makeArrayRef(HashPtr, HashPtr + getNamespace().getHashSize());
  }

  /// Get a reference into this storage.
  operator UniqueIDRef() const {
    assert(isValid() && "Cannot convert invalid UniqueID to reference");
    return UniqueIDRef(getNamespace(), getHash());
  }

  friend bool operator==(const UniqueID &LHS, const UniqueID &RHS) {
    if (!LHS.isValid() || !RHS.isValid())
      return LHS.isValid() == RHS.isValid();
    return UniqueIDRef(LHS) == UniqueIDRef(RHS);
  }
  friend bool operator==(const UniqueIDRef &LHS, const UniqueID &RHS) {
    if (!RHS.isValid())
      return false;
    return LHS == UniqueIDRef(RHS);
  }
  friend bool operator==(const UniqueID &LHS, const UniqueIDRef &RHS) {
    return RHS == LHS;
  }
  friend bool operator!=(const UniqueID &LHS, const UniqueID &RHS) {
    return !(LHS == RHS);
  }
  friend bool operator!=(const UniqueID &LHS, const UniqueIDRef &RHS) {
    return !(LHS == RHS);
  }
  friend bool operator!=(const UniqueIDRef &LHS, const UniqueID &RHS) {
    return !(LHS == RHS);
  }

  void dump() const;

  /// If valid, call \a UniqueIDRef::print(); else, print a sentinel.
  void print(raw_ostream &OS) const;
  friend raw_ostream &operator<<(raw_ostream &OS, const UniqueID &ID) {
    ID.print(OS);
    return OS;
  }

  UniqueID() = default;
  UniqueID(UniqueID &&ID) { moveImpl(ID); }
  UniqueID(const UniqueID &ID) {
    if (ID)
      copyImpl(ID);
  }

  UniqueID &operator=(UniqueID &&ID) {
    destroy();
    return moveImpl(ID);
  }
  UniqueID &operator=(const UniqueID &ID) {
    if (ID)
      return copyImpl(ID);
    destroy();
    NS = nullptr;
    return *this;
  }

  UniqueID(UniqueIDRef ID) { copyImpl(ID); }
  UniqueID &operator=(UniqueIDRef ID) {
    return copyImpl(ID);
  }

  UniqueID(const Namespace &NS, ArrayRef<uint8_t> Hash)
      : UniqueID(UniqueIDRef(NS, Hash)) {}

  ~UniqueID() { destroy(); }

private:
  void allocate() {
    if (!isSmall())
      Storage.Mem = new uint8_t[NS->getHashSize()];
  }
  void destroy() {
    if (!isSmall())
      delete[] Storage.Mem;
  }
  UniqueID &copyImpl(UniqueIDRef ID) {
    if (NS != &ID.getNamespace()) {
      destroy();
      NS = &ID.getNamespace();
      allocate();
    }
    ArrayRef<uint8_t> Hash = ID.getHash();
    memcpy(isSmall() ? Storage.Hash : Storage.Mem, Hash.begin(), Hash.size());
    return *this;
  }
  UniqueID &moveImpl(UniqueID &ID) {
    NS = ID.NS;
    memcpy(&Storage, &ID.Storage, sizeof(Storage));
    ID.NS = nullptr;
    memset(&ID.Storage, 0, sizeof(Storage));
    return *this;
  }
  enum : size_t {
    InlineHashSize = 32,
  };
  bool isSmall() const { return !NS || NS->getHashSize() <= InlineHashSize; }

  const Namespace *NS = nullptr;
  union {
    uint8_t *Mem = nullptr;
    uint8_t Hash[InlineHashSize];
  } Storage;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_UNIQUEID_H
