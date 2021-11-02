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

/// Reference to a unique ID. Does not own storage for its hash; like \a
/// StringRef, need to be aware of lifetime.
///
/// Must always be fully initialized. Use \a Optional if necessary for delayed
/// construction.
///
/// Associated with a specific \a Namespace.
class UniqueIDRef {
  // Fixed set of expected sizes for hashes. Allows comparing hashes without
  // chasing the Namespace pointer. Other sizes are still okay, but slower.
  enum HashSize {
    NotEmbedded,
    Embedded20B, // 160 bits
    Embedded32B, // 256 bits
    // Can fill common values can go here.
  };
  enum : size_t {
    // Expect hashes to be at least 16B.
    MinHashSize = 16,
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
  void print(raw_ostream &OS) const {
    ID.getNamespace().printID(ID, OS);
  }
  friend raw_ostream &operator<<(raw_ostream &OS, UniqueIDRef ID) {
    ID.print(OS);
    return OS;
  }

  ArrayRef<uint8_t> getHash() const {
    return makeArrayRef(Hash, Hash + getHashSize());
  }

  friend bool operator==(UniqueIDRef LHS, UniqueIDRef RHS) {
    if (LHS.NamespaceAndHashSize != RHS.NamespaceAndHashSize)
      return false;
    if (LHS.Hash == RHS.Hash)
      return true;
    if (makeArrayRef(LHS.Hash, MinHashSize) !=
        makeArrayRef(RHS.Hash, MinHashSize))
      return false;
    return LHS.getHash().drop_front(MinHashSize) ==
           RHS.getHash().drop_front(MinHashSize);
  }
  friend bool operator!=(UniqueIDRef LHS, UniqueIDRef RHS) {
    return !(LHS == RHS);
  }

  uint64_t getHashValue() const {
    uint64_t Value = 0;
    for (size_t I = 0, E = sizeof(uint64_t); I != E; ++I)
      Value |= Hash[I] << (I * 8);
    return Value;
  }

  UniqueIDRef() = delete;
  UniqueIDRef(UniqueIDRef &&) = default;
  UniqueIDRef(const UniqueIDRef &) = default;
  UniqueIDRef &operator=(UniqueIDRef &&) = default;
  UniqueIDRef &operator=(const UniqueIDRef &) = default;

  UniqueIDRef(const Namespace &NS, ArrayRef<uint8_t> Hash) : NS(&NS), Hash(Hash.begin()) {
    assert(NS.getHashSize() >= MinHashSize && "Expected hash not to be small");
    assert(NS.getHashSize() == Hash.size() && "Expected valid hash for namespace");
    if (NS.getHashSize() == 20)
      NS.setInt(Embedded20B);
    else if (NS.getHashSize() == 32)
      NS.setInt(Embedded32B);
  }

  struct DenseMapTombstoneTag {};
  struct DenseMapEmptyTag {};

  explicit UniqueIDRef(DenseMapTombstoneTag) :
      : Hash(DenseMapInfo<ArrayRef<uint8_t>>::getTombstoneKey());
  explicit UniqueIDRef(DenseMapEmptyTag) :
      : Hash(DenseMapInfo<ArrayRef<uint8_t>>::getEmptyKey());

private:
  PointerIntPair<const Namespace *, 3> NamespaceAndHashSize;
  const uint8_t *Hash = nullptr;
};

hash_code hash_value(UniqueIDRef ID) { return hash_code(ID.getHashValue()); }

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
    return DenseMapInfo<ArrayRef<uint8_t>>::isEqual(LHS.getHash(),
                                                    RHS.getHash());
  }
};

namespace cas {

/// A unique ID that owns its hash storage. Uses small string optimization for
/// hashes within 32B.
///
/// May be uninitialized, in which case \a getNamespace() is \a nullptr.
class UniqueID {
public:
  /// Check if this ID is valid.
  bool isValid() const { return NS; }
  explicit operator bool() const { return isValid(); }

  const Namespace &getNamespace() const {
    assert(NS && "Invalid namespace");
    return *NS;
  }

  /// Get the raw bytes of the hash.
  ArrayRef<uint8_t> getHash() const {
    assert(isValid() && "Cannot get hash from invalid UniqueID");
    const uint8_t *HashPtr = isSmall() ? Storage.Hash : Storage.Mem;
    return makeArrayRef(HashPtr, HashPtr + getNamespace().getHashSize());
  }

  /// Get a reference into this storage.
  operator UniqueIDRef() const {
    assert(isValid() && "Cannot convert invalid UniqueID to reference");
    return UniqueIDRef(getNamespace(), getHash());
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
    memcpy(isSmall() ? Storage.Hash : Storage.Mem, Hash, Hash.size());
    return *this;
  }
  UniqueID &moveImpl(UniqueID &ID) {
    NS = ID.NS;
    memcpy(&Storage, &ID.Storage, sizeof(Storage));
    ID.NS = nullptr;
    memset(&ID.Storage, sizeof(Storage), 0);
    return *this;
  }
  constexpr inline size_t InlineHashSize = 32;
  bool isSmall() const {
    return !NS || NS->getNamespace().getHashSize() <= InlineHashSize;
  }

  const Namespace *NS = nullptr;
  union {
    const uint8_t *Mem = nullptr;
    uint8_t Hash[InlineHashSize];
  } Storage;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_UNIQUEID_H
