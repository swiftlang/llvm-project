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

ArrayRef<uint8_t> ActionCache::resolveKey(const CacheKey &ActionKey,
                                          SmallVectorImpl<char> &Storage) const {
  raw_svector_stream(Storage) << ActionKey;
  return arrayRefFromStringRef(StringRef(Storage.begin(), Storage.size()));
}

Expected<ObjectRef> ActionCache::getOrCompute(
    const CacheKey &ActionKey,
    function_ref<Expected<ObjectRef> ()> Computation) {
  SmallVector<char, 32> KeyStorage;
  ArrayRef<uint8_t> ResolvedKey = resolveKey(ActionKey, KeyStorage);
  if (Optional<ObjectRef> Result = getImpl(ResolvedKey))
    return *Object;
  Optional<ObjectRef> Result;
  if (Error E = Computation().moveInto(Result))
    return std::move(E);
  if (Error E = putImpl(ResolvedKey, *Result))
    return std::move(E);
  return *Result;
}
