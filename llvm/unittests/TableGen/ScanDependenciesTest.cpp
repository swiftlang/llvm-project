//===- unittests/TableGen/ScanDependenciesTest.cpp - Test tblgen scandeps -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/TableGen/ScanDependencies.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::tablegen;

namespace {

TEST(ScanDependenciesTest, scanTextForIncludes) {
  std::pair<StringRef, SmallVector<StringRef>> Tests[] = {
      {"", {}},
      {"include \"file.td\"", {"file.td"}},
      {"include\"file.td\"", {"file.td"}},
      {"include\t\"file.td\"", {"file.td"}},
      {"include\v\"file.td\"", {"file.td"}},
      {"include\n\"file.td\"", {"file.td"}},
      {"include\r\"file.td\"", {"file.td"}},
      {"include \t\v\n\r\"file.td\"", {"file.td"}},
      {"include \"file.td\"include \"file2.td\"", {"file.td", "file2.td"}},
      {"include \"file.td\" include \"file2.td\"", {"file.td", "file2.td"}},
      {"include \"file.td\"\ninclude \"file2.td\"\n", {"file.td", "file2.td"}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);

    // Include "T.first" so that it's included in failure output.
    EXPECT_EQ(std::make_pair(T.first, makeArrayRef(T.second)),
              std::make_pair(T.first, makeArrayRef(Includes)));
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesNotStart) {
  std::pair<StringRef, SmallVector<StringRef>> Tests[] = {
      {" include \"file.td\"", {"file.td"}},
      {"    include \"file.td\"", {"file.td"}},
      {";include \"file.td\"", {"file.td"}},
      {"\tinclude \"file.td\"", {"file.td"}},
      {"\vinclude \"file.td\"", {"file.td"}},
      {"\ninclude \"file.td\"", {"file.td"}},
      {"\rinclude \"file.td\"", {"file.td"}},
      {"after things include \"file.td\"", {"file.td"}},
      {"after things;include \"file.td\"", {"file.td"}},
      {"after things\tinclude \"file.td\"", {"file.td"}},
      {"after things\vinclude \"file.td\"", {"file.td"}},
      {"after things\ninclude \"file.td\"", {"file.td"}},
      {"after things\rinclude \"file.td\"", {"file.td"}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);

    // Include "T.first" so that it's included in failure output.
    EXPECT_EQ(std::make_pair(T.first, makeArrayRef(T.second)),
              std::make_pair(T.first, makeArrayRef(Includes)));
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesSuffix) {
  std::pair<StringRef, SmallVector<StringRef>> Tests[] = {
      {"include \"file.td\"\n", {"file.td"}},
      {"include \"file.td\"\r", {"file.td"}},
      {"include \"file.td\"\t", {"file.td"}},
      {"include \"file.td\"\v", {"file.td"}},
      {"include \"file.td\" ", {"file.td"}},
      {"include \"file.td\"\"", {"file.td"}},
      {"include \"file.td\"a", {"file.td"}},
      {"include \"file.td\";", {"file.td"}},
      {"include \"file.td\"'", {"file.td"}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);

    // Include "T.first" so that it's included in failure output.
    EXPECT_EQ(std::make_pair(T.first, makeArrayRef(T.second)),
              std::make_pair(T.first, makeArrayRef(Includes)));
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesWithPrefix) {
  // Don't recognize 'include' if it's not after an appropriate token?
  std::pair<StringRef, SmallVector<StringRef>> Tests[] = {
      {"prefixinclude \"file.td\"", {}},
      {"prefix0include \"file.td\"", {}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);

    // Include "T.first" so that it's included in failure output.
    EXPECT_EQ(std::make_pair(T.first, makeArrayRef(T.second)),
              std::make_pair(T.first, makeArrayRef(Includes)));
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesComments) {
  // It'd be nice to skip comments but the current fuzzy scan doesn't bother.
  std::pair<StringRef, SmallVector<StringRef>> Tests[] = {
      {"// include \"file.td\"", {"file.td"}},
      {"/* include \"file.td\"", {"file.td"}},
      {"/* include \"file.td\" */", {"file.td"}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);

    // Include "T.first" so that it's included in failure output.
    EXPECT_EQ(std::make_pair(T.first, makeArrayRef(T.second)),
              std::make_pair(T.first, makeArrayRef(Includes)));
  }
}

TEST(ScanDependenciesTest, flattenStrings) {
  std::pair<SmallVector<StringRef>, StringRef> Tests[] = {
      {{}, StringLiteral::withInnerNUL("")},
      {{""}, StringLiteral::withInnerNUL("\0")},
      {{"a"}, StringLiteral::withInnerNUL("a\0")},
      {{"ab"}, StringLiteral::withInnerNUL("ab\0")},
      {{"\n\n\n"}, StringLiteral::withInnerNUL("\n\n\n\0")},
      {{"", "ab", "ab", "de", "cd", ""},
       StringLiteral::withInnerNUL("\0ab\0ab\0de\0cd\0\0")},
  };
  for (auto T : Tests) {
    SmallString<128> Flattened;
    flattenStrings(T.first, Flattened);
    EXPECT_EQ(T, std::make_pair(T.first, StringRef(Flattened)));

    SmallVector<StringRef> Split;
    splitFlattenedStrings(T.second, Split);

    // Include "T.second" so that it's included in failure output.
    EXPECT_EQ(std::make_pair(makeArrayRef(T.first), T.second),
              std::make_pair(makeArrayRef(Split), T.second));
  }
}

} // end namespace
