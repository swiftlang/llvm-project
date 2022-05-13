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
