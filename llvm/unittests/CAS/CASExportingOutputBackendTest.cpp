//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASExportingOutputBackend.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Testing/Support/Error.h"
#include "llvm/Testing/Support/SupportHelpers.h"
#include "gtest/gtest.h"
#include <mutex>

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::vfs;

namespace {
/// Collects the recorded outputs, and whether they were on disk then.
class CollectingOutputBackend final : public CASExportingOutputBackend {
public:
  struct Output {
    std::string Path;
    ObjectRef Object;
    bool WasOnDisk;
  };

  explicit CollectingOutputBackend(ObjectStore &CAS)
      : CASExportingOutputBackend(CAS) {}

  SmallVector<Output> takeOutputs() { return std::move(Outputs); }

private:
  IntrusiveRefCntPtr<OutputBackend> cloneImpl() const override {
    return const_cast<CollectingOutputBackend *>(this);
  }

  Error recordOutput(StringRef Path, ObjectRef Object) override {
    std::lock_guard<std::mutex> Lock(OutputsLock);
    Outputs.push_back({Path.str(), Object, sys::fs::exists(Path)});
    return Error::success();
  }

  std::mutex OutputsLock;
  SmallVector<Output> Outputs;
};
} // namespace

/// Write \p Content as the output for \p Path in \p Backend.
static Error writeOutput(OutputBackend &Backend, StringRef Path,
                         StringRef Content,
                         std::optional<OutputConfig> Config = std::nullopt) {
  OutputFile O;
  if (Error E = Backend.createFile(Path, Config).moveInto(O))
    return E;
  O << Content;
  return O.keep();
}

static std::string readFile(StringRef Path) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> Buffer = MemoryBuffer::getFile(Path);
  if (!Buffer)
    return "<missing>";
  return (*Buffer)->getBuffer().str();
}

