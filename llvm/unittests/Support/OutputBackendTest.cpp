//===- unittests/Support/OutputBackendTest.cpp - OutputBackend tests ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/OutputBackend.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::vfs;

namespace {

using ContentBuffer = OutputFile::ContentBuffer;

TEST(OutputFileContentBufferTest, takeBuffer) {
  StringRef S = "data";
  ContentBuffer Content = S;
  EXPECT_FALSE(Content.ownsContent());
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(S.begin(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeBuffer("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());

  // The buffer should be a reference to the original data, and Content should
  // still have it too.
  EXPECT_EQ(S.begin(), Taken->getBufferStart());
  EXPECT_EQ(S.begin(), Content.getBytes().begin());
}

TEST(OutputFileContentBufferTest, takeBufferVector) {
  SmallString<0> V = StringRef("data");
  StringRef R = V;

  ContentBuffer Content = std::move(V);
  EXPECT_TRUE(Content.ownsContent());
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.begin(), Content.getBytes().begin());
  EXPECT_NE(V.begin(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeBuffer("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.begin(), Taken->getBufferStart());

  // Content should still have a reference to the data.
  EXPECT_FALSE(Content.ownsContent());
  EXPECT_EQ(Taken->getBuffer(), Content.getBytes());
  EXPECT_EQ(Taken->getBufferStart(), Content.getBytes().begin());
  EXPECT_EQ(Content.getBytes().begin(), Taken->getBufferStart());
}

TEST(OutputFileContentBufferTest, takeBufferVectorSmallStorage) {
  // Something using small storage will have to move.
  SmallString<128> V = StringRef("data");
  ASSERT_EQ(128u, V.capacity());
  StringRef R = V;

  ContentBuffer Content = std::move(V);
  EXPECT_TRUE(Content.ownsContent());
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_NE(R.begin(), Content.getBytes().begin());
}

TEST(OutputFileContentBufferTest, takeBufferMemoryBuffer) {
  std::unique_ptr<MemoryBuffer> B = MemoryBuffer::getMemBuffer("data");
  MemoryBufferRef R = *B;

  ContentBuffer Content = std::move(B);
  EXPECT_TRUE(Content.ownsContent());
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.getBufferStart(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeBuffer("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.getBufferStart(), Taken->getBufferStart());

  // Content should still have a reference to the data.
  EXPECT_FALSE(Content.ownsContent());
  EXPECT_EQ(Taken->getBuffer(), Content.getBytes());
  EXPECT_EQ(Taken->getBufferStart(), Content.getBytes().begin());
  EXPECT_EQ(Content.getBytes().begin(), Taken->getBufferStart());
}

TEST(OutputFileContentBufferTest, takeBufferMemoryBufferSameName) {
  std::unique_ptr<MemoryBuffer> B = MemoryBuffer::getMemBuffer("data", "name");
  MemoryBufferRef R = *B;

  ContentBuffer Content = std::move(B);
  EXPECT_TRUE(Content.ownsContent());
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.getBufferStart(), Content.getBytes().begin());

  // This exact buffer should be returned since the name matches.
  std::unique_ptr<MemoryBuffer> Taken = Content.takeBuffer("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.getBufferStart(), Taken->getBufferStart());
  EXPECT_EQ(R.getBufferIdentifier().begin(),
            Taken->getBufferIdentifier().begin());
}

TEST(OutputFileContentBufferTest, takeBufferMemoryBufferWrongName) {
  std::unique_ptr<MemoryBuffer> B = MemoryBuffer::getMemBuffer("data", "other");
  MemoryBufferRef R = *B;

  ContentBuffer Content = std::move(B);
  EXPECT_TRUE(Content.ownsContent());
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.getBufferStart(), Content.getBytes().begin());

  // Check that the name is updated.
  std::unique_ptr<MemoryBuffer> Taken = Content.takeBuffer("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.getBufferStart(), Taken->getBufferStart());
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrNull) {
  StringRef S = "data";
  ContentBuffer Content = S;
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(S.begin(), Content.getBytes().begin());

  // ContentBuffer doesn't own this.
  EXPECT_FALSE(Content.takeOwnedBufferOrNull("name"));
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrNullVector) {
  SmallString<0> V = StringRef("data");
  StringRef R = V;

  ContentBuffer Content = std::move(V);
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.begin(), Content.getBytes().begin());
  EXPECT_NE(V.begin(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeOwnedBufferOrNull("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.begin(), Taken->getBufferStart());

  // Content should still have a reference to the data.
  EXPECT_FALSE(Content.ownsContent());
  EXPECT_EQ(Taken->getBuffer(), Content.getBytes());
  EXPECT_EQ(Taken->getBufferStart(), Content.getBytes().begin());
  EXPECT_EQ(Content.getBytes().begin(), Taken->getBufferStart());

  // The next call should fail since it's no longer owned.
  EXPECT_FALSE(Content.takeOwnedBufferOrNull("name"));
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrNullVectorEmpty) {
  SmallString<0> V;

  ContentBuffer Content = std::move(V);
  EXPECT_EQ("", Content.getBytes());

  // Check that ContentBuffer::takeOwnedBufferOrNull() fails for an empty
  // vector.
  EXPECT_FALSE(Content.ownsContent());
  ASSERT_FALSE(Content.takeOwnedBufferOrNull("name"));

  // ContentBuffer::takeBuffer() should still work.
  std::unique_ptr<MemoryBuffer> Taken = Content.takeBuffer("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrNullMemoryBuffer) {
  std::unique_ptr<MemoryBuffer> B = MemoryBuffer::getMemBuffer("data");
  MemoryBufferRef R = *B;

  ContentBuffer Content = std::move(B);
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.getBufferStart(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeOwnedBufferOrNull("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.getBufferStart(), Taken->getBufferStart());

  // Content should still have a reference to the data.
  EXPECT_EQ(Taken->getBuffer(), Content.getBytes());
  EXPECT_EQ(Taken->getBufferStart(), Content.getBytes().begin());
  EXPECT_EQ(Content.getBytes().begin(), Taken->getBufferStart());

  // The next call should fail since it's no longer owned.
  EXPECT_FALSE(Content.takeOwnedBufferOrNull("name"));
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrCopy) {
  StringRef S = "data";
  ContentBuffer Content = S;
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(S.begin(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeOwnedBufferOrCopy("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());

  // We should have a copy, but Content should be unchanged.
  EXPECT_NE(S.begin(), Taken->getBufferStart());
  EXPECT_EQ(S.begin(), Content.getBytes().begin());
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrCopyVector) {
  SmallString<0> V = StringRef("data");
  StringRef R = V;

  ContentBuffer Content = std::move(V);
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.begin(), Content.getBytes().begin());
  EXPECT_NE(V.begin(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeOwnedBufferOrCopy("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());

  // Content should still have a reference to the data, but it's no longer
  // owned.
  EXPECT_EQ(Taken->getBuffer(), Content.getBytes());
  EXPECT_EQ(Taken->getBufferStart(), Content.getBytes().begin());
  EXPECT_EQ(Content.getBytes().begin(), Taken->getBufferStart());
  EXPECT_FALSE(Content.takeOwnedBufferOrNull("name"));

  // The next call should get a different (but equal) buffer.
  std::unique_ptr<MemoryBuffer> Copy = Content.takeOwnedBufferOrCopy("name");
  ASSERT_TRUE(Copy);
  EXPECT_EQ("data", Copy->getBuffer());
  EXPECT_EQ("name", Copy->getBufferIdentifier());
  EXPECT_NE(Taken->getBufferStart(), Copy->getBufferStart());
}

TEST(OutputFileContentBufferTest, takeOwnedBufferOrCopyMemoryBuffer) {
  std::unique_ptr<MemoryBuffer> B = MemoryBuffer::getMemBuffer("data");
  MemoryBufferRef R = *B;

  ContentBuffer Content = std::move(B);
  EXPECT_EQ("data", Content.getBytes());
  EXPECT_EQ(R.getBufferStart(), Content.getBytes().begin());

  std::unique_ptr<MemoryBuffer> Taken = Content.takeOwnedBufferOrCopy("name");
  ASSERT_TRUE(Taken);
  EXPECT_EQ("data", Taken->getBuffer());
  EXPECT_EQ("name", Taken->getBufferIdentifier());
  EXPECT_EQ(R.getBufferStart(), Taken->getBufferStart());

  // Content should still have a reference to the data.
  EXPECT_EQ(Taken->getBuffer(), Content.getBytes());
  EXPECT_EQ(Taken->getBufferStart(), Content.getBytes().begin());
  EXPECT_EQ(Content.getBytes().begin(), Taken->getBufferStart());

  // The next call should get a different (but equal) buffer.
  std::unique_ptr<MemoryBuffer> Copy = Content.takeOwnedBufferOrCopy("name");
  ASSERT_TRUE(Copy);
  EXPECT_EQ("data", Copy->getBuffer());
  EXPECT_EQ("name", Copy->getBufferIdentifier());
  EXPECT_NE(Taken->getBufferStart(), Copy->getBufferStart());
}

struct OnDiskTempDirectory {
  bool Created = false;
  SmallString<128> Path;

  OnDiskTempDirectory() = delete;
  explicit OnDiskTempDirectory(StringRef Name) {
    if (!sys::fs::createUniqueDirectory(Name, Path))
      Created = true;
  }
  ~OnDiskTempDirectory() {
    if (Created)
      sys::fs::remove_directories(Path);
  }
};

struct OnDiskFile {
  const OnDiskTempDirectory &D;
  SmallString<128> Path;
  StringRef ParentPath;
  StringRef Filename;
  StringRef Stem;
  StringRef Extension;
  std::unique_ptr<MemoryBuffer> LastBuffer;

  template <class... PathArgTypes>
  OnDiskFile(const OnDiskTempDirectory &D, PathArgTypes &&... PathArgs) : D(D) {
    assert(D.Created);
    sys::path::append(Path, D.Path, std::forward<PathArgTypes>(PathArgs)...);
    ParentPath = sys::path::parent_path(Path);
    Filename = sys::path::filename(Path);
    Stem = sys::path::stem(Filename);
    Extension = sys::path::extension(Filename);
  }

  Optional<OnDiskFile> findTemp() const {
    std::error_code EC;
    for (sys::fs::directory_iterator I(ParentPath, EC), E; !EC && I != E;
         I.increment(EC)) {
      StringRef TempPath = I->path();
      if (!TempPath.startswith(D.Path))
        continue;

      // Look for "<stem>-*.<extension>.tmp".
      if (sys::path::extension(TempPath) != ".tmp")
        continue;

      // Drop the ".tmp" and check the extension and stem.
      StringRef TempStem = sys::path::stem(TempPath);
      if (sys::path::extension(TempStem) != Extension)
        continue;
      StringRef OriginalStem = sys::path::stem(TempStem);
      if (!OriginalStem.startswith(Stem))
        continue;
      if (!OriginalStem.drop_front(Stem.size()).startswith("-"))
        continue;

      // Found it.
      return OnDiskFile(D, TempPath.drop_front(D.Path.size() + 1));
    }
    return None;
  }

  Optional<sys::fs::UniqueID> getCurrentUniqueID() {
    sys::fs::file_status Status;
    sys::fs::status(Path, Status, /*follow=*/false);
    if (!sys::fs::is_regular_file(Status))
      return None;
    return Status.getUniqueID();
  }

  bool hasUniqueID(sys::fs::UniqueID ID) {
    auto CurrentID = getCurrentUniqueID();
    if (!CurrentID)
      return false;
    return *CurrentID == ID;
  }

  Optional<StringRef> getCurrentContent() {
    auto OnDiskOrErr = MemoryBuffer::getFile(Path);
    if (!OnDiskOrErr)
      return None;
    LastBuffer = std::move(*OnDiskOrErr);
    return LastBuffer->getBuffer();
  }

  bool equalsCurrentContent(StringRef Data) {
    auto CurrentContent = getCurrentContent();
    if (!CurrentContent)
      return false;
    return *CurrentContent == Data;
  }

  bool equalsCurrentContent(NoneType) { return getCurrentContent() == None; }
};

struct OutputBackendTest : public ::testing::Test {
  Optional<OnDiskTempDirectory> D;

  void TearDown() override { D = None; }

  bool makeTempDirectory() {
    D.emplace("OutputBackendTest.d");
    return D->Created;
  }

  template <class T>
  std::unique_ptr<T>
  expectedToPointer(Expected<std::unique_ptr<T>> ExpectedPointer) {
    if (ExpectedPointer)
      return std::move(*ExpectedPointer);
    consumeError(ExpectedPointer.takeError());
    return nullptr;
  }

  template <class T>
  IntrusiveRefCntPtr<T>
  expectedToPointer(Expected<IntrusiveRefCntPtr<T>> ExpectedPointer) {
    if (ExpectedPointer)
      return std::move(*ExpectedPointer);
    consumeError(ExpectedPointer.takeError());
    return nullptr;
  }

  std::unique_ptr<MemoryBuffer> openBufferForRead(InMemoryFileSystem &FS,
                                                  StringRef Path) {
    if (auto FileOrErr = FS.openFileForRead(Path))
      if (*FileOrErr)
        if (auto BufferOrErr = (*FileOrErr)->getBuffer(Path))
          if (*BufferOrErr)
            return std::move(*BufferOrErr);
    return nullptr;
  }
};

TEST_F(OutputBackendTest, Null) {
  auto Backend = makeNullOutputBackend();

  // Create the output.
  auto O = expectedToPointer(Backend->createFile("ignored.data"));
  ASSERT_TRUE(O);
  ASSERT_TRUE(O->getOS());

  // Write some data into it and close the stream.
  *O->takeOS() << "some data";

  // Closing should have no effect.
  EXPECT_FALSE(errorToBool(O->close()));
}

TEST_F(OutputBackendTest, NullErase) {
  auto Backend = makeNullOutputBackend();

  // Create the output.
  auto O = expectedToPointer(Backend->createFile("ignored.data"));
  ASSERT_TRUE(O);
  ASSERT_TRUE(O->getOS());

  // Write some data into it and close the stream.
  *O->takeOS() << "some data";

  // Erasing should have no effect.
  O = nullptr;
}

TEST_F(OutputBackendTest, OnDisk) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  Backend->getSettings().WriteThrough.Use = false;

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // File shouldn't exist, but the temporary should.
  EXPECT_TRUE(File.equalsCurrentContent(None));
  Optional<OnDiskFile> Temp = File.findTemp();
  ASSERT_TRUE(Temp);
  Optional<sys::fs::UniqueID> TempID = Temp->getCurrentUniqueID();
  ASSERT_TRUE(TempID);

  // Write some data into it and flush.
  *O->takeOS() << "some data";
  EXPECT_TRUE(Temp->equalsCurrentContent("some data"));
  EXPECT_TRUE(File.equalsCurrentContent(None));

  // Close and check again.
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(Temp->equalsCurrentContent(None));
  EXPECT_TRUE(File.equalsCurrentContent("some data"));

  // The temp file should have been moved.
  EXPECT_TRUE(File.hasUniqueID(*TempID));
}

TEST_F(OutputBackendTest, OnDiskErase) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  Backend->getSettings().WriteThrough.Use = false;

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // File shouldn't exist, but the temporary should.
  EXPECT_TRUE(File.equalsCurrentContent(None));
  Optional<OnDiskFile> Temp = File.findTemp();
  ASSERT_TRUE(Temp);

  // Write some data into it and close the stream.
  *O->takeOS() << "some data";

  // Erase and check that the temp is gone.
  O = nullptr;
  EXPECT_TRUE(Temp->equalsCurrentContent(None));
  EXPECT_TRUE(File.equalsCurrentContent(None));
}

TEST_F(OutputBackendTest, OnDiskMissingDirectories) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "missing", "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // File shouldn't exist, but the temporary should.
  EXPECT_TRUE(File.equalsCurrentContent(None));
  Optional<OnDiskFile> Temp = File.findTemp();
  ASSERT_TRUE(Temp);
  Optional<sys::fs::UniqueID> TempID = Temp->getCurrentUniqueID();
  ASSERT_TRUE(TempID);

  // Write some data into it and flush.
  *O->takeOS() << "some data";
  EXPECT_TRUE(Temp->equalsCurrentContent("some data"));
  EXPECT_TRUE(File.equalsCurrentContent(None));

  // Close and check again.
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(Temp->equalsCurrentContent(None));
  EXPECT_TRUE(File.equalsCurrentContent("some data"));

  // The temp file should have been moved.
  EXPECT_TRUE(File.hasUniqueID(*TempID));
}

TEST_F(OutputBackendTest, OnDiskNoImplyCreateDirectories) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "missing", "on-disk.data");

  // Fail to create the missing directory.
  auto Expected = Backend->createFile(
      File.Path, OutputConfig().setNoImplyCreateDirectories());
  ASSERT_FALSE(Expected);
  std::error_code EC = errorToErrorCode(Expected.takeError());
  EXPECT_EQ(int(std::errc::no_such_file_or_directory), EC.value());
}

TEST_F(OutputBackendTest, OnDiskNoAtomicWrite) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(
      Backend->createFile(File.Path, OutputConfig().setNoAtomicWrite()));
  ASSERT_TRUE(O);

  // File should exist with no temporary.
  EXPECT_TRUE(File.equalsCurrentContent(""));
  Optional<sys::fs::UniqueID> ID = File.getCurrentUniqueID();
  ASSERT_TRUE(ID);
  Optional<OnDiskFile> Temp = File.findTemp();
  ASSERT_FALSE(Temp);

  // Write some data into it and flush.
  *O->takeOS() << "some data";
  EXPECT_TRUE(File.equalsCurrentContent("some data"));

  // Close and check again.
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent("some data"));
  EXPECT_TRUE(File.hasUniqueID(*ID));
}

TEST_F(OutputBackendTest, OnDiskNoAtomicWriteErase) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(
      Backend->createFile(File.Path, OutputConfig().setNoAtomicWrite()));
  ASSERT_TRUE(O);

  // File should exist with no temporary.
  EXPECT_TRUE(File.equalsCurrentContent(""));

  // Write some data into it and flush.
  *O->takeOS() << "some data";
  EXPECT_TRUE(File.equalsCurrentContent("some data"));

  // Erase. Since NoAtomicWrite was used the file should now be gone.
  O = nullptr;
}

TEST_F(OutputBackendTest, OnDiskWriteThroughUse) {
  auto Backend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  Backend->getSettings().WriteThrough.Use = true;

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // File shouldn't exist, but the temporary should.
  EXPECT_TRUE(File.equalsCurrentContent(None));
  Optional<OnDiskFile> Temp = File.findTemp();
  ASSERT_TRUE(Temp);
  Optional<sys::fs::UniqueID> TempID = Temp->getCurrentUniqueID();
  ASSERT_TRUE(TempID);

  // Write some data into it and flush. Confirm it's not on disk because it's
  // waiting to be written using a write-through buffer.
  *O->takeOS() << "some data";
  EXPECT_TRUE(Temp->equalsCurrentContent(""));
  EXPECT_TRUE(File.equalsCurrentContent(None));

  // Close and check again.
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(Temp->equalsCurrentContent(None));
  EXPECT_TRUE(File.equalsCurrentContent("some data"));
  EXPECT_TRUE(File.hasUniqueID(*TempID));
}

struct CaptureLastBufferOutputBackend : public StableUniqueEntityAdaptor<> {
  struct File : public OutputFile {
    Error storeContentBuffer(ContentBuffer &Content) final {
      Storage = Content.takeBuffer(getPath());
      return Error::success();
    }

    File(StringRef Path, std::unique_ptr<MemoryBuffer> &Storage)
        : OutputFile(Path), Storage(Storage) {}

    std::unique_ptr<MemoryBuffer> &Storage;
  };

  Expected<std::unique_ptr<OutputFile>> createFileImpl(StringRef Path,
                                                       OutputConfig) final {
    return std::make_unique<File>(Path, Storage);
  }

  std::unique_ptr<MemoryBuffer> &Storage;
  CaptureLastBufferOutputBackend(std::unique_ptr<MemoryBuffer> &Storage)
      : StableUniqueEntityAdaptorType(sys::path::Style::native),
        Storage(Storage) {}
};

TEST_F(OutputBackendTest, OnDiskWriteThroughForward) {
  auto OnDiskBackend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  OnDiskBackend->getSettings().WriteThrough.Use = true;
  OnDiskBackend->getSettings().WriteThrough.Forward = true;

  std::unique_ptr<MemoryBuffer> Buffer;
  auto Backend = makeMirroringOutputBackend(
      std::move(OnDiskBackend),
      makeIntrusiveRefCnt<CaptureLastBufferOutputBackend>(Buffer));

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write enough data to get a write-through buffer, and add one to ensure
  // it's not page-aligned.
  std::string Data;
  Data.append(OnDiskOutputBackend::MinimumSizeToReturnWriteThroughBuffer + 1,
              'z');
  *O->takeOS() << Data;
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent(Data));

  // Check that the saved buffer is a write-through buffer.
  EXPECT_TRUE(Buffer);
  EXPECT_EQ(File.Path, Buffer->getBufferIdentifier());
  EXPECT_EQ(Data, Buffer->getBuffer());
  EXPECT_EQ(MemoryBuffer::MemoryBuffer_MMap, Buffer->getBufferKind());
}

TEST_F(OutputBackendTest, OnDiskWriteThroughForwardTooSmall) {
  auto OnDiskBackend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  OnDiskBackend->getSettings().WriteThrough.Use = true;
  OnDiskBackend->getSettings().WriteThrough.Forward = true;

  std::unique_ptr<MemoryBuffer> Buffer;
  auto Backend = makeMirroringOutputBackend(
      std::move(OnDiskBackend),
      makeIntrusiveRefCnt<CaptureLastBufferOutputBackend>(Buffer));

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write almost enough data to return write-through buffer, but not quite.
  std::string Data;
  Data.append(OnDiskOutputBackend::MinimumSizeToReturnWriteThroughBuffer - 1,
              'z');
  *O->takeOS() << Data;
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent(Data));

  // Check that the saved buffer is not write-through buffer.
  EXPECT_TRUE(Buffer);
  EXPECT_EQ(File.Path, Buffer->getBufferIdentifier());
  EXPECT_EQ(Data, Buffer->getBuffer());
  EXPECT_EQ(MemoryBuffer::MemoryBuffer_Malloc, Buffer->getBufferKind());
}

TEST_F(OutputBackendTest, FilteredOnDisk) {
  auto Backend =
      makeFilteringOutputBackend(makeIntrusiveRefCnt<OnDiskOutputBackend>(),
                                 [](StringRef, OutputConfig) { return true; });

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close. The content should be there.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent("some data"));
}

TEST_F(OutputBackendTest, FilteredOnDiskSkipped) {
  auto Backend =
      makeFilteringOutputBackend(makeIntrusiveRefCnt<OnDiskOutputBackend>(),
                                 [](StringRef, OutputConfig) { return false; });

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close. It should not exist on disk.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent(None));
}

TEST_F(OutputBackendTest, InMemory) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS);

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  ASSERT_FALSE(FS->exists(Path));
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  ASSERT_FALSE(FS->exists(Path));
  EXPECT_FALSE(errorToBool(O->close()));

  // Lookup the file.
  ASSERT_TRUE(FS->exists(Path));
  std::unique_ptr<MemoryBuffer> Buffer = openBufferForRead(*FS, Path);
  EXPECT_EQ(Path, Buffer->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer->getBuffer());
}

TEST_F(OutputBackendTest, InMemoryFailIfExists) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS);

  // Add a conflicting file to FS.
  StringRef Path = "//root/in/memory.data";
  FS->addFile(Path, 0, MemoryBuffer::getMemBuffer("some data"));

  // Check that we get the error from failing to clobber the in-memory file.
  Backend->getSettings().FailIfExists = true;
  auto Expected = Backend->createFile(Path);
  ASSERT_FALSE(Expected);
  std::error_code EC = errorToErrorCode(Expected.takeError());
  EXPECT_EQ(int(std::errc::file_exists), EC.value());

  // No error if we turn off the flag though.
  Backend->getSettings().FailIfExists = false;
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Writing identical data should be okay.
  *O->takeOS() << "some data";
  ASSERT_FALSE(errorToBool(O->close()));
}

TEST_F(OutputBackendTest, InMemoryMismatchedContent) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS);
  Backend->getSettings().FailIfExists = false;

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  ASSERT_FALSE(FS->exists(Path));
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_FALSE(FS->exists(Path));
  ASSERT_TRUE(O);

