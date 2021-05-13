//===- CASDBTest.cpp ------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASDB.h"
#include "llvm/Support/FileSystem.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

class CASDBTest : public testing::TestWithParam<
                      std::function<std::unique_ptr<CASDB>(int)>> {
protected:
  Optional<int> NextCASIndex;

  std::unique_ptr<CASDB> createCAS() { return GetParam()((*NextCASIndex)++); }
  void SetUp() { NextCASIndex = 0; }
  void TearDown() { NextCASIndex = None; }
};

template <class T>
static std::unique_ptr<T>
expectedToPointer(Expected<std::unique_ptr<T>> ExpectedPointer) {
  if (ExpectedPointer)
    return std::move(*ExpectedPointer);
  consumeError(ExpectedPointer.takeError());
  return nullptr;
}

static CASID createBlobUnchecked(CASDB &CAS, StringRef Content) {
  return *expectedToOptional(CAS.createBlob(Content));
}

static CASID createTreeUnchecked(CASDB &CAS,
                                 ArrayRef<NamedTreeEntry> Entries = None) {
  return *expectedToOptional(CAS.createTree(Entries));
}

TEST_P(CASDBTest, Blobs) {
  std::unique_ptr<CASDB> CAS1 = createCAS();
  StringRef ContentStrings[] = {
      "word",
      "some longer text std::string's local memory",
      R"(multiline text multiline text multiline text multiline text
multiline text multiline text multiline text multiline text multiline text
multiline text multiline text multiline text multiline text multiline text
multiline text multiline text multiline text multiline text multiline text
multiline text multiline text multiline text multiline text multiline text
multiline text multiline text multiline text multiline text multiline text)",
  };

  SmallVector<CASID> IDs;
  for (StringRef Content : ContentStrings) {
    // Use StringRef::str() to create a temporary std::string. This could cause
    // problems if the CAS is storing references to the input string instead of
    // copying it.
    IDs.push_back(createBlobUnchecked(*CAS1, Content.str()));
  }

  // Check that the blobs give the same IDs later.
  for (int I = 0, E = IDs.size(); I != E; ++I)
    EXPECT_EQ(IDs[I], createBlobUnchecked(*CAS1, ContentStrings[I]));

  // Check that the blobs can be retrieved multiple times.
  for (int I = 0, E = IDs.size(); I != E; ++I) {
    for (int J = 0, JE = 3; J != JE; ++J) {
      Optional<BlobRef> Buffer = expectedToOptional(CAS1->getBlob(IDs[I]));
      ASSERT_TRUE(Buffer);
      EXPECT_EQ(ContentStrings[I], **Buffer);
    }
  }

  // Confirm these blobs don't exist in a fresh CAS instance.
  std::unique_ptr<CASDB> CAS2 = createCAS();
  for (int I = 0, E = IDs.size(); I != E; ++I)
    EXPECT_EQ(None, expectedToOptional(CAS2->getBlob(IDs[I])));

  // Insert into the second CAS and confirm the IDs are stable. Getting them
  // should work now.
  for (int I = IDs.size(), E = 0; I != E; --I) {
    auto &ID = IDs[I - 1];
    auto &Content = ContentStrings[I - 1];
    EXPECT_EQ(ID, createBlobUnchecked(*CAS2, Content));

    Optional<BlobRef> Buffer = expectedToOptional(CAS2->getBlob(ID));
    ASSERT_TRUE(Buffer);
    EXPECT_EQ(Content, **Buffer);
  }
}

