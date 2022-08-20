//===- ActionCacheTest.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CASTestConfig.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Testing/Support/Error.h"
#include "llvm/Testing/Support/SupportHelpers.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

TEST_P(CASTest, ActionCacheHit) {
  std::unique_ptr<CASDB> CAS = createCAS();
  std::unique_ptr<ActionCache> Cache = createActionCache(*CAS);

  Optional<ObjectProxy> ID;
  ASSERT_THAT_ERROR(CAS->createProxy(None, "1").moveInto(ID), Succeeded());
  ASSERT_THAT_ERROR(Cache->put(*ID, CAS->getReference(*ID)), Succeeded());
  Optional<ObjectRef> Result = Cache->get(*ID);
  ASSERT_TRUE(Result);
  ASSERT_EQ(*ID, *Result);
}

TEST_P(CASTest, ActionCacheMiss) {
  std::unique_ptr<CASDB> CAS = createCAS();
  std::unique_ptr<ActionCache> Cache = createActionCache(*CAS);

  Optional<ObjectProxy> ID1, ID2;
  ASSERT_THAT_ERROR(CAS->createProxy(None, "1").moveInto(ID1), Succeeded());
  ASSERT_THAT_ERROR(CAS->createProxy(None, "2").moveInto(ID2), Succeeded());
  ASSERT_THAT_ERROR(Cache->put(*ID1, CAS->getReference(*ID2)), Succeeded());
  // This is a cache miss for looking up a key doesn't exist.
  ASSERT_FALSE(Cache->get(*ID2));

  ASSERT_THAT_ERROR(Cache->put(*ID2, CAS->getReference(*ID1)), Succeeded());
  // Cache hit after adding the value.
  Optional<ObjectRef> Result = Cache->get(*ID2);
  ASSERT_TRUE(Result);
  ASSERT_EQ(*ID1, *Result);
}

TEST_P(CASTest, ActionCacheRewrite) {
  std::unique_ptr<CASDB> CAS = createCAS();
  std::unique_ptr<ActionCache> Cache = createActionCache(*CAS);

  Optional<ObjectProxy> ID1, ID2;
  ASSERT_THAT_ERROR(CAS->createProxy(None, "1").moveInto(ID1), Succeeded());
  ASSERT_THAT_ERROR(CAS->createProxy(None, "2").moveInto(ID2), Succeeded());
  ASSERT_THAT_ERROR(Cache->put(*ID1, CAS->getReference(*ID1)), Succeeded());
  // Writing to the same key with different value is error.
  ASSERT_THAT_ERROR(Cache->put(*ID1, CAS->getReference(*ID2)), Failed());
  // Writing the same value multiple times to the same key is fine.
  ASSERT_THAT_ERROR(Cache->put(*ID1, CAS->getReference(*ID1)), Succeeded());
}