  // Add a conflicting file to FS.
  FS->addFile(Path, 0, MemoryBuffer::getMemBuffer("old data"));
  ASSERT_TRUE(FS->exists(Path));

  // Write some data into it, flush, and close.
  *O->takeOS() << "new data";
  Error E = O->close();

  // Check the error.
  ASSERT_TRUE(bool(E));
  std::error_code EC = errorToErrorCode(std::move(E));
  EXPECT_EQ(int(std::errc::file_exists), EC.value());
}

TEST_F(OutputBackendTest, InMemoryNoOverwrite) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS);
  Backend->getSettings().FailIfExists = false;

  // Create the output once. This should be fine.
  StringRef Path = "//root/in/memory.data";
  auto O = expectedToPointer(
      Backend->createFile(Path, OutputConfig().setNoOverwrite()));
  ASSERT_TRUE(O);
  *O->takeOS() << "some data";
  ASSERT_FALSE(errorToBool(O->close()));

  // Doing it again should fail.
  auto Expected = Backend->createFile(Path, OutputConfig().setNoOverwrite());
  ASSERT_FALSE(Expected);
  std::error_code EC = errorToErrorCode(Expected.takeError());
  EXPECT_EQ(int(std::errc::file_exists), EC.value());

  // It should fail even in a different backend.
  Backend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS);
  Backend->getSettings().FailIfExists = false;
  Expected = Backend->createFile(Path, OutputConfig().setNoOverwrite());
  ASSERT_FALSE(Expected);
  EC = errorToErrorCode(Expected.takeError());
  EXPECT_EQ(int(std::errc::file_exists), EC.value());
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemory) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<OnDiskOutputBackend>(),
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS));

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent("some data"));

  // Lookup the file in FS.
  std::unique_ptr<MemoryBuffer> Buffer = openBufferForRead(*FS, File.Path);
  EXPECT_EQ(File.Path, Buffer->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer->getBuffer());
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemoryFailIfExists) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto InMemoryBackend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS);
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<OnDiskOutputBackend>(), InMemoryBackend);

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Add a file with the same name to FS.
  FS->addFile(File.Path, 0, MemoryBuffer::getMemBuffer("some data"));

  // Check that we get the error from failing to clobber the in-memory file.
  InMemoryBackend->getSettings().FailIfExists = true;
  auto Expected = Backend->createFile(File.Path);
  ASSERT_FALSE(Expected);
  std::error_code EC = errorToErrorCode(Expected.takeError());
  EXPECT_EQ(int(std::errc::file_exists), EC.value());

  // No error if we turn off the flag though.
  InMemoryBackend->getSettings().FailIfExists = false;
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Writing identical data should be okay.
  *O->takeOS() << "some data";
  ASSERT_FALSE(errorToBool(O->close()));
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemoryMismatchedContent) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<OnDiskOutputBackend>(),
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS));

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the output.
  ASSERT_FALSE(FS->exists(File.Path));
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_FALSE(FS->exists(File.Path));
  ASSERT_TRUE(O);

  // Add a conflicting file to FS.
  FS->addFile(File.Path, 0, MemoryBuffer::getMemBuffer("old data"));
  ASSERT_TRUE(FS->exists(File.Path));

  // Write some data into it, flush, and close.
  *O->takeOS() << "new data";
  Error E = O->close();

  // Check the error.
  ASSERT_TRUE(bool(E));
  std::error_code EC = errorToErrorCode(std::move(E));
  EXPECT_EQ(int(std::errc::file_exists), EC.value());
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemoryNoImplyCreateDirectories) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<OnDiskOutputBackend>(),
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS));

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "missing", "on-disk.data");

  // Check that we get the error from failing to create the missing directory
  // on-disk.
  auto Expected = Backend->createFile(
      File.Path, OutputConfig().setNoImplyCreateDirectories());
  ASSERT_FALSE(Expected);
  std::error_code EC = errorToErrorCode(Expected.takeError());
  EXPECT_EQ(int(std::errc::no_such_file_or_directory), EC.value());
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemoryWriteThrough) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto OnDiskBackend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  auto Backend = makeMirroringOutputBackend(
      OnDiskBackend, makeIntrusiveRefCnt<InMemoryOutputBackend>(FS));
  OnDiskBackend->getSettings().WriteThrough.Use = true;
  OnDiskBackend->getSettings().WriteThrough.Forward = true;

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write enough data to get a write-through buffer, and add one to ensure
  // it's not page-aligned.
  std::string Data;
  Data.append(OnDiskOutputBackend::MinimumSizeToReturnWriteThroughBuffer + 1,
              'z');
  *O->takeOS() << Data;
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent(Data));

  // Lookup the file in FS.
  std::unique_ptr<MemoryBuffer> Buffer = openBufferForRead(*FS, File.Path);
  EXPECT_EQ(File.Path, Buffer->getBufferIdentifier());
  EXPECT_EQ(Data, Buffer->getBuffer());
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemoryWriteThroughPageAligned) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto OnDiskBackend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  auto Backend = makeMirroringOutputBackend(
      OnDiskBackend, makeIntrusiveRefCnt<InMemoryOutputBackend>(FS));
  OnDiskBackend->getSettings().WriteThrough.Use = true;
  OnDiskBackend->getSettings().WriteThrough.Forward = true;

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write just enough data to get a write-through buffer. It'll likely be
  // page-aligned.
  std::string Data;
  Data.append(OnDiskOutputBackend::MinimumSizeToReturnWriteThroughBuffer, 'z');
  *O->takeOS() << Data;
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent(Data));

  // Lookup the file in FS. Ensure no errors when it creates a null-terminated
  // reference to the buffer.
  std::unique_ptr<MemoryBuffer> Buffer = openBufferForRead(*FS, File.Path);
  EXPECT_EQ(File.Path, Buffer->getBufferIdentifier());
  EXPECT_EQ(Data, Buffer->getBuffer());
}

