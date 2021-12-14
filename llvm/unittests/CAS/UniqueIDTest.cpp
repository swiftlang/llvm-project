//===- UniqueIDTest.cpp ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/UniqueID.h"
#include "TestingNamespaces.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

class UniqueIDTestBase : public testing::TestWithParam<size_t> {
protected:
  const Namespace *NS = nullptr;
  void SetUp() { NS = testing::getNamespace(GetParam()); }
  void TearDown() { NS = nullptr; }
};
class UniqueIDRefTest : UniqueIDTestBase {};
class UniqueIDTest : UniqueIDTestBase {};

TEST_P(UniqueIDRefTest, construct) {
  SmallVector<uint8_t> ZeroHash(NS->getHashSize(), 0);
  UniqueIDRef ID(*NS, ZeroHash);
  EXPECT_EQ(ZeroHash, ID.getHash());
}

TEST_P(UniqueIDTest, construct) {
  SmallVector<uint8_t> ZeroHash(NS->getHashSize(), 0);
  UniqueID ID = UniqueIDRef(*NS, ZeroHash);
  EXPECT_EQ(ZeroHash, ID.getHash());
}

INSTANTIATE_TEST_SUITE_P(UniqueIDRefTest, UniqueIDRefTest, ::testing::Values(8, 9, 20, 21, 32, 33));
INSTANTIATE_TEST_SUITE_P(UniqueIDTest, UniqueIDTest, ::testing::Values(8, 9, 20, 21, 32, 33));
