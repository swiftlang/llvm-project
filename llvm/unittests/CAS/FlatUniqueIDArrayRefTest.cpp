//===- FlatUniqueIDArrayRefTest.cpp ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/FlatUniqueIDArrayRef.h"
#include "TestingNamespaces.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

class FlatUniqueIDArrayRefTest : public ::testing::TestWithParam<size_t> {
protected:
  SmallVector<uint8_t> Zero;           /// Zero.
  SmallVector<uint8_t> Back1;          /// Last bit is set.
  SmallVector<uint8_t> Front1;         /// First bit is set.
  SmallVector<uint8_t> HashValueBack1; /// 64th bit is set.

  void SetUp() {
    Zero.resize(GetParam());
    Back1 = Front1 = HashValueBack1 = Zero;
    Back1.back() = 1U;
    Front1.front() = 128U;
    HashValueBack1[7] = 1U;
  }
};

TEST_P(FlatUniqueIDArrayRefTest, Empty) {
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  FlatUniqueIDArrayRef IDs(NS, None);
  EXPECT_TRUE(IDs.empty());
  EXPECT_EQ(0U, IDs.size());
  EXPECT_EQ(IDs.end(), IDs.begin());
  EXPECT_EQ(GetParam(), IDs.getHashSize());
  EXPECT_EQ(ArrayRef<uint8_t>(), IDs.getFlatHashes());
}

TEST_P(FlatUniqueIDArrayRefTest, One) {
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  for (ArrayRef<uint8_t> Hash : {Zero, Front1, Back1, HashValueBack1}) {
    UniqueIDRef ID(NS, Hash);
    FlatUniqueIDArrayRef IDs(NS, ID.getHash());
    EXPECT_FALSE(IDs.empty());
    EXPECT_EQ(1U, IDs.size());
    EXPECT_NE(IDs.end(), IDs.begin());
    EXPECT_EQ(IDs.end(), ++IDs.begin());
    EXPECT_EQ(ID, *IDs.begin());
    EXPECT_EQ(ID, IDs[0]);
    EXPECT_EQ(ID.getHash(), IDs.getFlatHashes());
  }
}

TEST_P(FlatUniqueIDArrayRefTest, Many) {
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  ArrayRef<uint8_t> Hashes[] = {Zero, Front1, Back1, HashValueBack1};
  SmallVector<uint8_t> FlatHashes;
  for (ArrayRef<uint8_t> Hash : Hashes)
    FlatHashes.append(Hash.begin(), Hash.end());

  FlatUniqueIDArrayRef IDs(NS, FlatHashes);
  EXPECT_FALSE(IDs.empty());
  EXPECT_EQ(4U, IDs.size());

  size_t H = 0;
  FlatUniqueIDArrayRef::iterator I = IDs.begin();
  for (; H != 4U && I != IDs.end(); ++H, ++I) {
    EXPECT_EQ(Hashes[H], I->getHash());
    EXPECT_EQ(UniqueIDRef(NS, Hashes[H]), *I);
  }
  EXPECT_EQ(4U, H);
  EXPECT_EQ(IDs.end(), I);
}

static size_t HashSizes[] = {8, 9, 16, 17, 20, 21, 32, 33};
INSTANTIATE_TEST_SUITE_P(FlatUniqueIDArrayRefTest, FlatUniqueIDArrayRefTest,
                         ::testing::ValuesIn(HashSizes));

} // end anonymous namespace
