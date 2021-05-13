//===- HashMappedTrieTest.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/HashMappedTrie.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Endian.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

static StringRef takeNextLine(StringRef &Lines) {
  size_t Newline = Lines.find('\n');
  StringRef Line = Lines.take_front(Newline);
  Lines = Lines.drop_front(Newline + 1);
  return Line;
}

namespace {

TEST(HashMappedTrieTest, TrieStructure) {
  using NumType = uint64_t;
  using HashType = std::array<uint8_t, sizeof(NumType)>;
  using TrieType = ThreadSafeHashMappedTrie<NumType, HashType>;
  NumType Numbers[] = {
      // Three numbers that will nest deeply to test (1) sinking subtries and
      // (2) deep, non-trivial hints.
      std::numeric_limits<NumType>::max(),
      std::numeric_limits<NumType>::max() - 2u,
      std::numeric_limits<NumType>::max() - 3u,
      // One number to stay at the top-level.
      0x37,
  };

  // Use the number itself as hash to test the pathological case.
  auto hash = [](NumType Num) {
    NumType HashN = llvm::support::endian::byte_swap(Num, llvm::support::big);
    HashType Hash;
    memcpy(&Hash[0], &HashN, sizeof(HashType));
    return Hash;
  };

  // Use root and subtrie sizes of 1 so this gets sunk quite deep.
  TrieType Trie(1, 1);
  for (NumType N : Numbers) {
    // Lookup first to exercise hint code for deep tries.
    TrieType::LookupResult Lookup = Trie.lookup(hash(N));
    EXPECT_FALSE(Lookup);

    Trie.insert(Lookup, TrieType::HashedDataType(hash(N), N));
  }
  for (NumType N : Numbers) {
    TrieType::LookupResult Lookup = Trie.lookup(hash(N));
    EXPECT_TRUE(Lookup);
    if (!Lookup)
      continue;
    EXPECT_EQ(hash(N), Lookup->Hash);
    EXPECT_EQ(N, Lookup->Data);

    // Confirm a subsequent insertion fails to overwrite by trying to insert a
    // bad value.
    EXPECT_EQ(
        N, Trie.insert(Lookup, TrieType::HashedDataType(hash(N), N - 1)).Data);
  }

  // Dump out the trie so we can confirm the structure is correct. Each subtrie
  // should have 2 slots. The root's index=0 should have the content for
  // 0x37 directly, and index=1 should be a linked-list of subtries, finally
  // ending with content for (max-2) and (max-3).
  //
  // Note: This structure is not exhaustive (too expensive to update tests),
  // but it does test that the dump format is somewhat readable and that the
  // basic structure is correct.
  //
  // Note: This test requires that the trie reads bytes starting from index 0
  // of the array of uint8_t, and then reads each byte's bits from high to low.
  SmallString<128> Dump;
  {
    raw_svector_ostream OS(Dump);
    Trie.print(OS);
  }

  // Check the header.
  StringRef DumpRef = Dump;
  ASSERT_EQ("root-bits=1 subtrie-bits=1", takeNextLine(DumpRef));

  // Check the root trie.
  ASSERT_EQ("root num-slots=2", takeNextLine(DumpRef));
  ASSERT_EQ("- index=0 content=[0000]000000000000037", takeNextLine(DumpRef));
  ASSERT_EQ("- index=1 subtrie=[1]", takeNextLine(DumpRef));
  ASSERT_EQ("subtrie=[1] num-slots=2", takeNextLine(DumpRef));

  // Check the last subtrie.
  size_t LastSubtrie = DumpRef.rfind("\nsubtrie=");
  ASSERT_NE(StringRef::npos, LastSubtrie);
  DumpRef = DumpRef.substr(LastSubtrie + 1);
  ASSERT_EQ("subtrie=fffffffffffffff[110] num-slots=2", takeNextLine(DumpRef));
  ASSERT_EQ("- index=0 content=fffffffffffffff[1100]", takeNextLine(DumpRef));
  ASSERT_EQ("- index=1 content=fffffffffffffff[1101]", takeNextLine(DumpRef));
  ASSERT_TRUE(DumpRef.empty());
}

TEST(HashMappedTrieTest, TrieStructureSmallFinalSubtrie) {
  using NumType = uint64_t;
  using HashType = std::array<uint8_t, sizeof(NumType)>;
  using TrieType = ThreadSafeHashMappedTrie<NumType, HashType>;
  NumType Numbers[] = {
      // Three numbers that will nest deeply to test (1) sinking subtries and
      // (2) deep, non-trivial hints.
      std::numeric_limits<NumType>::max(),
      std::numeric_limits<NumType>::max() - 2u,
      std::numeric_limits<NumType>::max() - 3u,
      // One number to stay at the top-level.
      0x37,
  };

  // Use the number itself as hash to test the pathological case.
  auto hash = [](NumType Num) {
    NumType HashN = llvm::support::endian::byte_swap(Num, llvm::support::big);
    HashType Hash;
    memcpy(&Hash[0], &HashN, sizeof(HashType));
    return Hash;
  };

  // Use subtrie size of 7 to avoid hitting 64 evenly, making the final subtrie
  // small.
  TrieType Trie(8, 5);
  for (NumType N : Numbers) {
    // Lookup first to exercise hint code for deep tries.
    TrieType::LookupResult Lookup = Trie.lookup(hash(N));
    EXPECT_FALSE(Lookup);

    Trie.insert(Lookup, TrieType::HashedDataType(hash(N), N));
  }
  for (NumType N : Numbers) {
    TrieType::LookupResult Lookup = Trie.lookup(hash(N));
    EXPECT_TRUE(Lookup);
    if (!Lookup)
      continue;
    EXPECT_EQ(hash(N), Lookup->Hash);
    EXPECT_EQ(N, Lookup->Data);

    // Confirm a subsequent insertion fails to overwrite by trying to insert a
    // bad value.
    EXPECT_EQ(
        N, Trie.insert(Lookup, TrieType::HashedDataType(hash(N), N - 1)).Data);
  }

  // Dump out the trie so we can confirm the structure is correct. The root
  // should have 2^8=256 slots, most subtries should have 2^5=32 slots, and the
  // deepest subtrie should have 2^1=2 slots (since (64-8)mod(5)=1).
  // should have 2 slots. The root's index=0 should have the content for
  // 0x37 directly, and index=1 should be a linked-list of subtries, finally
  // ending with content for (max-2) and (max-3).
  //
  // Note: This structure is not exhaustive (too expensive to update tests),
  // but it does test that the dump format is somewhat readable and that the
  // basic structure is correct.
  //
  // Note: This test requires that the trie reads bytes starting from index 0
  // of the array of uint8_t, and then reads each byte's bits from high to low.
  SmallString<128> Dump;
  {
    raw_svector_ostream OS(Dump);
    Trie.print(OS);
  }

  // Check the header.
  StringRef DumpRef = Dump;
  ASSERT_EQ("root-bits=8 subtrie-bits=5", takeNextLine(DumpRef));

  // Check the root trie.
  ASSERT_EQ("root num-slots=256", takeNextLine(DumpRef));
  ASSERT_EQ("- index=0 content=[00000000]00000000000037",
            takeNextLine(DumpRef));
  ASSERT_EQ("- index=255 subtrie=ff", takeNextLine(DumpRef));
  ASSERT_EQ("subtrie=ff num-slots=32", takeNextLine(DumpRef));

  // Check the last subtrie.
  size_t LastSubtrie = DumpRef.rfind("\nsubtrie=");
  ASSERT_NE(StringRef::npos, LastSubtrie);
  DumpRef = DumpRef.substr(LastSubtrie + 1);
  ASSERT_EQ("subtrie=fffffffffffffff[110] num-slots=2", takeNextLine(DumpRef));
  ASSERT_EQ("- index=0 content=fffffffffffffff[1100]", takeNextLine(DumpRef));
  ASSERT_EQ("- index=1 content=fffffffffffffff[1101]", takeNextLine(DumpRef));
  ASSERT_TRUE(DumpRef.empty());
}

TEST(HashMappedTrieTest, Strings) {
  for (unsigned RootBits : {2, 3, 6, 10}) {
    for (unsigned SubtrieBits : {2, 3, 4}) {
      ThreadSafeHashMappedTrieSet<std::string> Strings(RootBits, SubtrieBits);
      const std::string &A1 = Strings.insert("A");
      EXPECT_EQ(&A1, &Strings.insert("A"));
      std::string A2 = A1;
      EXPECT_EQ(&A1, &Strings.insert(A2));

      const std::string &B1 = Strings.insert("B");
      EXPECT_EQ(&B1, &Strings.insert(B1));
      std::string B2 = B1;
      EXPECT_EQ(&B1, &Strings.insert(B2));

      for (int I = 0, E = 1000; I != E; ++I) {
        ThreadSafeHashMappedTrieSet<std::string>::LookupResult Lookup;
        std::string S = Twine(I).str();
        if (I & 1)
          Lookup = Strings.lookup(S);
        const std::string &S1 = Strings.insert(Lookup, S);
        EXPECT_EQ(&S1, &Strings.insert(S1));
        std::string S2 = S1;
        EXPECT_EQ(&S1, &Strings.insert(S2));
      }
      for (int I = 0, E = 1000; I != E; ++I) {
        std::string S = Twine(I).str();
        ThreadSafeHashMappedTrieSet<std::string>::LookupResult Lookup =
            Strings.lookup(S);
        EXPECT_TRUE(Lookup);
        if (!Lookup)
          continue;
        EXPECT_EQ(S, *Lookup);
      }
    }
  }
}

} // namespace
