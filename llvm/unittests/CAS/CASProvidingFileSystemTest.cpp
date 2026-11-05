//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASProvidingFileSystem.h"
#include "llvm/CAS/CASFileSystem.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

TEST(CASProvidingFileSystemTest, Basic) {
  std::shared_ptr<ObjectStore> DB = createInMemoryCAS();
  auto FS = makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
  StringRef Path1 = "a.txt";
  StringRef Contents1 = "a";
  StringRef Path2 = "b.txt";
  StringRef Contents2 = "b";
  FS->addFile(Path1, 0, MemoryBuffer::getMemBuffer(Contents1));
  FS->addFile(Path2, 0, MemoryBuffer::getMemBuffer(Contents2));

  std::unique_ptr<vfs::FileSystem> VFS = createCASProvidingFileSystem(DB, FS);
  ASSERT_TRUE(VFS);
  auto &CASFS = cast<CASBackedFileSystem>(*VFS);
  {
    // Opening through the plain vfs::FileSystem interface still works; it just
    // does not carry a CAS reference.
    std::unique_ptr<vfs::File> File;
    ASSERT_THAT_ERROR(
        errorOrToExpected(CASFS.openFileForRead(Path1)).moveInto(File),
        Succeeded());
    ASSERT_TRUE(File);
    auto Buf = File->getBuffer(Path1);
    ASSERT_TRUE(Buf);
    EXPECT_EQ(Contents1, (*Buf)->getBuffer());
  }
  {
    std::optional<cas::ObjectRef> Ref;
    ASSERT_THAT_ERROR(CASFS.getObjectRefForFileContent(Path1).moveInto(Ref),
                      Succeeded());
    std::optional<ObjectProxy> BlobContents;
    ASSERT_THAT_ERROR(DB->getProxy(*Ref).moveInto(BlobContents), Succeeded());
    EXPECT_EQ(BlobContents->getData(), Contents1);
  }
  {
    std::optional<std::pair<std::unique_ptr<MemoryBuffer>, cas::ObjectRef>> Val;
    ASSERT_THAT_ERROR(
        CASFS
            .getBufferAndObjectRefForFile(Path2, /*FileSize*/ -1,
                                          /*RequiresNullTerminator*/ false,
                                          /*IsVolatile*/ false, /*IsText*/ true)
            .moveInto(Val),
        Succeeded());
    EXPECT_EQ(Contents2, Val->first->getBuffer());
    std::optional<ObjectProxy> BlobContents;
    ASSERT_THAT_ERROR(DB->getProxy(Val->second).moveInto(BlobContents),
                      Succeeded());
    EXPECT_EQ(BlobContents->getData(), Contents2);
  }
}

TEST(CASProvidingFileSystemTest, WithCASSupportingFS) {
  std::shared_ptr<ObjectStore> UnderlyingDB = createInMemoryCAS();
  auto FS = makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
  StringRef Path = "a.txt";
  StringRef Contents = "a";
  FS->addFile(Path, 0, MemoryBuffer::getMemBuffer(Contents));
  std::unique_ptr<vfs::FileSystem> UnderlyingFS =
      createCASProvidingFileSystem(UnderlyingDB, FS);
  ASSERT_TRUE(UnderlyingFS);

  std::shared_ptr<ObjectStore> DB = createInMemoryCAS();
  std::unique_ptr<vfs::FileSystem> VFS =
      createCASProvidingFileSystem(DB, std::move(UnderlyingFS));
  ASSERT_TRUE(VFS);
  auto &CASFS = cast<CASBackedFileSystem>(*VFS);

  std::optional<cas::ObjectRef> MaybeRef;
  ASSERT_THAT_ERROR(CASFS.getObjectRefForFileContent(Path).moveInto(MaybeRef),
                    Succeeded());
  cas::ObjectRef Ref = *MaybeRef;
  std::optional<ObjectProxy> BlobContents;
  ASSERT_THAT_ERROR(DB->getProxy(Ref).moveInto(BlobContents), Succeeded());
  EXPECT_EQ(BlobContents->getData(), Contents);

  CASID ID = DB->getID(Ref);
  std::optional<ObjectProxy> Proxy;
  // The file also ingested into the underlying FS.
  ASSERT_THAT_ERROR(UnderlyingDB->getProxy(ID).moveInto(Proxy), Succeeded());
}

