//===- OutputBackend.cpp - Virtualize compiler outputs --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements the OutputBackend interface.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/OutputBackend.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::vfs;

void OutputFile::anchor() {}
void OutputBackend::anchor() {}
void OutputDirectory::anchor() {}
void OnDiskOutputBackend::anchor() {}
void InMemoryOutputBackend::anchor() {}

void OutputError::anchor() {}
char OutputError::ID = 0;

void CannotOverwriteExistingOutputError::anchor() {}
char CannotOverwriteExistingOutputError::ID = 0;

void OnDiskOutputRenameTempError::anchor() {}
char OnDiskOutputRenameTempError::ID = 0;

OutputFile::~OutputFile() = default;

std::unique_ptr<raw_pwrite_stream> OutputFile::createStreamForContentBuffer() {
  Bytes = std::make_unique<SmallVector<char, 0>>();
  return std::make_unique<raw_svector_ostream>(*Bytes);
}

Error OutputFile::close() {
  assert(isOpen() && "Output already closed");

  // Destruct the stream if we still own it.
  OS = nullptr;
  IsOpen = false;

  // If there's no content buffer this is using a stream.
  if (!Bytes)
    return storeStreamedContent();

  // If there's a content buffer, send it along.
  OutputFile::ContentBuffer Buffer(std::move(*Bytes));
  return storeContentBuffer(Buffer);
}

void OutputFile::ContentBuffer::finishConstructingFromVector() {
  // If Vector is empty, we can't guarantee SmallVectorMemoryBuffer will keep
  // pointer identity. But since there's no content, it's all moot; just
  // construct a reference.
  if (Vector->empty()) {
    // Drop the vector and use a null-terminated string for the bytes.
    Vector = None;
    Bytes = "";
    return;
  }

  // Null-terminate and set the bytes.
  Vector->push_back(0);
  Vector->pop_back();
  Bytes = StringRef(Vector->begin(), Vector->size());
}

namespace {
class RenamedMemoryBuffer : public llvm::MemoryBuffer {
public:
  StringRef getBufferIdentifier() const override { return Identifier; }
  BufferKind getBufferKind() const override {
    return UnderlyingMB->getBufferKind();
  }

  RenamedMemoryBuffer(std::unique_ptr<llvm::MemoryBuffer> UnderlyingMB,
                      StringRef Identifier)
      : Identifier(Identifier.str()), UnderlyingMB(std::move(UnderlyingMB)) {
    init(this->UnderlyingMB->getBufferStart(),
         this->UnderlyingMB->getBufferEnd(),
         /*RequiresNullTerminator=*/true);
  }

private:
  std::string Identifier;
  std::unique_ptr<llvm::MemoryBuffer> UnderlyingMB;
};
} // namespace

std::unique_ptr<MemoryBuffer>
OutputFile::ContentBuffer::takeOwnedBufferOrNull(StringRef Identifier) {
  if (OwnedBuffer) {
    // Ensure the identifier is correct.
    if (OwnedBuffer->getBufferIdentifier() == Identifier)
      return std::move(OwnedBuffer);
    return std::make_unique<RenamedMemoryBuffer>(std::move(OwnedBuffer),
                                                 Identifier);
  }

  if (!Vector)
    return nullptr;

  bool IsEmpty = Vector->empty();
  (void)IsEmpty;
  auto VectorBuffer =
      std::make_unique<SmallVectorMemoryBuffer>(std::move(*Vector), Identifier);
  Vector = None;
  assert(Bytes.begin() == VectorBuffer->getBufferStart());
  assert(Bytes.end() == VectorBuffer->getBufferEnd());
  return VectorBuffer;
}

std::unique_ptr<MemoryBuffer>
OutputFile::ContentBuffer::takeBuffer(StringRef Identifier) {
  if (std::unique_ptr<MemoryBuffer> B = takeOwnedBufferOrNull(Identifier))
    return B;
  return MemoryBuffer::getMemBuffer(Bytes, Identifier);
}

std::unique_ptr<MemoryBuffer>
OutputFile::ContentBuffer::takeOwnedBufferOrCopy(StringRef Identifier) {
  if (std::unique_ptr<MemoryBuffer> B = takeOwnedBufferOrNull(Identifier))
    return B;
  return MemoryBuffer::getMemBufferCopy(Bytes, Identifier);
}

static std::unique_ptr<OutputFile> makeNullOutput(StringRef Path) {
  class NullOutput : public OutputFile {
    std::unique_ptr<raw_pwrite_stream> takeOSImpl() override {
      return std::make_unique<raw_null_ostream>();
    }
    Error storeStreamedContent() override { return Error::success(); }
    Error storeContentBuffer(ContentBuffer &) override {
      return Error::success();
    }
    bool isNull() const override { return true; }

  public:
    NullOutput(StringRef Path) : OutputFile(Path) {}
  };
  return std::make_unique<NullOutput>(Path);
}

