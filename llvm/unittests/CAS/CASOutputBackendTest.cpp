//===- CASOutputBackendTest.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASOutputBackend.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASFileSystem.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

template <class T>
static std::unique_ptr<T>
errorOrToPointer(ErrorOr<std::unique_ptr<T>> ErrorOrPointer) {
  if (ErrorOrPointer)
    return std::move(*ErrorOrPointer);
  return nullptr;
}

template <class T>
static std::unique_ptr<T>
expectedToPointer(Expected<std::unique_ptr<T>> ExpectedPointer) {
  if (ExpectedPointer)
    return std::move(*ExpectedPointer);
  consumeError(ExpectedPointer.takeError());
  return nullptr;
}

TEST(CASOutputBackendTest, createFiles) {
  std::unique_ptr<CASDB> CAS = createInMemoryCAS();
  ASSERT_TRUE(CAS);

  auto Outputs = makeIntrusiveRefCnt<CASOutputBackend>(*CAS);

  auto make = [&](StringRef Content, StringRef Path) {
    auto O = expectedToPointer(Outputs->createFile(Path));
    if (!O)
      return false;
    *O->takeOS() << Content;
    return !errorToBool(O->close());
  };
  EXPECT_TRUE(make("blob2", "/d2"));
  EXPECT_TRUE(make("blob1", "/t1/d1"));
  EXPECT_TRUE(make("blob3", "/t3/d3"));
  EXPECT_TRUE(make("blob1", "/t3/t1nested/d1"));
  EXPECT_TRUE(make("blob1", "/t3/t2/d1also"));
  EXPECT_TRUE(make("blob2", "/t3/t2/d2"));

  // FIXME: Add test of duplicate paths. Should probably error at createFile()
  // instead of createTree() when possible?
  Optional<TreeRef> Root = expectedToOptional(Outputs->createTree());
  ASSERT_TRUE(Root);

  // FIXME: Test directly instead of using CASFS.
  std::unique_ptr<vfs::FileSystem> CASFS =
      expectedToPointer(createCASFileSystem(*CAS, Root->getID()));
  ASSERT_TRUE(CASFS);

  std::unique_ptr<MemoryBuffer> T1D1 =
      errorOrToPointer(CASFS->getBufferForFile("/t1/d1"));
  std::unique_ptr<MemoryBuffer> T1NestedD1 =
      errorOrToPointer(CASFS->getBufferForFile("t3/t1nested/d1"));
  std::unique_ptr<MemoryBuffer> T3T2D1Also =
      errorOrToPointer(CASFS->getBufferForFile("/t3/t2/d1also"));
  std::unique_ptr<MemoryBuffer> T3TD3 =
      errorOrToPointer(CASFS->getBufferForFile("t3/d3"));
  ASSERT_TRUE(T1D1);
  ASSERT_TRUE(T1NestedD1);
  ASSERT_TRUE(T3T2D1Also);
  ASSERT_TRUE(T3TD3);

  EXPECT_EQ("/t1/d1", T1D1->getBufferIdentifier());
  EXPECT_EQ("t3/t1nested/d1", T1NestedD1->getBufferIdentifier());
  EXPECT_EQ("/t3/t2/d1also", T3T2D1Also->getBufferIdentifier());
  EXPECT_EQ("t3/d3", T3TD3->getBufferIdentifier());

  EXPECT_EQ("blob1", T1D1->getBuffer());
  EXPECT_EQ("blob1", T1NestedD1->getBuffer());
  EXPECT_EQ("blob1", T3T2D1Also->getBuffer());
  EXPECT_EQ("blob3", T3TD3->getBuffer());
}
