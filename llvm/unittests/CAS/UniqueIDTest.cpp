//===- UniqueIDTest.cpp ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/UniqueID.h"
#include "TestingNamespaces.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

class UniqueIDTestBase : public ::testing::TestWithParam<size_t> {
protected:
  SmallVector<uint8_t> Zero;           /// Zero.
  SmallVector<uint8_t> Zero2;          /// Zero with different storage.
  SmallVector<uint8_t> Back1;          /// Last bit is set.
  SmallVector<uint8_t> Front1;         /// First bit is set.
  SmallVector<uint8_t> HashValueBack1; /// 64th bit is set.
  Optional<SmallVector<uint8_t>> PastHashValueFront1; /// 65th bit is set.

  ArrayRef<uint8_t> RefZero;
  ArrayRef<uint8_t> RefZero2;
  ArrayRef<uint8_t> RefBack1;
  ArrayRef<uint8_t> RefFront1;
  ArrayRef<uint8_t> RefHashValueBack1;
  Optional<ArrayRef<uint8_t>> RefPastHashValueFront1;

  void SetUp() {
    Zero.resize(GetParam());
    Zero2 = Back1 = Front1 = HashValueBack1 = Zero;
    Back1.back() = 1U;
    Front1.front() = 128U;
    HashValueBack1[7] = 1U;

    RefZero = makeArrayRef(Zero);
    RefZero2 = makeArrayRef(Zero2);
    RefBack1 = makeArrayRef(Back1);
    RefFront1 = makeArrayRef(Front1);
    RefHashValueBack1 = makeArrayRef(HashValueBack1);

    if (Zero.size() <= 8)
      return;
    PastHashValueFront1 = Zero;
    (*PastHashValueFront1)[8] = 128U;
    RefPastHashValueFront1 = makeArrayRef(*PastHashValueFront1);
  }
};

class UniqueIDRefTest : public UniqueIDTestBase {};

TEST_P(UniqueIDRefTest, construct) {
  static_assert(!std::is_default_constructible<UniqueIDRef>::value,
                "UniqueIDRef should guarantee validity");
  static_assert(
      !std::is_convertible<UniqueIDRef, bool>::value,
      "UniqueIDRef is always valid; should not implicitly convert to bool");
  static_assert(
      !std::is_constructible<bool, UniqueIDRef>::value,
      "UniqueIDRef is always valid; should not explicitly convert to bool");

  const Namespace &NS = cas::testing::getNamespace(GetParam());
  for (ArrayRef<uint8_t> Hash : {Zero, Back1, Front1, HashValueBack1}) {
    UniqueIDRef ID(NS, Hash);
    EXPECT_EQ(&NS, &ID.getNamespace());
    EXPECT_EQ(Hash, ID.getHash());
  }
}

TEST_P(UniqueIDRefTest, equals) {
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  UniqueIDRef ZeroID(NS, Zero);
  EXPECT_EQ(RefZero, ZeroID.getHash());
  for (ArrayRef<uint8_t> Hash : {Zero, Zero2})
    EXPECT_EQ(ZeroID, UniqueIDRef(NS, Hash));
  for (ArrayRef<uint8_t> Hash : {Back1, Front1, HashValueBack1})
    EXPECT_NE(ZeroID, UniqueIDRef(NS, Hash));

  UniqueIDRef Back1ID(NS, Back1);
  UniqueIDRef Front1ID(NS, Front1);
  EXPECT_EQ(Front1ID, Front1ID);
  EXPECT_NE(Front1ID, Back1ID);
  EXPECT_EQ(Back1ID, Back1ID);

  const Namespace &OtherNS = cas::testing::getNamespace(GetParam(), "other");
  for (UniqueIDRef ID : {ZeroID, Back1ID, Front1ID}) {
    for (ArrayRef<uint8_t> Hash :
         {ZeroID.getHash(), Back1ID.getHash(), Front1ID.getHash()}) {
      EXPECT_NE(ID, UniqueIDRef(OtherNS, Hash));
    }
  }
}

TEST_P(UniqueIDRefTest, Storage) {
  // UniqueIDRef should not own storage.
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  SmallVector<uint8_t> Hash;
  Hash = Back1;
  UniqueIDRef HashID1(NS, Hash);
  EXPECT_EQ(makeArrayRef(Hash), HashID1.getHash());

  Hash = Front1;
  UniqueIDRef HashID2(NS, Hash);
  EXPECT_EQ(makeArrayRef(Hash), HashID1.getHash());
  EXPECT_EQ(makeArrayRef(Hash), HashID2.getHash());

  Hash = Zero;
  EXPECT_EQ(RefZero, HashID1.getHash());
  EXPECT_EQ(RefZero, HashID2.getHash());
}

TEST_P(UniqueIDRefTest, getHashValue) {
  // UniqueIDRef::getHashValue() should steal the first 8 bytes as a hash
  // value.
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  UniqueIDRef ZeroID(NS, Zero);
  UniqueIDRef Back1ID(NS, Back1);
  UniqueIDRef Front1ID(NS, Front1);
  UniqueIDRef HashValueBack1ID(NS, HashValueBack1);

  EXPECT_EQ(ZeroID.getHashValue(), ZeroID.getHashValue());
  EXPECT_NE(ZeroID.getHashValue(), Front1ID.getHashValue());
  EXPECT_NE(ZeroID.getHashValue(), HashValueBack1ID.getHashValue());
  EXPECT_NE(Front1ID.getHashValue(), HashValueBack1ID.getHashValue());

  // Return early if the hash is only 64 bits long.
  if (!PastHashValueFront1)
    return;

  // Expect changes past the first 64 bits to be missing.
  UniqueIDRef PastHashValueFront1ID(NS, *PastHashValueFront1);
  EXPECT_EQ(ZeroID.getHashValue(), Back1ID.getHashValue());
  EXPECT_EQ(ZeroID.getHashValue(), PastHashValueFront1ID.getHashValue());
  EXPECT_EQ(Back1ID.getHashValue(), PastHashValueFront1ID.getHashValue());
}

