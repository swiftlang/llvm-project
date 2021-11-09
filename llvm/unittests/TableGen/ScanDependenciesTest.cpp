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

using TestInputType = std::pair<StringRef, SmallVector<StringRef>>;

TEST(ScanDependenciesTest, scanTextForIncludes) {
  TestInputType Tests[] = {
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
    EXPECT_EQ(T.second, Includes);
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesNotStart) {
  TestInputType Tests[] = {
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
    EXPECT_EQ(T.second, Includes);
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesSuffix) {
  TestInputType Tests[] = {
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
    EXPECT_EQ(T.second, Includes);
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesWithPrefix) {
  // Don't recognize 'include' if it's not after an appropriate token?
  TestInputType Tests[] = {
      {"prefixinclude \"file.td\"", {}},
      {"prefix0include \"file.td\"", {}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);
    EXPECT_EQ(T.second, Includes);
  }
}

TEST(ScanDependenciesTest, scanFuzzyIncludesComments) {
  // It'd be nice to skip comments but the current fuzzy scan doesn't bother.
  TestInputType Tests[] = {
      {"// include \"file.td\"", {"file.td"}},
      {"/* include \"file.td\"", {"file.td"}},
      {"/* include \"file.td\" */", {"file.td"}},
  };
  for (auto T : Tests) {
    SmallVector<StringRef> Includes;
    scanTextForIncludes(T.first, Includes);
    EXPECT_EQ(T.second, Includes);
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
    flattenStrings(makeArrayRef(T.first), Flattened);
    EXPECT_EQ(T.second, StringRef(Flattened));

    SmallVector<StringRef> Split;
    splitFlattenedStrings(T.second, Split);
    EXPECT_EQ(T.first, Split);
  }
}

} // end namespace
