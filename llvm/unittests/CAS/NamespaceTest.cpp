//===- NamespaceTest.cpp --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/Namespace.h"
#include "TestingNamespaces.h"
#include "llvm/CAS/UniqueID.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

TEST(NamespaceTest, Accessors) {
  // Use testing namespaces to confirm accessors work.
  for (size_t HashSize : {8U, 32U}) {
    for (StringRef Suffix : {"", "suffix1", "suffix2"}) {
      const Namespace &NS = cas::testing::getNamespace(HashSize, Suffix);
      EXPECT_EQ(HashSize, NS.getHashSize());
      EXPECT_EQ(("llvm.test[" + Twine(HashSize) + "]" + Suffix).str(),
                NS.getName());
    }
  }
}

#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
TEST(NamespaceTest, MinimumHashSize) {
  EXPECT_DEATH(cas::testing::getNamespace(7),
               "Expected minimum hash size of 64 bits");
}
#endif

TEST(NamespaceTest, printIDReportFatalError) {
  const Namespace &BareNS = cas::testing::getNamespace(8);
  const Namespace &SuffixNS = cas::testing::getNamespace(8, "suffix");
  ASSERT_NE(&BareNS, &SuffixNS);
  std::array<uint8_t, 8> Zero;
  UniqueIDRef BareID(BareNS, Zero);
  UniqueIDRef SuffixID(SuffixNS, Zero);

  // Don't actually test printing. Just that it doesn't crash when the
  // namespace matches.
  raw_null_ostream OS;
  BareNS.printID(BareID, OS);
  SuffixNS.printID(SuffixID, OS);

#if defined(GTEST_HAS_DEATH_TEST)
  // Check for a guaranteed crash on a mismatched namespace.
  EXPECT_DEATH(SuffixNS.printID(BareID, OS), "cannot print ID from namespace");
  EXPECT_DEATH(BareNS.printID(SuffixID, OS), "cannot print ID from namespace");
#endif
}
