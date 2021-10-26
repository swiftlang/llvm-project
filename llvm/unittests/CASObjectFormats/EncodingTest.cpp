//===- EncodingTest.cpp - Unit tests for CASObjectFormats encodings -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::encoding;

template <class T> static Expected<T> makeRoundTripVBR8(T V) {
  SmallVector<char> Data;
  writeVBR8(V, Data);

  T RoundTripV;
  StringRef Remaining(Data.begin(), Data.size());
  if (Error E = consumeVBR8(Remaining, RoundTripV))
    return std::move(E);
  if (!Remaining.empty())
    return createStringError(inconvertibleErrorCode(),
                             "data left over; got: " + Twine(RoundTripV));
  return RoundTripV;
}

template <class T> static size_t getSizeVBR8(T V) {
  SmallVector<char> Data;
  writeVBR8(V, Data);
  return Data.size();
}

namespace {

TEST(EncodingTest, rotateSign) {
  // Check int.
  EXPECT_EQ(0U, rotateSign(0));
  EXPECT_EQ(2U, rotateSign(1));
  EXPECT_EQ(4U, rotateSign(2));
  EXPECT_EQ(6U, rotateSign(3));
  EXPECT_EQ(UINT_MAX - 1U, rotateSign(INT_MAX - 0));
  EXPECT_EQ(UINT_MAX - 3U, rotateSign(INT_MAX - 1));
  EXPECT_EQ(UINT_MAX - 5U, rotateSign(INT_MAX - 2));
  EXPECT_EQ(UINT_MAX - 7U, rotateSign(INT_MAX - 3));

  EXPECT_EQ(1U, rotateSign(-1));
  EXPECT_EQ(3U, rotateSign(-2));
  EXPECT_EQ(5U, rotateSign(-3));
  EXPECT_EQ(7U, rotateSign(-4));
  EXPECT_EQ(UINT_MAX - 0U, rotateSign(INT_MIN + 0));
  EXPECT_EQ(UINT_MAX - 2U, rotateSign(INT_MIN + 1));
  EXPECT_EQ(UINT_MAX - 4U, rotateSign(INT_MIN + 2));
  EXPECT_EQ(UINT_MAX - 6U, rotateSign(INT_MIN + 3));

  // Check long long.
  EXPECT_EQ(0ULL, rotateSign(0LL));
  EXPECT_EQ(2ULL, rotateSign(1LL));
  EXPECT_EQ(4ULL, rotateSign(2LL));
  EXPECT_EQ(6ULL, rotateSign(3LL));
  EXPECT_EQ(ULLONG_MAX - 1ULL, rotateSign(LLONG_MAX - 0LL));
  EXPECT_EQ(ULLONG_MAX - 3ULL, rotateSign(LLONG_MAX - 1LL));
  EXPECT_EQ(ULLONG_MAX - 5ULL, rotateSign(LLONG_MAX - 2LL));
  EXPECT_EQ(ULLONG_MAX - 7ULL, rotateSign(LLONG_MAX - 3LL));

  EXPECT_EQ(1ULL, rotateSign(-1LL));
  EXPECT_EQ(3ULL, rotateSign(-2LL));
  EXPECT_EQ(5ULL, rotateSign(-3LL));
  EXPECT_EQ(7ULL, rotateSign(-4LL));
  EXPECT_EQ(ULLONG_MAX - 0ULL, rotateSign(LLONG_MIN + 0LL));
  EXPECT_EQ(ULLONG_MAX - 2ULL, rotateSign(LLONG_MIN + 1LL));
  EXPECT_EQ(ULLONG_MAX - 4ULL, rotateSign(LLONG_MIN + 2LL));
  EXPECT_EQ(ULLONG_MAX - 6ULL, rotateSign(LLONG_MIN + 3LL));

  // Check char.
  EXPECT_EQ(0U, rotateSign(char(0)));
  EXPECT_EQ(2U, rotateSign(char(1)));
  EXPECT_EQ(4U, rotateSign(char(2)));
  EXPECT_EQ(6U, rotateSign(char(3)));
  EXPECT_EQ(UCHAR_MAX - 1U, rotateSign(char(CHAR_MAX - 0)));
  EXPECT_EQ(UCHAR_MAX - 3U, rotateSign(char(CHAR_MAX - 1)));
  EXPECT_EQ(UCHAR_MAX - 5U, rotateSign(char(CHAR_MAX - 2)));
  EXPECT_EQ(UCHAR_MAX - 7U, rotateSign(char(CHAR_MAX - 3)));

  EXPECT_EQ(1U, rotateSign(char(-1LL)));
  EXPECT_EQ(3U, rotateSign(char(-2LL)));
  EXPECT_EQ(5U, rotateSign(char(-3LL)));
  EXPECT_EQ(7U, rotateSign(char(-4LL)));
  EXPECT_EQ(UCHAR_MAX - 0U, rotateSign(char(CHAR_MIN + 0LL)));
  EXPECT_EQ(UCHAR_MAX - 2U, rotateSign(char(CHAR_MIN + 1LL)));
  EXPECT_EQ(UCHAR_MAX - 4U, rotateSign(char(CHAR_MIN + 2LL)));
  EXPECT_EQ(UCHAR_MAX - 6U, rotateSign(char(CHAR_MIN + 3LL)));
}

TEST(EncodingTest, unrotateSign) {
  // Check int.
  EXPECT_EQ(0, unrotateSign(0U));
  EXPECT_EQ(1, unrotateSign(2U));
  EXPECT_EQ(2, unrotateSign(4U));
  EXPECT_EQ(3, unrotateSign(6U));
  EXPECT_EQ(INT_MAX - 0, unrotateSign(UINT_MAX - 1U));
  EXPECT_EQ(INT_MAX - 1, unrotateSign(UINT_MAX - 3U));
  EXPECT_EQ(INT_MAX - 2, unrotateSign(UINT_MAX - 5U));
  EXPECT_EQ(INT_MAX - 3, unrotateSign(UINT_MAX - 7U));

  EXPECT_EQ(-1, unrotateSign(1U));
  EXPECT_EQ(-2, unrotateSign(3U));
  EXPECT_EQ(-3, unrotateSign(5U));
  EXPECT_EQ(-4, unrotateSign(7U));
  EXPECT_EQ(INT_MIN + 0, unrotateSign(UINT_MAX - 0U));
  EXPECT_EQ(INT_MIN + 1, unrotateSign(UINT_MAX - 2U));
  EXPECT_EQ(INT_MIN + 2, unrotateSign(UINT_MAX - 4U));
  EXPECT_EQ(INT_MIN + 3, unrotateSign(UINT_MAX - 6U));

  // Check long long.
  EXPECT_EQ(0LL, unrotateSign(0ULL));
  EXPECT_EQ(1LL, unrotateSign(2ULL));
  EXPECT_EQ(2LL, unrotateSign(4ULL));
  EXPECT_EQ(3LL, unrotateSign(6ULL));
  EXPECT_EQ(LLONG_MAX - 0LL, unrotateSign(ULLONG_MAX - 1ULL));
  EXPECT_EQ(LLONG_MAX - 1LL, unrotateSign(ULLONG_MAX - 3ULL));
  EXPECT_EQ(LLONG_MAX - 2LL, unrotateSign(ULLONG_MAX - 5ULL));
  EXPECT_EQ(LLONG_MAX - 3LL, unrotateSign(ULLONG_MAX - 7ULL));

  EXPECT_EQ(-1LL, unrotateSign(1ULL));
  EXPECT_EQ(-2LL, unrotateSign(3ULL));
  EXPECT_EQ(-3LL, unrotateSign(5ULL));
  EXPECT_EQ(-4LL, unrotateSign(7ULL));
  EXPECT_EQ(LLONG_MIN + 0LL, unrotateSign(ULLONG_MAX - 0ULL));
  EXPECT_EQ(LLONG_MIN + 1LL, unrotateSign(ULLONG_MAX - 2ULL));
  EXPECT_EQ(LLONG_MIN + 2LL, unrotateSign(ULLONG_MAX - 4ULL));
  EXPECT_EQ(LLONG_MIN + 3LL, unrotateSign(ULLONG_MAX - 6ULL));

  // Check char.
  EXPECT_EQ(char(0), unrotateSign(0U));
  EXPECT_EQ(char(1), unrotateSign(2U));
  EXPECT_EQ(char(2), unrotateSign(4U));
  EXPECT_EQ(char(3), unrotateSign(6U));
  EXPECT_EQ(char(CHAR_MAX - 0), unrotateSign(UCHAR_MAX - 1U));
  EXPECT_EQ(char(CHAR_MAX - 1), unrotateSign(UCHAR_MAX - 3U));
  EXPECT_EQ(char(CHAR_MAX - 2), unrotateSign(UCHAR_MAX - 5U));
  EXPECT_EQ(char(CHAR_MAX - 3), unrotateSign(UCHAR_MAX - 7U));

  EXPECT_EQ(char(-1LL), unrotateSign(1U));
  EXPECT_EQ(char(-2LL), unrotateSign(3U));
  EXPECT_EQ(char(-3LL), unrotateSign(5U));
  EXPECT_EQ(char(-4LL), unrotateSign(7U));
  EXPECT_EQ(char(CHAR_MIN + 0LL), unrotateSign(UCHAR_MAX - 0U));
  EXPECT_EQ(char(CHAR_MIN + 1LL), unrotateSign(UCHAR_MAX - 2U));
  EXPECT_EQ(char(CHAR_MIN + 2LL), unrotateSign(UCHAR_MAX - 4U));
  EXPECT_EQ(char(CHAR_MIN + 3LL), unrotateSign(UCHAR_MAX - 6U));
}

TEST(EncodingTest, VBR8) {
  for (unsigned V : {0U, 1U, 2U, 3U}) {
    unsigned MaxV = UINT_MAX - V;
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(V), HasValue(V));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MaxV), HasValue(MaxV));
  }
  for (unsigned long long V : {0U, 1U, 2U, 3U}) {
    unsigned long long MaxV = ULLONG_MAX - V;
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(V), HasValue(V));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MaxV), HasValue(MaxV));
  }
  for (unsigned char V : {0U, 1U, 2U, 3U}) {
    unsigned char MaxV = UCHAR_MAX - V;
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(V), HasValue(V));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MaxV), HasValue(MaxV));
  }
  for (int V : {0, 1, 2, 3}) {
    int MinV = INT_MIN + V;
    int MaxV = INT_MAX - V;
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(V), HasValue(V));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MaxV), HasValue(MaxV));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MinV), HasValue(MinV));
  }
  for (long long V : {0, 1, 2, 3}) {
    long long MinV = LLONG_MIN + V;
    long long MaxV = LLONG_MAX - V;
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(V), HasValue(V));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MaxV), HasValue(MaxV));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MinV), HasValue(MinV));
  }
  for (char V : {0, 1, 2, 3}) {
    char MinV = CHAR_MIN + V;
    char MaxV = CHAR_MAX - V;
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(V), HasValue(V));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MaxV), HasValue(MaxV));
    EXPECT_THAT_EXPECTED(makeRoundTripVBR8(MinV), HasValue(MinV));
  }
}