TEST_F(OutputBackendTest, MirrorOnDiskInMemoryWriteThroughTooSmall) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto OnDiskBackend = makeIntrusiveRefCnt<OnDiskOutputBackend>();
  auto Backend = makeMirroringOutputBackend(
      OnDiskBackend, makeIntrusiveRefCnt<InMemoryOutputBackend>(FS));
  OnDiskBackend->getSettings().WriteThrough.Use = true;
  OnDiskBackend->getSettings().WriteThrough.Forward = true;

  // Get a filename we can use on disk.
  ASSERT_TRUE(makeTempDirectory());
  OnDiskFile File(*D, "on-disk.data");

  // Create the file.
  auto O = expectedToPointer(Backend->createFile(File.Path));
  ASSERT_TRUE(O);

  // Write almost (but not quite) enough data to get a write-through buffer.
  std::string Data;
  Data.append(OnDiskOutputBackend::MinimumSizeToReturnWriteThroughBuffer - 1,
              'z');
  *O->takeOS() << Data;
  EXPECT_FALSE(errorToBool(O->close()));
  EXPECT_TRUE(File.equalsCurrentContent(Data));

  // Lookup the file in FS.
  std::unique_ptr<MemoryBuffer> Buffer = openBufferForRead(*FS, File.Path);
  EXPECT_EQ(File.Path, Buffer->getBufferIdentifier());
  EXPECT_EQ(Data, Buffer->getBuffer());
}

