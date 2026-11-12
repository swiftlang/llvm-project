//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/TracingCASBackedFileSystem.h"
#include "llvm/CAS/CASProvidingFileSystem.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

TEST(TracingCASBackedFileSystemTest, Basic) {
  std::shared_ptr<ObjectStore> DB = createInMemoryCAS();
  auto FS = makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
  StringRef Path = "/a.txt";
  StringRef Contents = "a";
  FS->addFile(Path, 0, MemoryBuffer::getMemBuffer(Contents));

  auto TracingFS = makeIntrusiveRefCnt<TracingCASBackedFileSystem>(
      createCASProvidingFileSystem(DB, FS));
  vfs::TracingFileSystem &Tracing = TracingFS->getTracingFS();
  EXPECT_EQ(&TracingFS->getCAS(), DB.get());

  EXPECT_TRUE(TracingFS->status(Path));
  EXPECT_TRUE(TracingFS->exists(Path));
  EXPECT_TRUE(TracingFS->openFileForRead(Path));
  EXPECT_EQ(Tracing.NumStatusCalls, 1u);
  EXPECT_EQ(Tracing.NumExistsCalls, 1u);
  EXPECT_EQ(Tracing.NumOpenFileForReadCalls, 1u);

  // Reading a file together with its reference opens it too.
  std::optional<std::pair<std::unique_ptr<MemoryBuffer>, ObjectRef>> BufAndRef;
  ASSERT_THAT_ERROR(
      TracingFS
          ->getBufferAndObjectRefForFile(Path, /*FileSize=*/-1,
                                         /*RequiresNullTerminator=*/false,
                                         /*IsVolatile=*/false, /*IsText=*/true)
          .moveInto(BufAndRef),
      Succeeded());
  EXPECT_EQ(BufAndRef->first->getBuffer(), Contents);
  std::optional<ObjectProxy> Blob;
  ASSERT_THAT_ERROR(DB->getProxy(BufAndRef->second).moveInto(Blob),
                    Succeeded());
  EXPECT_EQ(Blob->getData(), Contents);
  EXPECT_EQ(Tracing.NumOpenFileForReadCalls, 2u);

  // The tracing filesystem holding the counts is visible to clients that look
  // for it among the filesystem's children.
  vfs::TracingFileSystem *Found = nullptr;
  TracingFS->visit([&](vfs::FileSystem &Child) {
    if (auto *T = dyn_cast<vfs::TracingFileSystem>(&Child))
      Found = T;
  });
  EXPECT_EQ(Found, &Tracing);
}