TEST(EncodingTest, VBR8Size) {
  // Check unsigned.
  EXPECT_EQ(1U, getSizeVBR8(0U));
  EXPECT_EQ(1U, getSizeVBR8(1U));
  EXPECT_EQ(1U, getSizeVBR8((1U << 7) - 1U));
  EXPECT_EQ(2U, getSizeVBR8((1U << 7)));
  EXPECT_EQ(2U, getSizeVBR8((1U << 14) - 1U));
  EXPECT_EQ(3U, getSizeVBR8((1U << 14)));
  EXPECT_EQ(4U, getSizeVBR8((1U << 28) - 1U));
  EXPECT_EQ(5U, getSizeVBR8((1U << 28)));
  EXPECT_EQ(5U, getSizeVBR8(UINT_MAX));

  // Spot-check int.
  EXPECT_EQ(1U, getSizeVBR8(0));
  EXPECT_EQ(1U, getSizeVBR8(1));
  EXPECT_EQ(1U, getSizeVBR8(63));
  EXPECT_EQ(2U, getSizeVBR8(64));
  EXPECT_EQ(1U, getSizeVBR8(-1));
  EXPECT_EQ(1U, getSizeVBR8(-2));
  EXPECT_EQ(1U, getSizeVBR8(-64));
  EXPECT_EQ(2U, getSizeVBR8(-65));

  // Check limits of unsigned long long.
  EXPECT_EQ(8U, getSizeVBR8(ULLONG_MAX >> 8));
  EXPECT_EQ(9U, getSizeVBR8(ULLONG_MAX >> 7));
  EXPECT_EQ(9U, getSizeVBR8(ULLONG_MAX));

  // Check char.
  EXPECT_EQ(1U, getSizeVBR8(char(0)));
  EXPECT_EQ(1U, getSizeVBR8(char(1)));
  EXPECT_EQ(1U, getSizeVBR8(char(127)));
  EXPECT_EQ(1U, getSizeVBR8(char(-1)));
  EXPECT_EQ(1U, getSizeVBR8(char(-128)));
}

} // end namespace
