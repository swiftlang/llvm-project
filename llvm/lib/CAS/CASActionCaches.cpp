//===- CASActionCaches.cpp --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASActionCaches.h"
#include "llvm/CAS/HashMappedTrie.h"
#include "llvm/Support/BLAKE3.h"

using namespace llvm;
using namespace llvm::cas;

namespace {
class InMemoryActionCache final : public ActionCache {
public:
  Optional<ObjectRef> getImpl(ArrayRef<uint8_t> &ActionKey) const override;
  Error putImpl(ArrayRef<uint8_t> &ActionKey, const ObjectRef &Result) override;
  Error poisonImpl(ArrayRef<uint8_t> &ActionKey) override;

private:
  using HasherT = BLAKE3;
  using HashType = decltype(HasherT::hash(std::declval<ArrayRef<uint8_t> &>()));

  ArrayRef<uint8_t> resolveKey(const CacheKey &ActionKey,
                               SmallVectorImpl<char> &Storage) const override;

  using InMemoryCacheT =
      ThreadSafeHashMappedTrie<ObjectRef, sizeof(HashType)>;
  InMemoryCacheT Cache;
};
} // end namespace

ArrayRef<uint8_t> InMemoryActionCache::resolveKey(const CacheKey &ActionKey,
                                                  SmallVectorImpl<char> &Storage) const {
  {
    HashT Hash = Hasher::hash(ActionKey.toString());
    StringRef String = toStringRef(makeArrayRef(Hash));
    Storage.append(Hash.begin(), Hash.end());
  }
  return toStringRef(Storage);
}

Optional<ObjectRef> InMemoryActionCache::getImpl(ArrayRef<uint8_t> ResolvedKey) const {
  if (InMemoryActionCache::pointer Result = Cache.find(ResolvedKey))
    return Result->Data;
  return None;
}

Error InMemoryActionCache::putImpl(ArrayRef<uint8_t> ResolvedKey, const ObjectRef &Result) {
  InMemoryActionCache::pointer Inserted = Cache.insert(
          InMemoryActionCache::pointer{},
          InMemoryActionCache::value_type{ResolvedKey, Result},{                                               ResolvedKey))
  Cache.insert(InMemoryActionCache::pointer(), InMemoryActionCache
}

Error InMemoryActionCache::poisonImpl(ArrayRef<uint8_t> ResolvedKey) {
}

Error StringMapActionCacheAdaptor::putImpl(ArrayRef<uint8_t> ResolvedKey, const ObjectRef &Result) {
  return putID(ResolvedKey, getCAS().getObjectID(Result));
}

Optional<CASID> StringMapActionCacheAdaptor::putID(ArrayRef<uint8_t> ResolvedKey, const CASID &Result) {
  return getMap().put(toStringRef(ResolvedKey), ResultString);
}

Optional<ObjectRef> StringMapActionCacheAdaptor::getImpl(ArrayRef<uint8_t> ResolvedKey) const {
  Optional<CASID> ID = getID(ResolvedKey);
  if (!ID)
    return None;
  return getCAS().getReference(*ID);
}


ArrayRef<uint8_t> StringMapActionCacheAdaptor::resolveKey(const CacheKey &ActionKey,
                                                          SmallVectorImpl<char> &Storage) const {
  constexpr StringLiteral ActionPrefix = "action/";
  assert(Storage.empty());
  Storage.append(ActionPrefix.begin(), ActionPrefix.end());
  raw_svector_stream(Storage) << ActionKey;
  return arrayRefFromStringRef(StringRef(Storage.begin(), Storage.size()));
}

Optional<CASID> StringMapActionCacheAdaptor::getID(ArrayRef<uint8_t> ResolvedKey) const {
  SmallString<64> KeyString = "action/" + ActionKey.toString();
  SmallString<128> ResultString;
  {
    raw_ostream OS(ResultString);
    if (getMap().get(KeyString, OS))
      return None;
  }
  return expectedToOptional(getCAS().parseID(ResultString));
}

Error StringMapActionCacheAdaptorWithObjectStorage::put(ArrayRef<uint8_t> ResolvedKey,
                                                        const ObjectRef &Result) {
  SmallDenseMap<ObjectRef> Seen;
  SmallVector<ObjectRef> Stack;
  report_fatal_error("not implemented");
}

Optional<ObjectRef> StringMapActionCacheAdaptorWithObjectStorage::get(ArrayRef<uint8_t> ResolvedKey) const {
  report_fatal_error("not implemented");
}
