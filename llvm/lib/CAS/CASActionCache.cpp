//===- CASActionCache.cpp ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASActionCache.h"

using namespace llvm;
using namespace llvm::cas;

void CASActionCache::anchor() {}

virtual Expected<ObjectRef> CASActionCache::getOrCompute(
    const CacheKey &ActionKey,
    function_ref<Expected<ObjectRef> ()> Computation) {
  if (Optional<ObjectRef> Result = get(ActionKey))
    return *Object;
  Optional<ObjectRef> Result;
  if (Error E = Computation().moveInto(Result))
    return std::move(E);
  if (Error E = put(ActionKey, *Result))
    return std::move(E);
  return *Result;
}

Error StringMapActionCacheAdaptor::put(const CacheKey &ActionKey, const ObjectRef &Result) {
  return putID(ActionKey, getCAS().getObjectID(Result));
}

Optional<CASID> StringMapActionCacheAdaptor::putID(const CacheKey &ActionKey, const CASID &Result) {
  SmallString<64> KeyString = "action/" + ActionKey.toString();
  return getMap().put(KeyString, ResultString);
}

Optional<ObjectRef> StringMapActionCacheAdaptor::get(const CacheKey &ActionKey) const {
  Optional<CASID> ID = getID(ActionKey);
  if (!ID)
    return None;
  return getCAS().getReference(*ID);
}

Optional<CASID> StringMapActionCacheAdaptor::getID(const CacheKey &ActionKey) const {
  SmallString<64> KeyString = "action/" + ActionKey.toString();
  SmallString<128> ResultString;
  {
    raw_ostream OS(ResultString);
    if (getMap().get(KeyString, OS))
      return None;
  }
  return expectedToOptional(getCAS().parseID(ResultString));
}

Error StringMapActionCacheAdaptorWithObjectStorage::put(const CacheKey &ActionKey,
                                                        const ObjectRef &Result) {
  SmallDenseMap<ObjectRef> Seen;
  SmallVector<ObjectRef> Stack;
}

Optional<ObjectRef> StringMapActionCacheAdaptorWithObjectStorage::get(const CacheKey &ActionKey) const {
}
