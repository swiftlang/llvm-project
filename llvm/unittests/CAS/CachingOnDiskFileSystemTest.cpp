//===- CachingOnDiskFileSystemTest.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Testing/Support/SupportHelpers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <map>
#include <string>

using namespace llvm;
using llvm::sys::fs::UniqueID;
using llvm::unittest::TempDir;
using llvm::unittest::TempFile;
using llvm::unittest::TempLink;

// FIXME: these tests are essentially all copy/pasted from
// VirtualFileSystemTest.cpp. They should be shared somehow.

namespace {

TEST(CachingOnDiskFileSystemTest, BasicRealFSIteration) {
  TempDir TestDirectory("virtual-file-system-test", /*Unique*/ true);
  IntrusiveRefCntPtr<vfs::FileSystem> FS =
      cantFail(cas::createCachingOnDiskFileSystem(cas::createInMemoryCAS()));

  std::error_code EC;
  vfs::directory_iterator I = FS->dir_begin(Twine(TestDirectory.path()), EC);
  ASSERT_FALSE(EC);
  EXPECT_EQ(vfs::directory_iterator(), I); // empty directory is empty

  TempDir _a(TestDirectory.path("a"));
  TempDir _ab(TestDirectory.path("a/b"));
  TempDir _c(TestDirectory.path("c"));
  TempDir _cd(TestDirectory.path("c/d"));

  I = FS->dir_begin(Twine(TestDirectory.path()), EC);
  ASSERT_FALSE(EC);
  ASSERT_NE(vfs::directory_iterator(), I);
  // Check either a or c, since we can't rely on the iteration order.
  EXPECT_TRUE(I->path().endswith("a") || I->path().endswith("c"));
  I.increment(EC);
  ASSERT_FALSE(EC);
  ASSERT_NE(vfs::directory_iterator(), I);
  EXPECT_TRUE(I->path().endswith("a") || I->path().endswith("c"));
  I.increment(EC);
  EXPECT_EQ(vfs::directory_iterator(), I);
}

TEST(CachingOnDiskFileSystemTest, MultipleWorkingDirs) {
  // Our root contains a/aa, b/bb, c, where c is a link to a/.
  // Run tests both in root/b/ and root/c/ (to test "normal" and symlink dirs).
  // Interleave operations to show the working directories are independent.
  TempDir Root("r", /*Unique*/ true);
  TempDir ADir(Root.path("a"));
  TempDir BDir(Root.path("b"));
  TempLink C(ADir.path(), Root.path("c"));
  TempFile AA(ADir.path("aa"), "", "aaaa");
  TempFile BB(BDir.path("bb"), "", "bbbb");
  IntrusiveRefCntPtr<vfs::FileSystem> BFS =
      cantFail(cas::createCachingOnDiskFileSystem(cas::createInMemoryCAS()));
  IntrusiveRefCntPtr<vfs::FileSystem> CFS;
  //,
  //                                    CFS = cantFail(
  //                                        cas::createCachingOnDiskFileSystem(
  //                                            cas::createInMemoryCAS()));
  ASSERT_FALSE(BFS->setCurrentWorkingDirectory(BDir.path()));
  ASSERT_FALSE(BFS->setCurrentWorkingDirectory(C.path()));

  // EXPECT_EQ(BDir.path(), *BFS->getCurrentWorkingDirectory());
  // EXPECT_EQ(C.path(), *CFS->getCurrentWorkingDirectory());

  // openFileForRead(), indirectly.
  // auto BBuf = BFS->getBufferForFile("bb");
  // ASSERT_TRUE(BBuf);
  // EXPECT_EQ("bbbb", (*BBuf)->getBuffer());

  return;
  auto ABuf = CFS->getBufferForFile("aa");
  ASSERT_TRUE(ABuf);
  EXPECT_EQ("aaaa", (*ABuf)->getBuffer());

  // status()
  auto BStat = BFS->status("bb");
  ASSERT_TRUE(BStat);
  EXPECT_EQ("bb", BStat->getName());

  auto AStat = CFS->status("aa");
  ASSERT_TRUE(AStat);
  EXPECT_EQ("aa", AStat->getName()); // unresolved name

  // getRealPath()
  SmallString<128> BPath;
  ASSERT_FALSE(BFS->getRealPath("bb", BPath));
  EXPECT_EQ(BB.path(), BPath);

  SmallString<128> APath;
  ASSERT_FALSE(CFS->getRealPath("aa", APath));
  EXPECT_EQ(AA.path(), APath); // Reports resolved name.

  // dir_begin
  std::error_code EC;
  auto BIt = BFS->dir_begin(".", EC);
  ASSERT_FALSE(EC);
  ASSERT_NE(BIt, vfs::directory_iterator());
  EXPECT_EQ((BDir.path() + "/./bb").str(), BIt->path());
  BIt.increment(EC);
  ASSERT_FALSE(EC);
  ASSERT_EQ(BIt, vfs::directory_iterator());

  auto CIt = CFS->dir_begin(".", EC);
  ASSERT_FALSE(EC);
  ASSERT_NE(CIt, vfs::directory_iterator());

  // Note: matches getRealFileSystem(), rather than getPhysicalFileSystem(),
  // the latter of which will incorrectly have resolved symlinks in the working
  // directory.
  EXPECT_EQ((C.path() + "/./aa").str(), CIt->path());
  CIt.increment(EC); // Because likely to read through this path.
  ASSERT_FALSE(EC);
  ASSERT_EQ(CIt, vfs::directory_iterator());
}

TEST(CachingOnDiskFileSystemTest, BrokenSymlinkRealFSIteration) {
  TempDir TestDirectory("virtual-file-system-test", /*Unique*/ true);
  IntrusiveRefCntPtr<vfs::FileSystem> FS =
      cantFail(cas::createCachingOnDiskFileSystem(cas::createInMemoryCAS()));

  TempLink _a("no_such_file", TestDirectory.path("a"));
  TempDir _b(TestDirectory.path("b"));
  TempLink _c("no_such_file", TestDirectory.path("c"));

  // Should get no iteration error, but a stat error for the broken symlinks.
  std::map<std::string, std::error_code> StatResults;
  std::error_code EC;
  for (vfs::directory_iterator
           I = FS->dir_begin(Twine(TestDirectory.path()), EC),
           E;
       I != E; I.increment(EC)) {
    EXPECT_FALSE(EC);
    StatResults[std::string(sys::path::filename(I->path()))] =
        FS->status(I->path()).getError();
  }
  EXPECT_THAT(
      StatResults,
      testing::ElementsAre(
          testing::Pair(
              "a", std::make_error_code(std::errc::no_such_file_or_directory)),
          testing::Pair("b", std::error_code()),
          testing::Pair("c", std::make_error_code(
                                 std::errc::no_such_file_or_directory))));
}

TEST(CachingOnDiskFileSystemTest, BasicRealFSRecursiveIteration) {
  TempDir TestDirectory("virtual-file-system-test", /*Unique*/ true);
  IntrusiveRefCntPtr<vfs::FileSystem> FS =
      cantFail(cas::createCachingOnDiskFileSystem(cas::createInMemoryCAS()));

  std::error_code EC;
  auto I =
      vfs::recursive_directory_iterator(*FS, Twine(TestDirectory.path()), EC);
  ASSERT_FALSE(EC);
  EXPECT_EQ(vfs::recursive_directory_iterator(), I); // empty directory is empty

  TempDir _a(TestDirectory.path("a"));
  TempDir _ab(TestDirectory.path("a/b"));
  TempDir _c(TestDirectory.path("c"));
  TempDir _cd(TestDirectory.path("c/d"));

  I = vfs::recursive_directory_iterator(*FS, Twine(TestDirectory.path()), EC);
  ASSERT_FALSE(EC);
  ASSERT_NE(vfs::recursive_directory_iterator(), I);

  std::vector<std::string> Contents;
  for (auto E = vfs::recursive_directory_iterator(); !EC && I != E;
       I.increment(EC)) {
    Contents.push_back(std::string(I->path()));
  }

  // Check contents, which may be in any order
  EXPECT_EQ(4U, Contents.size());
  int Counts[4] = {0, 0, 0, 0};
  for (const std::string &Name : Contents) {
    ASSERT_FALSE(Name.empty());
    int Index = Name[Name.size() - 1] - 'a';
    ASSERT_TRUE(Index >= 0 && Index < 4);
    Counts[Index]++;
  }
  EXPECT_EQ(1, Counts[0]); // a
  EXPECT_EQ(1, Counts[1]); // b
  EXPECT_EQ(1, Counts[2]); // c
  EXPECT_EQ(1, Counts[3]); // d
}

TEST(CachingOnDiskFileSystemTest, BasicRealFSRecursiveIterationNoPush) {
  TempDir TestDirectory("virtual-file-system-test", /*Unique*/ true);

  TempDir _a(TestDirectory.path("a"));
  TempDir _ab(TestDirectory.path("a/b"));
  TempDir _c(TestDirectory.path("c"));
  TempDir _cd(TestDirectory.path("c/d"));
  TempDir _e(TestDirectory.path("e"));
  TempDir _ef(TestDirectory.path("e/f"));
  TempDir _g(TestDirectory.path("g"));

  IntrusiveRefCntPtr<vfs::FileSystem> FS =
      cantFail(cas::createCachingOnDiskFileSystem(cas::createInMemoryCAS()));

  // Test that calling no_push on entries without subdirectories has no effect.
  {
    std::error_code EC;
    auto I =
        vfs::recursive_directory_iterator(*FS, Twine(TestDirectory.path()), EC);
    ASSERT_FALSE(EC);

    std::vector<std::string> Contents;
    for (auto E = vfs::recursive_directory_iterator(); !EC && I != E;
         I.increment(EC)) {
      Contents.push_back(std::string(I->path()));
      char last = I->path().back();
      switch (last) {
      case 'b':
      case 'd':
      case 'f':
      case 'g':
        I.no_push();
        break;
      default:
        break;
      }
    }
    EXPECT_EQ(7U, Contents.size());
  }

  // Test that calling no_push skips subdirectories.
  {
    std::error_code EC;
    auto I =
        vfs::recursive_directory_iterator(*FS, Twine(TestDirectory.path()), EC);
    ASSERT_FALSE(EC);

    std::vector<std::string> Contents;
    for (auto E = vfs::recursive_directory_iterator(); !EC && I != E;
         I.increment(EC)) {
      Contents.push_back(std::string(I->path()));
      char last = I->path().back();
      switch (last) {
      case 'a':
      case 'c':
      case 'e':
        I.no_push();
        break;
      default:
        break;
      }
    }

    // Check contents, which may be in any order
    EXPECT_EQ(4U, Contents.size());
    int Counts[7] = {0, 0, 0, 0, 0, 0, 0};
    for (const std::string &Name : Contents) {
      ASSERT_FALSE(Name.empty());
      int Index = Name[Name.size() - 1] - 'a';
      ASSERT_TRUE(Index >= 0 && Index < 7);
      Counts[Index]++;
    }
    EXPECT_EQ(1, Counts[0]); // a
    EXPECT_EQ(0, Counts[1]); // b
    EXPECT_EQ(1, Counts[2]); // c
    EXPECT_EQ(0, Counts[3]); // d
    EXPECT_EQ(1, Counts[4]); // e
    EXPECT_EQ(0, Counts[5]); // f
    EXPECT_EQ(1, Counts[6]); // g
  }
}

TEST(CachingOnDiskFileSystemTest, BrokenSymlinkRealFSRecursiveIteration) {
  TempDir TestDirectory("virtual-file-system-test", /*Unique*/ true);
  IntrusiveRefCntPtr<vfs::FileSystem> FS =
      cantFail(cas::createCachingOnDiskFileSystem(cas::createInMemoryCAS()));

  TempLink _a("no_such_file", TestDirectory.path("a"));
  TempDir _b(TestDirectory.path("b"));
  TempLink _ba("no_such_file", TestDirectory.path("b/a"));
  TempDir _bb(TestDirectory.path("b/b"));
  TempLink _bc("no_such_file", TestDirectory.path("b/c"));
  TempLink _c("no_such_file", TestDirectory.path("c"));
  TempDir _d(TestDirectory.path("d"));
  TempDir _dd(TestDirectory.path("d/d"));
  TempDir _ddd(TestDirectory.path("d/d/d"));
  TempLink _e("no_such_file", TestDirectory.path("e"));

  std::vector<std::string> VisitedBrokenSymlinks;
  std::vector<std::string> VisitedNonBrokenSymlinks;
  std::error_code EC;
  for (vfs::recursive_directory_iterator
           I(*FS, Twine(TestDirectory.path()), EC),
       E;
       I != E; I.increment(EC)) {
    EXPECT_FALSE(EC);
    (FS->status(I->path()) ? VisitedNonBrokenSymlinks : VisitedBrokenSymlinks)
        .push_back(std::string(I->path()));
  }

  // Check visited file names.
  EXPECT_THAT(VisitedBrokenSymlinks,
              testing::UnorderedElementsAre(_a.path().str(), _ba.path().str(),
                                            _bc.path().str(), _c.path().str(),
                                            _e.path().str()));
  EXPECT_THAT(VisitedNonBrokenSymlinks,
              testing::UnorderedElementsAre(_b.path().str(), _bb.path().str(),
                                            _d.path().str(), _dd.path().str(),
                                            _ddd.path().str()));
}

} // namespace