class UniqueIDTest : public UniqueIDTestBase {};

TEST_P(UniqueIDTest, construct) {
  UniqueID ID;
  EXPECT_FALSE(ID);
#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
  EXPECT_DEATH((void)ID.getNamespace(), "Cannot get namespace from invalid ID");
  EXPECT_DEATH((void)ID.getHash(), "Cannot get hash from invalid ID");
#endif

  const Namespace &NS = cas::testing::getNamespace(GetParam());
  for (ArrayRef<uint8_t> Hash : {Zero, Back1, Front1, HashValueBack1}) {
    ID = UniqueID(NS, Hash);
    EXPECT_TRUE(ID);
    EXPECT_EQ(Hash, ID.getHash());
    EXPECT_EQ(&NS, &ID.getNamespace());

    UniqueID CopyToID = ID;
    EXPECT_TRUE(CopyToID);
    EXPECT_EQ(Hash, CopyToID.getHash());
    EXPECT_EQ(&NS, &CopyToID.getNamespace());
    EXPECT_TRUE(ID);
    EXPECT_EQ(Hash, ID.getHash());
    EXPECT_EQ(&NS, &ID.getNamespace());

    UniqueID MoveToID = std::move(ID);
    EXPECT_TRUE(MoveToID);
    EXPECT_EQ(Hash, MoveToID.getHash());
    EXPECT_EQ(&NS, &MoveToID.getNamespace());
    EXPECT_FALSE(ID);
  }
}

TEST_P(UniqueIDTest, equals) {
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  UniqueID ZeroID(NS, Zero);
  EXPECT_EQ(RefZero, ZeroID.getHash());
  for (ArrayRef<uint8_t> Hash : {Zero, Zero2})
    EXPECT_EQ(ZeroID, UniqueIDRef(NS, Hash));
  for (ArrayRef<uint8_t> Hash : {Back1, Front1, HashValueBack1})
    EXPECT_NE(ZeroID, UniqueIDRef(NS, Hash));
  if (PastHashValueFront1)
    EXPECT_NE(ZeroID, UniqueIDRef(NS, *PastHashValueFront1));

  UniqueID Back1ID(NS, Back1);
  UniqueID Front1ID(NS, Front1);
  EXPECT_EQ(Front1ID, Front1ID);
  EXPECT_NE(Front1ID, Back1ID);
  EXPECT_EQ(Back1ID, Back1ID);

  const Namespace &OtherNS = cas::testing::getNamespace(GetParam(), "other");
  for (const UniqueID &ID : {ZeroID, Back1ID, Front1ID}) {
    for (ArrayRef<uint8_t> Hash :
         {ZeroID.getHash(), Back1ID.getHash(), Front1ID.getHash()}) {
      EXPECT_NE(ID, UniqueIDRef(OtherNS, Hash));
    }
  }
}

TEST_P(UniqueIDTest, Storage) {
  // UniqueID should have its own storage.
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  SmallVector<uint8_t> Hash;
  Hash = Back1;
  UniqueID HashID1(NS, Hash);
  EXPECT_EQ(makeArrayRef(Hash), HashID1.getHash());
  EXPECT_EQ(RefBack1, HashID1.getHash());

  Hash = Front1;
  UniqueID HashID2(NS, Hash);
  EXPECT_EQ(makeArrayRef(Hash), HashID2.getHash());
  EXPECT_EQ(RefFront1, HashID2.getHash());

  Hash = Zero;
  EXPECT_EQ(RefBack1, HashID1.getHash());
  EXPECT_EQ(RefFront1, HashID2.getHash());
  EXPECT_NE(HashID1, HashID2);
}

TEST_P(UniqueIDTest, print) {
  const Namespace &NS = cas::testing::getNamespace(GetParam());
  for (ArrayRef<uint8_t> Hash : {Zero, Back1, Front1, HashValueBack1}) {
    UniqueID ID(NS, Hash);
    SmallString<128> Printed;
    raw_svector_ostream(Printed) << ID;

    UniqueID Ref = ID;
    SmallString<128> PrintedRef;
    raw_svector_ostream(PrintedRef) << Ref;
    EXPECT_EQ(Printed, PrintedRef);

    UniqueID Parsed;
    EXPECT_THAT_ERROR(NS.parseID(Printed, Parsed), Succeeded());
    EXPECT_EQ(Parsed, ID);
  }
}

static size_t HashSizes[] = {8, 9, 16, 17, 20, 21, 32, 33};
INSTANTIATE_TEST_SUITE_P(UniqueIDRefTest, UniqueIDRefTest,
                         ::testing::ValuesIn(HashSizes));
INSTANTIATE_TEST_SUITE_P(UniqueIDTest, UniqueIDTest,
                         ::testing::ValuesIn(HashSizes));

} // end anonymous namespace