/// A CAS reference must be keyed on the *contents* of a file, never on a path
/// taken from the top of a VFS stack.
///
/// This test pins the two properties that force that design, using the stack a
/// dependency scan actually builds: a RedirectingFileSystem (-ivfsoverlay) with
/// "use-external-names": false layered over a CAS-providing filesystem.
///
///   1. The CAS filesystem is not reachable by RTTI from the top of the stack,
///      so a consumer holding only the stack cannot ask it anything.
///   2. The name the stack reports for the file is the *virtual* path, which
///      exists in no filesystem below the overlay. Handing that path to the CAS
///      filesystem fails -- and in an overlay that remapped onto a different
///      file it would instead silently name the wrong contents.
///
/// Content identity has neither problem: the reference for the bytes that were
/// actually read is the same reference the CAS filesystem assigned them.
TEST(CASProvidingFileSystemTest, RefIsKeyedOnContentsNotPath) {
  std::shared_ptr<ObjectStore> DB = createInMemoryCAS();

  auto Lower = makeIntrusiveRefCnt<vfs::InMemoryFileSystem>();
  StringRef ExternalPath = "/root/real/actual.h";
  StringRef VirtualPath = "/root/vdir/virtual.h";
  StringRef Contents = "struct S { int x; };\n";
  Lower->addFile(ExternalPath, 0, MemoryBuffer::getMemBuffer(Contents));

  IntrusiveRefCntPtr<vfs::FileSystem> CASFS =
      createCASProvidingFileSystem(DB, Lower);
  ASSERT_TRUE(CASFS);

  std::string YAML = R"({
    "version": 0, "use-external-names": false,
    "roots": [ { "name": "/root/vdir", "type": "directory",
      "contents": [ { "name": "virtual.h", "type": "file",
                      "external-contents": "/root/real/actual.h" } ] } ] })";
  IntrusiveRefCntPtr<vfs::FileSystem> Stack = vfs::getVFSFromYAML(
      MemoryBuffer::getMemBufferCopy(YAML), /*DiagHandler=*/nullptr,
      /*YAMLFilePath=*/"overlay.yaml", /*DiagContext=*/nullptr, CASFS);
  ASSERT_TRUE(Stack);

  // (1) The CAS filesystem is not reachable from the top of the stack.
  EXPECT_EQ(dyn_cast<CASBackedFileSystem>(Stack.get()), nullptr);

  // The contents are readable through the stack under the virtual path, and
  // that virtual path is what the stack reports as the file's name -- i.e. it
  // is what a FileEntryRef would carry.
  auto File = Stack->openFileForRead(VirtualPath);
  ASSERT_TRUE(File);
  auto Name = (*File)->getName();
  ASSERT_TRUE(Name);
  EXPECT_EQ(*Name, VirtualPath);
  auto Buffer = (*File)->getBuffer(VirtualPath);
  ASSERT_TRUE(Buffer);
  EXPECT_EQ((*Buffer)->getBuffer(), Contents);

  auto &Backed = cast<CASBackedFileSystem>(*CASFS);

  // (2) Asking the CAS filesystem for that reported path fails: it never saw
  // it. This is why a path-keyed design cannot serve this stack.
  EXPECT_THAT_ERROR(Backed.getObjectRefForFileContent(VirtualPath).takeError(),
                    Failed());

  // Content identity works: the reference for the bytes we read is exactly the
  // reference the CAS filesystem assigned to the file it did see.
  std::optional<ObjectRef> RefFromContents;
  ASSERT_THAT_ERROR(
      DB->storeFromString({}, (*Buffer)->getBuffer()).moveInto(RefFromContents),
      Succeeded());
  std::optional<ObjectRef> RefFromExternalPath;
  ASSERT_THAT_ERROR(Backed.getObjectRefForFileContent(ExternalPath)
                        .moveInto(RefFromExternalPath),
                    Succeeded());
  EXPECT_EQ(*RefFromContents, *RefFromExternalPath);
}
