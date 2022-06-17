//===- llvm/CAS/CASCacheKey.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASCACHEKEY_H
#define LLVM_CAS_CASCACHEKEY_H

#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/CASReference.h"
#include "llvm/CAS/TreeEntry.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h" // FIXME: Split out sys::fs::file_status.
#include <cstddef>

namespace llvm {
namespace cas {

/// A key for caching an operation. Use \a CacheKeyBuilder to create one.
///
/// The key optionally embeds an internal hash, along with a hash schema
/// identifier to catch misuses.
class CacheKey {
public:
  StringRef getKey() const { return Key; }

  /// Get an already computed hash of the key for \p Schema, if any.
  Optional<ArrayRef<uint8_t>> getHashedKey(StringRef Schema) const {
    if (Hashed && Hashed->Schema == Schema)
      return arrayRefFromStringRef(Hashed->KeyHash);
    return None;
  }

  /// Hash and return the key for \p Schema, or use the stored one if it
  /// already exists.
  ArrayRef<uint8_t> hashKey(
      StringRef Schema,
      function_ref<void (StringRef Key, SmallVectorImpl<uint8_t> &Hash)> Hasher) {
    if (Optional<ArrayRef<uint8_t>> HashedKey = getHashedKey(Schema))
      return HashedKey;

    SmallVector<uint8_t> Hash;
    Hasher(Key, Hasher);
    HashedKey.emplace(HashedKeyT{toStringRef(Hash).str(), Schema.str()});
    return arrayRefFromStringRef(HashedKey->Hash);
  }

  explicit CacheKey(const Twine &Key) : Key(Key.str()) {}

private:
  struct HashedKeyT {
    std::string Hash;
    std::string Schema;
  };

  std::string Key;
  Optional<HashedKeyT> HashedKey;
};

class CASDB;

/// A builder for a cache key.
class CacheKeyBuilder {
public:
  CacheKeyBuilder &add(const Twine &Twine, Optional<StringRef> Name = None) {
    if (Name)
      Elements.push_back(Twine(*Name));
    Elements.push_back(Twine);
  }
  CacheKeyBuilder &add(ObjectRef Object, Optional<StringRef> Name = None) {
    if (Name)
      Elements.push_back(Twine(*Name));
    Elements.push_back(Object);
  }
  CacheKey build(CASDB &CAS);

  explicit CacheKeyBuilder(Optional<StringRef> KeyName = None) {
    if (KeyName)
      Elements.push_back(Twine(*KeyName));
  }

private:
  struct CacheKeyElement {
    Optional<std::string> Bytes;
    Optional<ObjectRef> Object;
    CacheKeyElement(const Twine &String) : Bytes(String.str()) {}
    CacheKeyElement(const ObjectRef &Object) : Object(Object) {}
  };
  SmallVector<CacheKeyElement> Elements;
  BumpPtrAllocator Alloc;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_CASCACHEKEY_H
