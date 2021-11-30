//===- BlockDataTest.cpp - Unit tests for BlockData encoding --------------===//
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
#include "llvm/Support/StringSaver.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::data;

namespace {

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
TEST(BlockDataTest, Empty) {
  EXPECT_DEATH(BlockData(""), "Expected at least two bytes of data");
  EXPECT_DEATH(BlockData("0"), "Expected at least two bytes of data");
}
#endif

template <class... ArgsT>
static BlockData makeData(BumpPtrAllocator &Alloc, ArgsT &&... Args) {
  SmallString<128> Data;
  BlockData::encode(std::forward<ArgsT>(Args)..., Data);
  StringSaver Saver(Alloc);
  return BlockData(Saver.save(Data.str()));
}

TEST(BlockDataTest, ZeroFill) {
  BumpPtrAllocator Alloc;
  Fixup TwoFixups[] = {
      {jitlink::Edge::Kind(0), 0U},
      {jitlink::Edge::Kind(127), 12345678U},
  };
  for (StringRef ContentHint : {"1", "longer content\n"}) {
    for (uint64_t Size :
         {1ULL, 2ULL, 7ULL, 1024ULL, (unsigned long long)ContentHint.size()}) {
      Optional<StringRef> Content;
      if (Size == ContentHint.size())
        Content = ContentHint;
      for (Align A :
           {Align(1ULL), Align(1ULL << 2), Align(1ULL << 7), Align(1ULL << 8),
            Align(1ULL << 9), Align(1ULL << 15), Align(1ULL << 16),
            Align(1ULL << 17), Align(1ULL << 31), Align(1ULL << 32),
            Align(1ULL << 33), Align(1ULL << 62)}) {
        for (uint64_t Offset : {0ULL, 1ULL, A.value() - 1ULL}) {
          if (Offset >= A.value())
            continue;
          for (ArrayRef<Fixup> Fixups :
               {ArrayRef<Fixup>(), makeArrayRef(TwoFixups[0]),
                makeArrayRef(TwoFixups[1]), makeArrayRef(TwoFixups)}) {
            BlockData Data =
                makeData(Alloc, Size, A.value(), Offset, Content, Fixups);
            EXPECT_EQ(!bool(Content), Data.isZeroFill());
            EXPECT_EQ(Size, Data.getSize());
            EXPECT_EQ(A.value(), Data.getAlignment());
            EXPECT_EQ(Offset, Data.getAlignmentOffset());
            EXPECT_EQ(Content, Data.getContent());
            {
              FixupList FL = Data.getFixups();
              EXPECT_EQ(Fixups,
                        makeArrayRef(SmallVector<Fixup>(FL.begin(), FL.end())));
            }

            uint64_t DecodedSize;
            uint64_t DecodedAlignment;
            uint64_t DecodedAlignmentOffset;
            Optional<StringRef> DecodedContent;
            FixupList DecodedFL;
            EXPECT_THAT_ERROR(Data.decode(DecodedSize, DecodedAlignment,
                                          DecodedAlignmentOffset,
                                          DecodedContent, DecodedFL),
                              Succeeded());
            EXPECT_EQ(Size, DecodedSize);
            EXPECT_EQ(A.value(), DecodedAlignment);
            EXPECT_EQ(Offset, DecodedAlignmentOffset);
            EXPECT_EQ(Content, DecodedContent);
            EXPECT_EQ(Fixups, makeArrayRef(SmallVector<Fixup>(
                                  DecodedFL.begin(), DecodedFL.end())));
          }
        }
      }
    }
  }
}

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
TEST(BlockDataTest, MismatchedContentSize) {
  BumpPtrAllocator Alloc;
  EXPECT_DEATH(makeData(Alloc, 2, 1, 0, StringRef("2"), None),
               "Mismatched content size");
}
#endif

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
TEST(BlockDataTest, NoAlignment) {
  BumpPtrAllocator Alloc;
  EXPECT_DEATH(makeData(Alloc, 2, 0, 0, None, None),
               "Expected non-zero alignment");
}
#endif

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
TEST(BlockDataTest, AlignmentOffsetTooBig) {
  BumpPtrAllocator Alloc;
  EXPECT_DEATH(makeData(Alloc, 2, 1, 1, None, None),
               "Expected alignment offset to be less than alignment");
  EXPECT_DEATH(makeData(Alloc, 2, 1, 2, None, None),
               "Expected alignment offset to be less than alignment");
  EXPECT_DEATH(makeData(Alloc, 2, 2, 2, None, None),
               "Expected alignment offset to be less than alignment");
  EXPECT_DEATH(makeData(Alloc, 2, 1024, 1024, None, None),
               "Expected alignment offset to be less than alignment");
}
#endif

} // end namespace
