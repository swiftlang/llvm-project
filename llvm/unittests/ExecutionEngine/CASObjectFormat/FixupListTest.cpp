//===- FixupListTest.cpp - Unit tests for FixupList object file schema ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/CASObjectFormat/ObjectFileSchema.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Memory.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformat;

namespace {

TEST(FixupListTest, Empty) {
  EXPECT_EQ(FixupList().begin(), FixupList().end());

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(++FixupList().begin(), "past the end");
  EXPECT_DEATH(++FixupList().end(), "past the end");
#endif
}

TEST(FixupListTest, Single) {
  SmallString<128> Data;
  auto makeList = [&Data](Fixup &&F) {
    Data.clear();
    FixupList::encode(makeArrayRef(F), Data);
    return FixupList(Data);
  };

  {
    FixupList FL = makeList(Fixup{jitlink::Edge::Kind(0), 0U});
    EXPECT_NE(FL.begin(), FL.end());
    EXPECT_EQ(jitlink::Edge::Kind(0), FL.begin()->Kind);
    EXPECT_EQ(0U, FL.begin()->Offset);
    EXPECT_EQ(1, std::distance(FL.begin(), FL.end()));
#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(++++FL.begin(), "past the end");
    EXPECT_DEATH(++FL.end(), "past the end");
#endif
  }

  {
    FixupList FL = makeList(Fixup{jitlink::Edge::Kind(127), 12345678U});
    EXPECT_NE(FL.begin(), FL.end());
    EXPECT_EQ(jitlink::Edge::Kind(127), FL.begin()->Kind);
    EXPECT_EQ(12345678U, FL.begin()->Offset);
    EXPECT_EQ(1, std::distance(FL.begin(), FL.end()));
#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(++++FL.begin(), "past the end");
    EXPECT_DEATH(++FL.end(), "past the end");
#endif
  }
}

TEST(FixupListTest, Multiple) {
  Fixup Input[] = {
      {jitlink::Edge::Kind(0), 0U},      {jitlink::Edge::Kind(1), 0U},
      {jitlink::Edge::Kind(0), 1U},      {jitlink::Edge::Kind(1), 5U},
      {jitlink::Edge::Kind(0), 876543U}, {jitlink::Edge::Kind(1), 555555U},
      {jitlink::Edge::Kind(20), 0U},     {jitlink::Edge::Kind(127), 0U},
      {jitlink::Edge::Kind(20), 17U},    {jitlink::Edge::Kind(127), 12345678U},
  };

  SmallString<128> Data;
  FixupList::encode(Input, Data);
  FixupList FL(Data);
  SmallVector<Fixup, 16> Output(FL.begin(), FL.end());
  EXPECT_EQ(makeArrayRef(Input), makeArrayRef(Output));
}

} // end namespace