static void testExportingOutputBackend(ObjectStore &CAS) {
  unittest::TempDir D("CASExportingOutputBackendTest.d", /*Unique=*/true);
  SmallString<128> Path(D.path());
  sys::path::append(Path, "out.o");
  auto Outputs = makeIntrusiveRefCnt<CollectingOutputBackend>(CAS);

  // Checks that the only output recorded since the last check is \p Content
  // for \p Path, and that it was on disk when it was recorded.
  auto checkRecorded = [&](StringRef Path, StringRef Content) {
    SmallVector<CollectingOutputBackend::Output> Recorded =
        Outputs->takeOutputs();
    ASSERT_EQ(1u, Recorded.size());
    EXPECT_EQ(Path, Recorded[0].Path);
    EXPECT_TRUE(Recorded[0].WasOnDisk);
    std::optional<ObjectProxy> Object;
    ASSERT_THAT_ERROR(CAS.getProxy(Recorded[0].Object).moveInto(Object),
                      Succeeded());
    EXPECT_EQ(Content, Object->getData());
  };

  // Large enough for the on-disk CAS to store it in its own file, which can be
  // cloned.
  std::string Content(100 * 1024, 'a');

  // The output is written on disk when it's kept.
  {
    std::optional<OutputFile> O;
    ASSERT_THAT_ERROR(
        consumeDiscardOnDestroy(Outputs->createFile(Path)).moveInto(O),
        Succeeded());
    *O << Content;
    std::error_code EC;
    EXPECT_EQ(sys::fs::directory_iterator(D.path(), EC),
              sys::fs::directory_iterator());
    ASSERT_THAT_ERROR(O->keep(), Succeeded());
  }
  EXPECT_EQ(Content, readFile(Path));
  checkRecorded(Path, Content);

  // An unchanged output is left alone with OnlyIfDifferent.
  sys::fs::file_status Status1, Status2;
  ASSERT_FALSE(sys::fs::status(Path, Status1));
  ASSERT_THAT_ERROR(
      writeOutput(*Outputs, Path, Content, OutputConfig().setOnlyIfDifferent()),
      Succeeded());
  ASSERT_FALSE(sys::fs::status(Path, Status2));
#ifndef _WIN32
  // On Windows, UniqueIDs are hashes of the file paths.
  EXPECT_EQ(Status1.getUniqueID(), Status2.getUniqueID());
#endif
  EXPECT_EQ(Content, readFile(Path));
  checkRecorded(Path, Content);

  // Configurations that don't allow replacing the file are still honored.
  SmallString<128> AppendPath(D.path());
  sys::path::append(AppendPath, "missing", "trace.json");
  OutputConfig Append = OutputConfig().setAppend();
  ASSERT_THAT_ERROR(writeOutput(*Outputs, AppendPath, "a\n", Append),
                    Succeeded());
  checkRecorded(AppendPath, "a\n");
  ASSERT_THAT_ERROR(writeOutput(*Outputs, AppendPath, "b\n", Append),
                    Succeeded());
  checkRecorded(AppendPath, "b\n");
  EXPECT_EQ("a\nb\n", readFile(AppendPath));
#ifndef _WIN32
  ASSERT_THAT_ERROR(writeOutput(*Outputs, "/dev/null", Content), Succeeded());
  checkRecorded("/dev/null", Content);
  sys::fs::file_status Status;
  ASSERT_FALSE(sys::fs::status("/dev/null", Status));
  EXPECT_EQ(sys::fs::file_type::character_file, Status.type());
#endif

  // An existing object is written without being recorded.
  SmallString<128> ObjectPath(D.path());
  sys::path::append(ObjectPath, "object", "object.o");
  std::optional<ObjectProxy> Object;
  ASSERT_THAT_ERROR(CAS.createProxy({}, Content).moveInto(Object), Succeeded());
  ASSERT_THAT_ERROR(CASExportingOutputBackend::writeObject(ObjectPath, *Object),
                    Succeeded());
  EXPECT_EQ(Content, readFile(ObjectPath));
  EXPECT_TRUE(Outputs->takeOutputs().empty());

  // The output looks newly written, even if it's exported by cloning a file
  // with old timestamps, like one stored from an old file.
  SmallString<128> OldPath(D.path());
  sys::path::append(OldPath, "old.o");
  std::string OldContent(100 * 1024, 'b');
  {
    std::error_code EC;
    raw_fd_ostream OS(OldPath, EC);
    ASSERT_FALSE(EC);
    OS << OldContent;
  }
  const sys::TimePoint<> OldTime = sys::toTimePoint(946684800); // 2000
  ASSERT_FALSE(sys::fs::setLastAccessAndModificationTime(OldPath, OldTime));
  std::optional<ObjectRef> OldRef;
  ASSERT_THAT_ERROR(CAS.storeFromFile(OldPath).moveInto(OldRef), Succeeded());
  ASSERT_THAT_ERROR(CAS.getProxy(*OldRef).moveInto(Object), Succeeded());
  SmallString<128> NewPath(D.path());
  sys::path::append(NewPath, "new.o");
  ASSERT_THAT_ERROR(CASExportingOutputBackend::writeObject(NewPath, *Object),
                    Succeeded());
  EXPECT_EQ(OldContent, readFile(NewPath));
  sys::fs::file_status NewStatus;
  ASSERT_FALSE(sys::fs::status(NewPath, NewStatus));
  EXPECT_GT(NewStatus.getLastModificationTime(), OldTime);
}

TEST(CASExportingOutputBackendTest, InMemoryCAS) {
  std::unique_ptr<ObjectStore> CAS = createInMemoryCAS();
  ASSERT_TRUE(CAS);
  testExportingOutputBackend(*CAS);
}

#if LLVM_ENABLE_ONDISK_CAS
TEST(CASExportingOutputBackendTest, OnDiskCAS) {
  unittest::TempDir D("on-disk-cas", /*Unique=*/true);
  std::unique_ptr<ObjectStore> CAS;
  ASSERT_THAT_ERROR(createOnDiskCAS(D.path()).moveInto(CAS), Succeeded());
  testExportingOutputBackend(*CAS);
}
#endif
