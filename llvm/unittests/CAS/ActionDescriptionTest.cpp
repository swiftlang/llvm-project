//===- ActionDescriptionTest.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/ActionDescription.h"
#include "TestingNamespaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

static uint64_t Values[] = {0U, 1U, 2U, 3U};
struct VectorOfIDs {
  SmallVector<UniqueIDRef> V;
  VectorOfIDs() {
    constexpr size_t NumHashBytes = sizeof(uint64_t);
    const Namespace &NS = cas::testing::getNamespace(NumHashBytes);
    for (uint64_t &Value : Values)
      V.emplace_back(NS, makeArrayRef(reinterpret_cast<const uint8_t *>(&Value),
                                      NumHashBytes));
  }
};

template <class T> static std::string toString(const T &V) {
  SmallString<128> Printed;
  raw_svector_ostream(Printed) << V;
  return Printed.str().str();
}

TEST(ActionDescriptionTest, Identifier) {
  for (StringRef ActionID : {"some.identifier", "another thing"}) {
    ActionDescription Action(ActionID);
    EXPECT_EQ(ActionID, Action.getIdentifier());
    EXPECT_EQ(ArrayRef<UniqueIDRef>(), Action.getIDs());
    EXPECT_EQ("", Action.getExtraData());
    EXPECT_EQ(ActionID, toString(Action));
  }
}

TEST(ActionDescriptionTest, WithIDs) {
  VectorOfIDs IDs;
  constexpr StringLiteral ActionID = "some.identifier";
  for (UniqueIDRef ID : IDs.V) {
    ActionDescription Action(ActionID, ID);
    EXPECT_EQ(ActionID, Action.getIdentifier());
    EXPECT_EQ(makeArrayRef(ID), Action.getIDs());
    EXPECT_EQ("", Action.getExtraData());
    EXPECT_EQ((ActionID + "\n- id: " + toString(ID)).str(), toString(Action));
  }

  ArrayRef<UniqueIDRef> TwoIDs = makeArrayRef(IDs.V).take_front(3).drop_front();
  ActionDescription Action(ActionID, TwoIDs);
  EXPECT_EQ(ActionID, Action.getIdentifier());
  EXPECT_EQ(TwoIDs, Action.getIDs());
  EXPECT_EQ("", Action.getExtraData());
  EXPECT_EQ((ActionID + "\n- id: " + toString(TwoIDs[0]) +
             "\n- id: " + toString(TwoIDs[1]))
                .str(),
            toString(Action));
}

TEST(ActionDescriptionTest, ExtraData) {
  constexpr StringLiteral ActionID = "some.identifier";
  for (StringRef ExtraData : {"12345", "123"}) {
    ActionDescription Action(ActionID, None, ExtraData);
    EXPECT_EQ(ActionID, Action.getIdentifier());
    EXPECT_EQ(ArrayRef<UniqueIDRef>(), Action.getIDs());
    EXPECT_EQ(ExtraData, Action.getExtraData());
    EXPECT_EQ(
        (ActionID + "\n- extra-data: (size=" + Twine(ExtraData.size()) + ")")
            .str(),
        toString(Action));
  }
}

TEST(ActionDescriptionTest, print) {
  // Configuration for print.
  VectorOfIDs IDs;
  constexpr StringLiteral ActionID = "some.identifier";
  constexpr uint8_t ExtraDataValues[] = {
      0U,  1U,  5U,  127U, 255U, 10U, 11U, 12U, 13U,  14U,  15U,
      6U,  7U,  8U,  16U,  32U,  64U, 1U,  5U,  127U, 255U, 10U,
      11U, 12U, 13U, 14U,  15U,  6U,  7U,  8U,  16U,  32U,  128U,
  };
  StringRef ExtraData(reinterpret_cast<const char *>(ExtraDataValues),
                      makeArrayRef(ExtraDataValues).size());
  ArrayRef<UniqueIDRef> TwoIDs = makeArrayRef(IDs.V).take_front(3).drop_front();

  ActionDescription Action(ActionID, TwoIDs, ExtraData);
  EXPECT_EQ(ActionID, Action.getIdentifier());
  EXPECT_EQ(TwoIDs, Action.getIDs());
  EXPECT_EQ(ExtraData, Action.getExtraData());
  std::string FullIDs =
      "\n- id: " + toString(TwoIDs[0]) + "\n- id: " + toString(TwoIDs[1]);
  std::string CountIDs = "\n- ids: (count=2)";
  std::string FullExtraData = std::string("\n- extra-data:\n") +
                              "0001057f ff0a0b0c 0d0e0f06 07081020 " +
                              "4001057f ff0a0b0c 0d0e0f06 07081020\n" + "80";
  std::string CountExtraData =
      ("\n- extra-data: (size=" + Twine(ExtraData.size()) + ")").str();

  struct {
    bool PrintIDs;
    bool PrintExtraData;
    std::string S;
  } Tests[] = {{false, false, (ActionID + CountIDs + CountExtraData).str()},
               {false, true, (ActionID + CountIDs + FullExtraData).str()},
               {true, true, (ActionID + FullIDs + FullExtraData).str()},
               {true, false, (ActionID + FullIDs + CountExtraData).str()}};
  for (auto T : Tests) {
    SmallString<128> Printed;
    raw_svector_ostream OS(Printed);
    Action.print(OS, T.PrintIDs, T.PrintExtraData);
    EXPECT_EQ(T.S, Printed);
  }
}

} // end anonymous namespace
