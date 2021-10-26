//===- TargetInfoListTest.cpp - Unit tests for TargetInfoList -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Memory.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::data;

namespace {

TEST(TargetInfoListTest, Empty) {
  EXPECT_EQ(TargetInfoList().begin(), TargetInfoList().end());

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(++TargetInfoList().begin(), "past the end");
  EXPECT_DEATH(++TargetInfoList().end(), "past the end");
#endif
}

TEST(TargetInfoListTest, Single) {
  SmallString<128> Data;
  auto makeList = [&Data](TargetInfo &&F) {
    Data.clear();
    TargetInfoList::encode(makeArrayRef(F), Data);
    return TargetInfoList(Data);
  };

  {
    TargetInfoList TIL = makeList(TargetInfo{0, 0U});
    EXPECT_NE(TIL.begin(), TIL.end());
    EXPECT_EQ(0, TIL.begin()->Addend);
    EXPECT_EQ(0U, TIL.begin()->Index);
    EXPECT_EQ(1, std::distance(TIL.begin(), TIL.end()));
#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(++++TIL.begin(), "past the end");
    EXPECT_DEATH(++TIL.end(), "past the end");
#endif
  }

  {
    TargetInfoList TIL = makeList(TargetInfo{7777777, 0U});
    EXPECT_NE(TIL.begin(), TIL.end());
    EXPECT_EQ(7777777, TIL.begin()->Addend);
    EXPECT_EQ(0U, TIL.begin()->Index);
    EXPECT_EQ(1, std::distance(TIL.begin(), TIL.end()));
#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
    EXPECT_DEATH(++++TIL.begin(), "past the end");
    EXPECT_DEATH(++TIL.end(), "past the end");
#endif
  }
}

TEST(TargetInfoListTest, Multiple) {
  TargetInfo Input[] = {
      {0, 0U},  {1, 1U},       {0, 2U},         {1, 3U},        {0, 7U},
      {1, 13U}, {-20, 12U},    {-7777777, 11U}, {20, 5U},       {7777777, 10U},
      {20, 4U}, {7777777, 9U}, {-20, 6U},       {-7777777, 8U},
  };

  // Then check iteration.
  SmallString<128> Data;
  TargetInfoList::encode(Input, Data);
  TargetInfoList TIL(Data);
  SmallVector<TargetInfo, 16> Output(TIL.begin(), TIL.end());
  EXPECT_EQ(makeArrayRef(Input), makeArrayRef(Output));
}

} // end namespace
