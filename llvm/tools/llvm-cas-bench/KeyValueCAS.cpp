//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// KeyValueCAS: an ObjectStore over a digest-to-blob store.
///
//===----------------------------------------------------------------------===//

#include "KeyValueCAS.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CAS/BuiltinObjectHasher.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casbench;

uint32_t KeyValueCAS::internIndex(const Digest &Key) const {
  auto [It, Inserted] = Indices.try_emplace(Key, uint32_t(Digests.size()));
  if (Inserted)
    Digests.push_back(Key);
  return It->second;
}

Expected<ObjectRef> KeyValueCAS::storeImpl(ArrayRef<uint8_t> ComputedHash,
                                           ArrayRef<ObjectRef> Refs,
                                           ArrayRef<char> Data) {
  assert(ComputedHash.size() == sizeof(Digest) && "unexpected digest size");
  Digest Key;
  llvm::copy(ComputedHash, Key.begin());

  uint32_t Index = internIndex(Key);

  SmallVectorImpl<uint8_t> &Blob = BlobBuf;
  Blob.resize_for_overwrite(wire::sizeFor(uint32_t(Refs.size()), Data.size()));
  uint8_t *P = Blob.data();
  support::endian::write32le(P, uint32_t(Refs.size()));
  P += wire::HeaderSize;
  for (ObjectRef Ref : Refs) {
    const Digest &ChildKey = digestOf(Ref);
    std::memcpy(P, ChildKey.data(), sizeof(Digest));
    P += sizeof(Digest);
  }
  if (!Data.empty())
    std::memcpy(P, Data.data(), Data.size());
  P[Data.size()] = 0; // BuiltinCAS object data is always null terminated.

  if (Error E = putBlob(Key, Blob))
    return std::move(E);

  return makeObjectRef(Index);
}

Expected<std::optional<ObjectHandle>> KeyValueCAS::loadIfExists(ObjectRef Ref) {
  uint32_t Index = indexOf(Ref);
  std::optional<ArrayRef<uint8_t>> Blob;
  if (Error Err = getBlob(Digests[Index]).moveInto(Blob))
    return std::move(Err);
  if (!Blob)
    return std::nullopt;
  if (!wire::isWellFormed(*Blob))
    return createCorruptObjectError(getID(Ref));
  LastBlob = *Blob;
  return makeObjectHandle(Index);
}

std::optional<ObjectRef> KeyValueCAS::getReference(const CASID &ID) const {
  ArrayRef<uint8_t> Hash = ID.getHash();
  if (Hash.size() != sizeof(Digest))
    return std::nullopt;
  Digest Key;
  llvm::copy(Hash, Key.begin());

  // getReference() must only name objects this store actually has, so this
  // cannot just intern the digest the way readRef() does.
  Expected<bool> Present = const_cast<KeyValueCAS *>(this)->containsBlob(Key);
  if (!Present) {
    consumeError(Present.takeError());
    return std::nullopt;
  }
  if (!*Present)
    return std::nullopt;
  return intern(Key);
}

Error KeyValueCAS::forEachRef(ObjectHandle,
                              function_ref<Error(ObjectRef)> Callback) const {
  uint32_t NumRefs = wire::getNumRefs(LastBlob);
  const Digest *Refs = wire::getRefs(LastBlob);
  for (uint32_t I = 0; I != NumRefs; ++I)
    if (Error E = Callback(intern(Refs[I])))
      return E;
  return Error::success();
}

Error KeyValueCAS::validate(bool CheckHash) const {
  // Only objects this store has interned can be reached; a digest-keyed blob
  // store has no enumeration order to walk.
  auto *Self = const_cast<KeyValueCAS *>(this);
  for (uint32_t I = 0, N = uint32_t(Digests.size()); I != N; ++I) {
    ObjectRef Ref = makeObjectRef(I);

    std::optional<ObjectHandle> Node;
    if (Error Err = Self->loadIfExists(Ref).moveInto(Node))
      return Err;
    if (!Node)
      continue; // Interned as somebody's child but never stored.

    ArrayRef<uint8_t> Blob = LastBlob;
    if (!CheckHash)
      continue;

    uint32_t NumRefs = wire::getNumRefs(Blob);
    const Digest *Refs = wire::getRefs(Blob);
    SmallVector<ArrayRef<uint8_t>, 16> RefHashes;
    RefHashes.reserve(NumRefs);
    for (uint32_t J = 0; J != NumRefs; ++J)
      RefHashes.push_back(Refs[J]);

    auto Recomputed = BuiltinObjectHasher<builtin::HasherT>::hashObject(
        RefHashes, wire::getData(Blob));
    if (Recomputed != Digests[I])
      return createCorruptObjectError(getID(Ref));
  }
  return Error::success();
}