TEST_P(CASDBTest, Trees) {
  std::unique_ptr<CASDB> CAS1 = createCAS();
  std::unique_ptr<CASDB> CAS2 = createCAS();

  auto createBlobInBoth = [&](StringRef Content) {
    CASID ID = createBlobUnchecked(*CAS1, Content);
    EXPECT_EQ(ID, createBlobUnchecked(*CAS2, Content));
    return ID;
  };

  CASID Blob1 = createBlobInBoth("blob1");
  CASID Blob2 = createBlobInBoth("blob2");
  CASID Blob3 = createBlobInBoth("blob3");

  SmallVector<SmallVector<NamedTreeEntry, 0>, 0> FlatTreeEntries = {
      {},
      {NamedTreeEntry(Blob1, TreeEntry::Regular, "regular")},
      {NamedTreeEntry(Blob2, TreeEntry::Executable, "executable")},
      {NamedTreeEntry(Blob3, TreeEntry::Symlink, "symlink")},
      {
          NamedTreeEntry(Blob1, TreeEntry::Regular, "various"),
          NamedTreeEntry(Blob1, TreeEntry::Regular, "names"),
          NamedTreeEntry(Blob1, TreeEntry::Regular, "that"),
          NamedTreeEntry(Blob1, TreeEntry::Regular, "do"),
          NamedTreeEntry(Blob1, TreeEntry::Regular, "not"),
          NamedTreeEntry(Blob1, TreeEntry::Regular, "conflict"),
          NamedTreeEntry(Blob1, TreeEntry::Regular, "but have spaces and..."),
          NamedTreeEntry(Blob1, TreeEntry::Regular,
                         "`~,!@#$%^&*()-+=[]{}\\<>'\""),
      },
  };

  SmallVector<CASID> FlatIDs;
  for (ArrayRef<NamedTreeEntry> Entries : FlatTreeEntries)
    FlatIDs.push_back(createTreeUnchecked(*CAS1, Entries));

  // Confirm we get the same IDs the second time and that the trees can be
  // visited (the entries themselves will be checked later).
  for (int I = 0, E = FlatIDs.size(); I != E; ++I) {
    EXPECT_EQ(FlatIDs[I], createTreeUnchecked(*CAS1, FlatTreeEntries[I]));
    Optional<TreeRef> Tree = expectedToOptional(CAS1->getTree(FlatIDs[I]));
    EXPECT_TRUE(Tree);
    EXPECT_EQ(FlatTreeEntries[I].size(), Tree->size());

    size_t NumCalls = 0;
    EXPECT_FALSE(
        errorToBool(Tree->forEachEntry([&NumCalls](ArrayRef<NamedTreeEntry>) {
          ++NumCalls;
          return Error::success();
        })));
    EXPECT_EQ(FlatTreeEntries[I].size(), NumCalls);
  }

  // Confirm these trees don't exist in a fresh CAS instance. Skip the first
  // tree, which is empty and could be implicitly in some CAS.
  for (int I = 1, E = FlatIDs.size(); I != E; ++I)
    EXPECT_FALSE(expectedToOptional(CAS2->getTree(FlatIDs[I])));

  // Insert into the other CAS and confirm the IDs are stable.
  for (int I = FlatIDs.size(), E = 0; I != E; --I) {
    auto &ID = FlatIDs[I - 1];
    // Make a copy of the original entries and sort them.
    SmallVector<NamedTreeEntry, 0> SortedEntries = FlatTreeEntries[I - 1];
    llvm::sort(SortedEntries);

    // Confirm we get the same tree out of CAS2.
    EXPECT_EQ(ID, createTreeUnchecked(*CAS2, SortedEntries));

    // Check that the correct entries come back.
    for (CASDB *CAS : {&*CAS1, &*CAS2}) {
      Optional<TreeRef> Tree = expectedToOptional(CAS->getTree(ID));
      EXPECT_TRUE(Tree);
      for (int I = 0, E = SortedEntries.size(); I != E; ++I)
        EXPECT_EQ(SortedEntries[I], Tree->get(I));
    }
  }

  // Create some nested trees.
  SmallVector<CASID> NestedTrees = FlatIDs;
  for (int I = 0, E = FlatTreeEntries.size() * 3; I != E; ++I) {
    // Copy one of the flat entries and add some trees.
    auto OriginalEntries =
        makeArrayRef(FlatTreeEntries[I % FlatTreeEntries.size()]);
    SmallVector<NamedTreeEntry> Entries(OriginalEntries.begin(),
                                        OriginalEntries.end());
    std::string Name = ("tree" + Twine(I)).str();
    Entries.emplace_back(FlatIDs[(I + 4) % FlatIDs.size()], TreeEntry::Tree,
                         Name);

    Optional<std::string> Name1, Name2;
    if (NestedTrees.size() >= 2) {
      int Nested1 = I % NestedTrees.size();
      int Nested2 = (I * 3 + 2) % NestedTrees.size();
      if (Nested2 == Nested1)
        Nested2 = (Nested1 + 1) % NestedTrees.size();
      ASSERT_NE(Nested1, Nested2);
      Name1.emplace(("tree" + Twine(I) + "-" + Twine(Nested1)).str());
      Name2.emplace(("tree" + Twine(I) + "-" + Twine(Nested2)).str());

      Entries.emplace_back(NestedTrees[I % NestedTrees.size()], TreeEntry::Tree,
                           *Name1);
      Entries.emplace_back(NestedTrees[(I * 3 + 2) % NestedTrees.size()],
                           TreeEntry::Tree, *Name2);
    }
    CASID ID = createTreeUnchecked(*CAS1, Entries);
    llvm::sort(Entries);
    EXPECT_EQ(ID, createTreeUnchecked(*CAS1, Entries));
    EXPECT_EQ(ID, createTreeUnchecked(*CAS2, Entries));

    for (CASDB *CAS : {&*CAS1, &*CAS2}) {
      Optional<TreeRef> Tree = expectedToOptional(CAS->getTree(ID));
      EXPECT_TRUE(Tree);
      for (int I = 0, E = Entries.size(); I != E; ++I)
        EXPECT_EQ(Entries[I], Tree->get(I));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(InMemoryCAS, CASDBTest, ::testing::Values([](int) {
                           return createInMemoryCAS();
                         }));
INSTANTIATE_TEST_SUITE_P(OnDiskCAS, CASDBTest, ::testing::Values([](int) {
                           // FIXME: Delete the directory in cleanup.
                           SmallString<128> Path;
                           std::error_code EC = sys::fs::createUniqueDirectory(
                               "on-disk-cas", Path);
                           (void)EC;
                           assert(!EC);
                           return std::move(
                               *expectedToOptional(createOnDiskCAS(Path)));
                         }));