TEST_F(OutputBackendTest, FilteredInMemory) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend =
      makeFilteringOutputBackend(makeIntrusiveRefCnt<InMemoryOutputBackend>(FS),
                                 [](StringRef, OutputConfig) { return true; });

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));

  // Lookup the file.
  ASSERT_TRUE(FS->exists(Path));
  std::unique_ptr<MemoryBuffer> Buffer = openBufferForRead(*FS, Path);
  EXPECT_EQ(Path, Buffer->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer->getBuffer());
}

TEST_F(OutputBackendTest, FilteredInMemorySkipped) {
  auto FS = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend =
      makeFilteringOutputBackend(makeIntrusiveRefCnt<InMemoryOutputBackend>(FS),
                                 [](StringRef, OutputConfig) { return false; });

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  ASSERT_FALSE(FS->exists(Path));
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));

  // The file should have been filtered out.
  ASSERT_FALSE(FS->exists(Path));
}

TEST_F(OutputBackendTest, MirrorFilteredInMemoryInMemoryNotFiltered) {
  auto FS1 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto FS2 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeMirroringOutputBackend(
      makeFilteringOutputBackend(
          makeIntrusiveRefCnt<InMemoryOutputBackend>(FS1),
          [](StringRef, OutputConfig) { return false; }),
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS2));

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));

  // Lookup the file. It should only be skipped in FS1.
  EXPECT_TRUE(!FS1->exists(Path));
  ASSERT_TRUE(FS2->exists(Path));
  std::unique_ptr<MemoryBuffer> Buffer2 = openBufferForRead(*FS2, Path);
  ASSERT_TRUE(Buffer2);
  EXPECT_EQ(Path, Buffer2->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer2->getBuffer());
}