Expected<StringRef>
OutputBackend::resolveOutputPath(const Twine &Path,
                                 SmallVectorImpl<char> &PathStorage) const {
  return Path.toStringRef(PathStorage);
}

IntrusiveRefCntPtr<OutputDirectory>
OutputBackend::getDirectoryImpl(StringRef ResolvedPath) const {
  return makeIntrusiveRefCnt<OutputDirectory>(const_cast<OutputBackend *>(this),
                                              ResolvedPath);
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
OutputBackend::getDirectory(const Twine &Path) const {
  SmallString<128> Storage;
  if (Expected<StringRef> E = resolveOutputPath(Path, Storage))
    return getDirectoryImpl(*E);
  else
    return E.takeError();
}

Expected<std::unique_ptr<OutputFile>>
OutputBackend::createFile(const Twine &Path, OutputConfig Config) {
  SmallString<128> Storage;
  if (Expected<StringRef> E = resolveOutputPath(Path, Storage))
    return createFileImpl(*E, Config);
  else
    return E.takeError();
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
OutputBackend::createDirectory(const Twine &Path, OutputConfig Config) {
  SmallString<128> Storage;
  if (Expected<StringRef> E = resolveOutputPath(Path, Storage))
    return createDirectoryImpl(*E, Config);
  else
    return E.takeError();
}

Expected<std::unique_ptr<OutputFile>>
OutputBackend::createUniqueFile(const Twine &Model, OutputConfig Config) {
  SmallString<128> Storage;
  if (Expected<StringRef> E = resolveOutputPath(Model, Storage))
    return createUniqueFileImpl(*E, Config);
  else
    return E.takeError();
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
OutputBackend::createUniqueDirectory(const Twine &Model, OutputConfig Config) {
  SmallString<128> Storage;
  if (Expected<StringRef> E = resolveOutputPath(Model, Storage))
    return createUniqueDirectoryImpl(*E, Config);
  else
    return E.takeError();
}

Expected<StringRef> OutputDirectoryAdaptorBase::resolveOutputPathImpl(
    const Twine &PathTwine, SmallVectorImpl<char> &PathStorage,
    sys::path::Style PathStyle) const {
  StringRef Path = PathTwine.toStringRef(PathStorage);
  if (sys::path::is_absolute(Path, PathStyle))
    return Path;

  // Return Path as-is if it has a root-name and it doesn't match
  // Directory's root name, indicating that Directory and Path are on different
  // drives.
  //
  // Note: This is intentionally asymmetric: a Directory without a root name
  // is not prepended if Path has a root name.
  StringRef PathRootName = sys::path::root_name(Path, PathStyle);
  if (!PathRootName.empty() &&
      PathRootName != sys::path::root_name(Directory, PathStyle))
    return Path;

  // Root names match. Make the path absolute.
  SmallString<128> ResolvedPath = StringRef(Directory);
  sys::path::append(ResolvedPath, PathStyle, Path);
  ResolvedPath.swap(PathStorage);
  return StringRef(PathStorage.begin(), PathStorage.size());
}

StringRef
StableUniqueEntityHelper::getNext(StringRef Model,
                                  SmallVectorImpl<char> &PathStorage) {
  unsigned N = Models[Model]++;

  // Replace '%' with chars representing N in hex.
  PathStorage.assign(Model.begin(), Model.end());
  for (auto I = PathStorage.rbegin(), E = PathStorage.rend(); I != E; ++I) {
    if (*I != '%')
      continue;
    *I = "0123456789abcdef"[N & 15];
    N >>= 4;
  }

  return StringRef(PathStorage.begin(), PathStorage.size());
}

template <class T>
static Expected<T> createStableUniqueEntity(
    StringRef Model, OutputConfig Config,
    llvm::function_ref<StringRef(StringRef, SmallVectorImpl<char> &)> GetNext,
    llvm::function_ref<Expected<T>(StringRef, OutputConfig)> CreateEntity) {
  // Turn off overwriting and try models one at a time.
  Config.setNoOverwrite();
  int NumTries = 0;
  SmallString<128> PathStorage;
  for (;;) {
    PathStorage.clear();
    StringRef Path = GetNext(Model, PathStorage);
    auto ExpectedEntity = CreateEntity(Path, Config);
    if (ExpectedEntity)
      return std::move(*ExpectedEntity);

    Error E = ExpectedEntity.takeError();
    if (++NumTries >= 128)
      return std::move(E);

    // FIXME: Return E if it can't indicate a failure to clobber an existing
    // entity; otherwise consume it and contintue.
    //
    // For OnDisk, we'd get:
    // - errc::file_exists in typical cases
    // - errc::permission_denied on Windows when marked for deletion
    //
    // But maybe we should rely on the backend being more clear.
  }
}

Expected<std::unique_ptr<OutputFile>>
StableUniqueEntityHelper::createStableUniqueFile(OutputBackend &Backend,
                                                 StringRef Model,
                                                 OutputConfig Config) {
  return createStableUniqueEntity<std::unique_ptr<OutputFile>>(
      Model, Config,
      [this](StringRef Model, SmallVectorImpl<char> &PathStorage) {
        return getNext(Model, PathStorage);
      },
      [&Backend](StringRef Path, OutputConfig Config) {
        return Backend.createFile(Path, Config);
      });
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
StableUniqueEntityHelper::createStableUniqueDirectory(OutputBackend &Backend,
                                                      StringRef Model,
                                                      OutputConfig Config) {
  return createStableUniqueEntity<IntrusiveRefCntPtr<OutputDirectory>>(
      Model, Config,
      [this](StringRef Model, SmallVectorImpl<char> &PathStorage) {
        return getNext(Model, PathStorage);
      },
      [&Backend](StringRef Path, OutputConfig Config) {
        return Backend.createDirectory(Path, Config);
      });
}

namespace {
class NullOutputBackend : public StableUniqueEntityAdaptor<> {
public:
  Expected<std::unique_ptr<OutputFile>> createFileImpl(StringRef Path,
                                                       OutputConfig) override {
    return makeNullOutput(Path);
  }

  explicit NullOutputBackend(sys::path::Style PathStyle)
      : StableUniqueEntityAdaptorType(PathStyle) {}
};
} // anonymous namespace

IntrusiveRefCntPtr<OutputBackend>
llvm::vfs::makeNullOutputBackend(sys::path::Style PathStyle) {
  return makeIntrusiveRefCnt<NullOutputBackend>(PathStyle);
}

namespace {
class OnDiskOutputFile : public OutputFile {
public:
  /// Open a file on-disk for writing to \a OutputPath.
  ///
  /// This calls \a initializeFD() to initialize \a FD (see that
  /// documentation for more detail) and \a UseWriteThroughBuffer. Unless \a
  /// UseWriteThroughBuffer was determined to be \c true, this function will
  /// then initialize \a OS with a valid stream.
  Error initializeFile();

  /// Erases the file if it hasn't be closed. If \a TempFile is set, erases it;
  /// otherwise, erases \a OutputPath.
  ~OnDiskOutputFile() override;

  /// Take an open output stream.
  std::unique_ptr<raw_pwrite_stream> takeOSImpl() override;

  Error storeStreamedContent() override { return closeFile(nullptr); }

  Error storeContentBuffer(ContentBuffer &Content) override {
    return closeFile(&Content);
  }

private:
  using CheckingContentConsumerType =
      function_ref<Error(Optional<ContentBuffer>)>;

  /// Close a file after successfully collecting the output.
  ///
  /// The output content must have already been written somewhere, but
  /// exactly where depends on the configuration.
  ///
  /// - If \a WriteThroughMemoryBuffer is \c true, the output will be in \a
  ///   Content and \a FD has an open file descriptor. In that case, the file
  ///   will be completed by opening \a WriteThroughMemoryBuffer and
  ///   calling \a std::memcpy().
  /// - Else if the output is in \a Content, it will be written to \a OS,
  ///   which should be an already-open file stream, and the content will be
  ///   written there and the stream destroyed to flush it.
  /// - Else the content should already be in the file.
  ///
  /// Once the content is on-disk, if \a TempPath is set, this calls \a
  /// sys::rename() to move the file to \a OutputPath.
  ///
  /// Returns a file-backed MemoryBuffer when the file was written using a
  /// write-through memory buffer, unless it won't be null-terminated or it's
  /// too small. Otherwise, when there's no error, returns \c nullptr.
  Error closeFile(ContentBuffer *MaybeContent);

  Expected<std::unique_ptr<sys::fs::mapped_file_region>>
  openMappedFile(size_t Size);

  std::unique_ptr<llvm::MemoryBuffer> convertMappedFileToBuffer(
      std::unique_ptr<sys::fs::mapped_file_region> Mapping);

  /// Attempt to open a temporary file for \a OutputPath.
  ///
  /// This tries to open a uniquely-named temporary file for \a OutputPath,
  /// possibly also creating any missing directories if \a
  /// OnDiskOutputConfig::UseTemporaryCreateMissingDirectories is set in \a
  /// Config.
  ///
  /// \post FD and \a TempPath are initialized if this is successful.
  std::error_code tryToCreateTemporary();

  /// Open a file on-disk for writing to OutputPath.
  ///
  /// This opens a file for writing and assigns the file descriptor to \c FD.
  /// Exactly how the file is opened depends on the \a OnDiskOutputConfig
  /// settings in \p Config and (if the file exists) the type of file.
  ///
  /// - If \a getPath() is \c "-" (indicating stdin), this function returns
  ///   \a Error::success() but has no effect.
  /// - Unless \a OutputConfig::getNoAtomicWrite(), a temporary file is opened
  ///   first, to be renamed to \a getPath() when \a closeFile() is called.
  ///   This is disabled if \a getPath() exists and \a
  ///   sys::fs::is_regular_file() returns \c false (such as a named pipe). If
  ///   \a tryToCreateTemporary() fails, this falls back to no-temp-file mode.
  ///   (Even if using a temporary file, \a getPath() is checked for write
  ///   permission.)
  /// - \a OnDiskOutputConfig::RemoveFileOnSignal installs a signal handler
  ///   to remove the opened file.
  ///
  /// This function also validates \c Settings.WriteThrough.Use based on the
  /// output file type.
  ///
  /// \post FD is set unless \a OutputPath is \c "-" or on error.
  /// \post TempPath is set if a temporary file was opened successfully.
  Error initializeFD();

  void initializeExistingFD();

public:
  OnDiskOutputFile(StringRef Path, OutputConfig Config,
                   OnDiskOutputBackend::OutputSettings Settings)
      : OutputFile(Path), Settings(Settings), Config(Config) {}

  /// Construct from an already-open file descriptor.
  OnDiskOutputFile(int FD, StringRef Path, OutputConfig Config,
                   OnDiskOutputBackend::OutputSettings Settings)
      : OutputFile(Path), Settings(Settings), Config(Config), FD(FD) {
    initializeExistingFD();
  }

private:
  OnDiskOutputBackend::OutputSettings Settings;
  OutputConfig Config;
  std::unique_ptr<raw_fd_ostream> OS;
  Optional<std::string> TempPath;
  Optional<int> FD;
};
} // anonymous namespace

std::error_code OnDiskOutputFile::tryToCreateTemporary() {
  // Create a temporary file.
  // Insert -%%%%%%%% before the extension (if any), and because some tools
  // (noticeable, clang's own GlobalModuleIndex.cpp) glob for build
  // artifacts, also append .tmp.
  StringRef OutputExtension = sys::path::extension(getPath());
  SmallString<128> TempPath =
      StringRef(getPath()).drop_back(OutputExtension.size());
  TempPath += "-%%%%%%%%";
  TempPath += OutputExtension;
  TempPath += ".tmp";

  auto tryToCreateImpl = [&]() {
    int NewFD;
    if (std::error_code EC =
            sys::fs::createUniqueFile(TempPath, NewFD, TempPath))
      return EC;
    if (Config.getCrashCleanup())
      sys::RemoveFileOnSignal(TempPath);
    this->TempPath = TempPath.str().str();
    FD = NewFD;
    return std::error_code();
  };

  // Try to create the temporary.
  std::error_code EC = tryToCreateImpl();
  if (!EC)
    return std::error_code();

  if (EC != std::errc::no_such_file_or_directory ||
      Config.getNoImplyCreateDirectories())
    return EC;

  // Create parent directories and try again.
  StringRef ParentPath = sys::path::parent_path(getPath());
  if ((EC = sys::fs::create_directories(ParentPath)))
    return EC;
  return tryToCreateImpl();
}

Error OnDiskOutputFile::initializeFD() {
  // Disable temporary file for stdout (and return early since we won't use a
  // file descriptor directly).
  if (getPath() == "-") {
    Settings.WriteThrough.Use = false;
    return Error::success();
  }

  // Function to check and update whether to use a write-through buffer.
  bool CheckedStatusForWriteThrough = false;
  auto checkStatusForWriteThrough = [&](const sys::fs::file_status &Status) {
    if (CheckedStatusForWriteThrough)
      return;
    sys::fs::file_type Type = Status.type();
    if (Type != sys::fs::file_type::regular_file &&
        Type != sys::fs::file_type::block_file)
      Settings.WriteThrough.Use = false;
    CheckedStatusForWriteThrough = true;
  };

  // Disable temporary file for other non-regular files, and if we get a status
  // object, also check if we can write and disable write-through buffers if
  // appropriate.
  if (Config.getAtomicWrite()) {
    sys::fs::file_status Status;
    sys::fs::status(getPath(), Status);
    if (sys::fs::exists(Status)) {
      if (!sys::fs::is_regular_file(Status))
        Config.setNoAtomicWrite();

      // Fail now if we can't write to the final destination.
      if (!sys::fs::can_write(getPath()))
        return errorCodeToError(
            std::make_error_code(std::errc::operation_not_permitted));

      checkStatusForWriteThrough(Status);
    }
  }

  // If (still) using a temporary file, try to create it (and return success if
  // that works).
  if (Config.getAtomicWrite())
    if (!tryToCreateTemporary())
      return Error::success();

  // Not using a temporary file. Open the final output file.
  int NewFD;
  if (auto EC = sys::fs::openFileForWrite(
          getPath(), NewFD, sys::fs::CD_CreateAlways,
          Config.getText() ? sys::fs::OF_Text : sys::fs::OF_None))
    return errorCodeToError(EC);
  FD = NewFD;

  // Check the status with the open FD to see whether a write-through buffer
  // makes sense.
  if (Settings.WriteThrough.Use && !CheckedStatusForWriteThrough) {
    sys::fs::file_status Status;
    sys::fs::status(NewFD, Status);
    checkStatusForWriteThrough(Status);
  }

  if (Config.getCrashCleanup())
    sys::RemoveFileOnSignal(getPath());
  return Error::success();
}

void OnDiskOutputFile::initializeExistingFD() {
  this->Config.setNoAtomicWrite();
  if (Config.getCrashCleanup())
    sys::RemoveFileOnSignal(getPath());
}

Error OnDiskOutputFile::initializeFile() {
  if (!FD)
    if (Error E = initializeFD())
      return E;

  assert(FD || getPath() == "-");
  if (Settings.WriteThrough.Use)
    return Error::success();

  // Open the raw_fd_ostream right away to free the file descriptor.
  std::error_code EC;
  OS = getPath() == "-"
           ? std::make_unique<raw_fd_ostream>(getPath(), EC)
           : std::make_unique<raw_fd_ostream>(*FD, /*shouldClose=*/true);
  assert(!EC && "Unexpected error opening stdin");

  return Error::success();
}

std::unique_ptr<raw_pwrite_stream> OnDiskOutputFile::takeOSImpl() {
  if (Settings.WriteThrough.Use)
    return createStreamForContentBuffer();

  assert(OS && "Expected file to be initialized");

  // Check whether we can get away with returning the stream directly. Note
  // that we don't need seeking for Text files.
  if (OS->supportsSeeking() || Config.getText())
    return std::move(OS);

  // Fall back on the content buffer.
  return createStreamForContentBuffer();
}

std::unique_ptr<llvm::MemoryBuffer> OnDiskOutputFile::convertMappedFileToBuffer(
    std::unique_ptr<sys::fs::mapped_file_region> Mapping) {
  assert(Mapping->const_data()[Mapping->size()] == 0 &&
         "Expected mmap region to be null-terminated");

  class MappedMemoryBuffer : public llvm::MemoryBuffer {
  public:
    StringRef getBufferIdentifier() const override { return Path; }
    BufferKind getBufferKind() const override { return MemoryBuffer_MMap; }

    MappedMemoryBuffer(StringRef Path,
                       std::unique_ptr<sys::fs::mapped_file_region> Mapping)
        : Path(Path.str()), Mapping(std::move(Mapping)) {
      const char *Data = this->Mapping->const_data();
      init(Data, Data + this->Mapping->size(), /*RequiresNullTerminator=*/true);
    }

  private:
    std::string Path;
    std::unique_ptr<sys::fs::mapped_file_region> Mapping;
  };
  return std::make_unique<MappedMemoryBuffer>(getPath(), std::move(Mapping));
}

Expected<std::unique_ptr<sys::fs::mapped_file_region>>
OnDiskOutputFile::openMappedFile(size_t Size) {
  if (auto EC = sys::fs::resize_file_before_mapping_readwrite(*FD, Size))
    return errorCodeToError(EC);

  std::error_code EC;
  auto Mapping = std::make_unique<sys::fs::mapped_file_region>(
      sys::fs::convertFDToNativeFile(*FD),
      sys::fs::mapped_file_region::readwrite, Size, 0, EC);
  if (EC)
    return errorCodeToError(EC);
  return Mapping;
}

Error OnDiskOutputFile::closeFile(ContentBuffer *MaybeContent) {
  Optional<StringRef> BufferedContent;
  if (MaybeContent)
    BufferedContent = MaybeContent->getBytes();
  if (Settings.WriteThrough.Use) {
    assert(FD && "Write-through buffer needs a file descriptor");
    assert(!OS && "Expected no stream when using write-through buffer");
    assert(BufferedContent && "Expected buffered content");
    std::unique_ptr<sys::fs::mapped_file_region> Mapping;
    if (auto ExpectedMapping = openMappedFile(BufferedContent->size()))
      Mapping = std::move(*ExpectedMapping);
    else
      return ExpectedMapping.takeError();

    std::memcpy(Mapping->data(), BufferedContent->begin(),
                BufferedContent->size());

    // Decide whether to return the file-backed buffer.  Skip it when it's too
    // small (fragmenting memory) or page-aligned (missing a null-terminator).
    static unsigned PageSize = sys::Process::getPageSizeEstimate();
    if (MaybeContent && Settings.WriteThrough.Forward &&
        BufferedContent->size() >=
            OnDiskOutputBackend::MinimumSizeToReturnWriteThroughBuffer &&
        (BufferedContent->size() & (PageSize - 1)) != 0) {
      // Synchronize the mapping to disk.
      Mapping->sync();

      // Forward the buffer.
      *MaybeContent =
          ContentBuffer(convertMappedFileToBuffer(std::move(Mapping)));
    }
    // Else, Mapping to destroy itself.
  } else if (OS) {
    // Write out the content and close the stream.
    assert(BufferedContent && "Need to write content to a stream");
    OS->write(BufferedContent->begin(), BufferedContent->size());
    OS = nullptr;
  } else {
    assert(!BufferedContent &&
           "Content in memory with no way to write it to disk?");
  }

  // Return early if there's no temporary path.
  if (!TempPath)
    return Error::success();

  // Move temporary to the final output path.
  std::error_code EC = sys::fs::rename(*TempPath, getPath());
  if (!EC)
    return Error::success();
  (void)sys::fs::remove(*TempPath);
  return createOnDiskOutputRenameTempError(*TempPath, getPath(), EC);
}

OnDiskOutputFile::~OnDiskOutputFile() {
  if (!isOpen())
    return;

  destroyOS();

  // If there's no FD we haven't created a file.
  if (!FD)
    return;

  // If there's a temporary file, that's the one to delete.
  if (TempPath)
    sys::fs::remove(*TempPath);
  else
    sys::fs::remove(getPath());
}

Expected<std::unique_ptr<OutputFile>>
OnDiskOutputBackend::createFileImpl(StringRef ResolvedPath,
                                    OutputConfig Config) {
  auto File =
      std::make_unique<OnDiskOutputFile>(ResolvedPath, Config, Settings);
  if (Error E = File->initializeFile())
    return std::move(E);
  return File;
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
OnDiskOutputBackend::createDirectoryImpl(StringRef ResolvedPath,
                                         OutputConfig Config) {
  bool IgnoreExisting = Config.getOverwrite();
  if (std::error_code EC =
          Config.getNoImplyCreateDirectories()
              ? sys::fs::create_directory(ResolvedPath, IgnoreExisting)
              : sys::fs::create_directories(ResolvedPath, IgnoreExisting))
    return errorCodeToError(EC);
  return getDirectoryImpl(ResolvedPath);
}

Expected<std::unique_ptr<OutputFile>>
OnDiskOutputBackend::createUniqueFileImpl(StringRef ResolvedModel,
                                          OutputConfig Config) {
  SmallString<128> Path;
  int FD = -1;
  {
    std::error_code EC = sys::fs::createUniqueFile(ResolvedModel, FD, Path);
    if (!EC)
      return std::make_unique<OnDiskOutputFile>(FD, Path, Config, Settings);

    if (EC != std::errc::no_such_file_or_directory ||
        Config.getNoImplyCreateDirectories())
      return errorCodeToError(EC);
  }

  // Create the parent path and try again.
  if (std::error_code EC =
          sys::fs::create_directories(sys::path::parent_path(ResolvedModel)))
    return errorCodeToError(EC);
  if (std::error_code EC = sys::fs::createUniqueFile(ResolvedModel, FD, Path))
    return errorCodeToError(EC);
  return std::make_unique<OnDiskOutputFile>(FD, Path, Config, Settings);
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
OnDiskOutputBackend::createUniqueDirectoryImpl(StringRef ResolvedPrefix,
                                               OutputConfig Config) {
  SmallString<128> Path;
  {
    std::error_code EC = sys::fs::createUniqueDirectory(ResolvedPrefix, Path);
    if (!EC)
      return getDirectoryImpl(Path);

    if (EC != std::errc::no_such_file_or_directory ||
        Config.getNoImplyCreateDirectories())
      return errorCodeToError(EC);
  }

  // Create the parent path and try again.
  if (std::error_code EC =
          sys::fs::create_directories(sys::path::parent_path(ResolvedPrefix)))
    return errorCodeToError(EC);
  if (std::error_code EC = sys::fs::createUniqueDirectory(ResolvedPrefix, Path))
    return errorCodeToError(EC);
  return getDirectoryImpl(Path);
}

namespace {
class InMemoryOutputDestination : public OutputFile {
public:
  Error storeContentBuffer(ContentBuffer &Content) override;

  InMemoryOutputDestination(StringRef Path,
                            IntrusiveRefCntPtr<InMemoryFileSystem> FS,
                            InMemoryOutputBackend::OutputSettings Settings)
      : OutputFile(Path), FS(std::move(FS)), Settings(Settings) {}

private:
  IntrusiveRefCntPtr<InMemoryFileSystem> FS;
  InMemoryOutputBackend::OutputSettings Settings;
};
} // anonymous namespace

Error InMemoryOutputDestination::storeContentBuffer(ContentBuffer &Content) {
  assert(Content.getBytes().end()[0] == 0 && "Expected null-terminated buffer");

  // Check the content of any existing buffer. We cannot rely on the logic in
  // InMemoryFileSystem::addFile, since Content and/or other backends could
  // have dangling references to the buffer and addFile will destroy it if it
  // can't be added.
  //
  // FIXME: Add an API to InMemoryFileSystem to get out an
  // Optional<MemoryBufferRef> to avoid an unnecessary malloc of MemoryBuffer.
  if (ErrorOr<std::unique_ptr<MemoryBuffer>> ExistingBuffer =
          FS->getBufferForFile(getPath()))
    return ExistingBuffer->get()->getBuffer() == Content.getBytes()
               ? Error::success()
               : createCannotOverwriteExistingOutputError(getPath());

  std::unique_ptr<MemoryBuffer> Buffer =
      Settings.CopyUnownedBuffers ? Content.takeOwnedBufferOrCopy(getPath())
                                  : Content.takeBuffer(getPath());
  bool WasAdded = FS->addFile(getPath(), 0, std::move(Buffer));
  assert(WasAdded && "Already checked this would work; what changed?");
  return Error::success();
}

Expected<std::unique_ptr<OutputFile>>
InMemoryOutputBackend::createFileImpl(StringRef ResolvedPath,
                                      OutputConfig Config) {
  if ((Settings.FailIfExists || Config.getNoOverwrite()) &&
      FS->exists(ResolvedPath))
    return createCannotOverwriteExistingOutputError(ResolvedPath);

  // FIXME: getNoOverwrite also needs to check for in-flight outputs.

  return std::make_unique<InMemoryOutputDestination>(ResolvedPath, FS,
                                                     Settings);
}

namespace {
class MirroringOutputBackend : public OutputBackend {
  class File;

public:
  Expected<StringRef>
  resolveOutputPath(const Twine &Path,
                    SmallVectorImpl<char> &PathStorage) const override {
    return Backend1->resolveOutputPath(Path, PathStorage);
  }

  Expected<std::unique_ptr<OutputFile>>
  createFileImpl(StringRef ResolvedPath, OutputConfig Config) override;

  Expected<std::unique_ptr<OutputFile>>
  createUniqueFileImpl(StringRef ResolvedModel, OutputConfig Config) override;

  Expected<IntrusiveRefCntPtr<OutputDirectory>>
  createDirectoryImpl(StringRef ResolvedPath, OutputConfig Config) override;

  Expected<IntrusiveRefCntPtr<OutputDirectory>>
  createUniqueDirectoryImpl(StringRef ResolvedModel,
                            OutputConfig Config) override;

  MirroringOutputBackend(IntrusiveRefCntPtr<OutputBackend> Backend1,
                         IntrusiveRefCntPtr<OutputBackend> Backend2)
      : OutputBackend(Backend1->getPathStyle()), Backend1(std::move(Backend1)),
        Backend2(std::move(Backend2)) {
    assert(this->Backend2->getPathStyle() == getPathStyle() &&
           "Path style needs to match between mirrored backends");
  }

private:
  IntrusiveRefCntPtr<OutputBackend> Backend1;
  IntrusiveRefCntPtr<OutputBackend> Backend2;
};
} // anonymous namespace

class MirroringOutputBackend::File : public OutputFile {
  Error storeContentBuffer(ContentBuffer &Content) override {
    if (Error E = storeContentBufferIn(Content, *File1))
      return E;
    return File2->storeContentBufferIn(Content, *File2);
  }

public:
  File(std::unique_ptr<OutputFile> File1, std::unique_ptr<OutputFile> File2)
      : OutputFile(File1->getPath()), File1(std::move(File1)),
        File2(std::move(File2)) {}

  static std::unique_ptr<OutputFile> create(std::unique_ptr<OutputFile> File1,
                                            std::unique_ptr<OutputFile> File2) {
    if (isNull(*File1))
      return File2;
    if (isNull(*File2))
      return File1;
    return std::make_unique<File>(std::move(File1), std::move(File2));
  }

  std::unique_ptr<OutputFile> File1;
  std::unique_ptr<OutputFile> File2;
};

IntrusiveRefCntPtr<OutputBackend> llvm::vfs::makeMirroringOutputBackend(
    IntrusiveRefCntPtr<OutputBackend> Backend1,
    IntrusiveRefCntPtr<OutputBackend> Backend2) {
  return std::make_unique<MirroringOutputBackend>(std::move(Backend1),
                                                  std::move(Backend2));
}

Expected<std::unique_ptr<OutputFile>>
MirroringOutputBackend::createFileImpl(StringRef ResolvedPath,
                                       OutputConfig Config) {
  // Create destinations for each backend.
  SmallVector<std::unique_ptr<OutputFile>, 2> Files;
  for (OutputBackend *B : {&*Backend1, &*Backend2}) {
    Expected<std::unique_ptr<OutputFile>> File =
        B->createFile(ResolvedPath, Config);
    if (!File)
      return File.takeError();
    Files.push_back(std::move(*File));
  }

  return File::create(std::move(Files[0]), std::move(Files[1]));
}

Expected<std::unique_ptr<OutputFile>>
MirroringOutputBackend::createUniqueFileImpl(StringRef ResolvedModel,
                                             OutputConfig Config) {
  // Create a unique file in the first backend.
  Expected<std::unique_ptr<OutputFile>> ExpectedFile =
      Backend1->createUniqueFile(ResolvedModel, Config);
  if (!ExpectedFile)
    return ExpectedFile.takeError();
  std::unique_ptr<OutputFile> File = std::move(*ExpectedFile);

  // Create the same file in the second backend.
  Config.setNoOverwrite();
  ExpectedFile = Backend2->createFile(File->getPath(), Config);
  if (!ExpectedFile)
    return ExpectedFile.takeError();
  return File::create(std::move(File), std::move(*ExpectedFile));
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
MirroringOutputBackend::createDirectoryImpl(StringRef ResolvedPath,
                                            OutputConfig Config) {
  // Create directories for each backend in order to check for errors.
  for (OutputBackend *B : {&*Backend1, &*Backend2}) {
    Expected<IntrusiveRefCntPtr<OutputDirectory>> Dir =
        B->createDirectory(ResolvedPath, Config);
    if (!Dir)
      return Dir.takeError();
  }

  // Return an output directory for this backend.
  return makeIntrusiveRefCnt<OutputDirectory>(this, ResolvedPath);
}

Expected<IntrusiveRefCntPtr<OutputDirectory>>
MirroringOutputBackend::createUniqueDirectoryImpl(StringRef ResolvedModel,
                                                  OutputConfig Config) {
  // Create a unique directory in the first backend.
  Expected<IntrusiveRefCntPtr<OutputDirectory>> ExpectedDir =
      Backend1->createUniqueDirectory(ResolvedModel, Config);
  if (!ExpectedDir)
    return ExpectedDir.takeError();
  IntrusiveRefCntPtr<OutputDirectory> Dir = std::move(*ExpectedDir);

  // Create the same directory in the second backend.
  Config.setNoOverwrite();
  ExpectedDir = Backend2->createDirectory(Dir->getPath(), Config);
  if (!ExpectedDir)
    return ExpectedDir.takeError();
  return makeIntrusiveRefCnt<OutputDirectory>(this, Dir->getPath());
}

namespace {
class FilteringOutputBackend : public ProxyOutputBackend {
public:
  Expected<std::unique_ptr<OutputFile>>
  createFileImpl(StringRef ResolvedPath, OutputConfig Config) override {
    if (!Filter(ResolvedPath, Config))
      return makeNullOutput(ResolvedPath);
    return ProxyOutputBackend::createFileImpl(ResolvedPath, Config);
  }

  using FilterType = unique_function<bool(StringRef, OutputConfig)>;
  FilteringOutputBackend(IntrusiveRefCntPtr<OutputBackend> UnderlyingBackend,
                         FilterType Filter)
      : ProxyOutputBackend(std::move(UnderlyingBackend)),
        Filter(std::move(Filter)) {}

private:
  FilterType Filter;
};
} // anonymous namespace

IntrusiveRefCntPtr<OutputBackend> llvm::vfs::makeFilteringOutputBackend(
    IntrusiveRefCntPtr<OutputBackend> UnderlyingBackend,
    FilteringOutputBackend::FilterType Filter) {
  return std::make_unique<FilteringOutputBackend>(std::move(UnderlyingBackend),
                                                  std::move(Filter));
}