TEST_F(OutputBackendTest, MirrorInMemoryWithFilteredInMemory) {
  auto FS1 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto FS2 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS1),
      makeFilteringOutputBackend(
          makeIntrusiveRefCnt<InMemoryOutputBackend>(FS2),
          [](StringRef, OutputConfig) { return false; }));

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));

  // Lookup the file. It should only be skipped in FS1.
  EXPECT_TRUE(!FS2->exists(Path));
  ASSERT_TRUE(FS1->exists(Path));
  std::unique_ptr<MemoryBuffer> Buffer2 = openBufferForRead(*FS1, Path);
  ASSERT_TRUE(Buffer2);
  EXPECT_EQ(Path, Buffer2->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer2->getBuffer());
}

TEST_F(OutputBackendTest, MirrorInMemoryInMemory) {
  auto FS1 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto FS2 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS1),
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS2));

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));

  // Lookup the file.
  ASSERT_TRUE(FS1->exists(Path));
  ASSERT_TRUE(FS2->exists(Path));
  std::unique_ptr<MemoryBuffer> Buffer1 = openBufferForRead(*FS1, Path);
  std::unique_ptr<MemoryBuffer> Buffer2 = openBufferForRead(*FS2, Path);
  ASSERT_TRUE(Buffer1);
  ASSERT_TRUE(Buffer2);
  EXPECT_EQ(Path, Buffer1->getBufferIdentifier());
  EXPECT_EQ(Path, Buffer2->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer1->getBuffer());
  EXPECT_EQ("some data", Buffer2->getBuffer());

  // Should be a reference to the same memory.
  EXPECT_EQ(Buffer1->getBufferStart(), Buffer2->getBufferStart());
}

TEST_F(OutputBackendTest, MirrorInMemoryInMemoryOwnBufferForMirror) {
  auto FS1 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto FS2 = makeIntrusiveRefCnt<InMemoryFileSystem>();
  auto CopyingBackend = makeIntrusiveRefCnt<InMemoryOutputBackend>(FS2);
  CopyingBackend->getSettings().CopyUnownedBuffers = true;
  auto Backend = makeMirroringOutputBackend(
      makeIntrusiveRefCnt<InMemoryOutputBackend>(FS1),
      std::move(CopyingBackend));

  // Create the output.
  StringRef Path = "//root/in/memory.data";
  auto O = expectedToPointer(Backend->createFile(Path));
  ASSERT_TRUE(O);

  // Write some data into it, flush, and close.
  *O->takeOS() << "some data";
  EXPECT_FALSE(errorToBool(O->close()));

  // Lookup the file.
  ASSERT_TRUE(FS1->exists(Path));
  ASSERT_TRUE(FS2->exists(Path));
  std::unique_ptr<MemoryBuffer> Buffer1 = openBufferForRead(*FS1, Path);
  std::unique_ptr<MemoryBuffer> Buffer2 = openBufferForRead(*FS2, Path);
  ASSERT_TRUE(Buffer1);
  ASSERT_TRUE(Buffer2);
  EXPECT_EQ(Path, Buffer1->getBufferIdentifier());
  EXPECT_EQ(Path, Buffer2->getBufferIdentifier());
  EXPECT_EQ("some data", Buffer1->getBuffer());
  EXPECT_EQ("some data", Buffer2->getBuffer());

  // Should be a copy.
  EXPECT_NE(Buffer1->getBufferStart(), Buffer2->getBufferStart());
}

} // anonymous namespace
