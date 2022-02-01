//===- DependencyScanningCASFilesystem.cpp - clang-scan-deps fs -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Tooling/DependencyScanning/DependencyScanningCASFilesystem.h"
#include "clang/Basic/Version.h"
#include "clang/Lex/DependencyDirectivesSourceMinimizer.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Threading.h"

using namespace clang;
using namespace tooling;
using namespace dependencies;

template <typename T>
static T reportAsFatalIfError(Expected<T> ValOrErr) {
  if (!ValOrErr)
    llvm::report_fatal_error(ValOrErr.takeError());
  return std::move(*ValOrErr);
}

static void reportAsFatalIfError(llvm::Error E) {
  if (E)
    llvm::report_fatal_error(std::move(E));
}

using llvm::Error;
namespace cas = llvm::cas;

DependencyScanningCASFilesystem::DependencyScanningCASFilesystem(
    DependencyScanningFilesystemSharedCache &SharedCache,
    IntrusiveRefCntPtr<llvm::cas::CachingOnDiskFileSystem> WorkerFS,
    ExcludedPreprocessorDirectiveSkipMapping *PPSkipMappings)
    : DependencyScanningWorkerFilesystem(SharedCache, WorkerFS, PPSkipMappings),
      FS(WorkerFS), Entries(EntryAlloc), CAS(WorkerFS->getCAS()) {}

DependencyScanningCASFilesystem::~DependencyScanningCASFilesystem() =
    default;

static void addSkippedRange(llvm::DenseMap<unsigned, unsigned> &Skip,
                            unsigned Offset, unsigned Length) {
  // Ignore small ranges as non-profitable.
  //
  // FIXME: This is a heuristic, its worth investigating the tradeoffs
  // when it should be applied.
  if (Length >= 16)
    Skip[Offset] = Length;
}

static Error cacheMinimized(cas::CASID InputID, cas::CASDB &CAS,
                            cas::CASID OutputDataID,
                            cas::CASID SkippedRangesID) {
  cas::HierarchicalTreeBuilder Builder;
  Builder.push(OutputDataID, cas::TreeEntry::Regular, "data");
  Builder.push(SkippedRangesID, cas::TreeEntry::Regular, "skipped-ranges");
  Expected<cas::CASID> OutputID = Builder.create(CAS);
  if (!OutputID)
    return OutputID.takeError();
  return CAS.putCachedResult(InputID, *OutputID);
}

Expected<StringRef> DependencyScanningCASFilesystem::getMinimized(
    cas::CASID OutputID, StringRef Identifier,
    Optional<cas::CASID> &MinimizedDataID) {
  // Extract the blob IDs from the tree.
  Expected<cas::TreeRef> Tree = CAS.getTree(OutputID);
  if (!Tree)
    return Tree.takeError();
  auto unwrapID =
      [](Optional<cas::NamedTreeEntry> Entry) -> Optional<cas::CASID> {
    if (Entry)
      return Entry->getID();
    return None;
  };
  Optional<cas::CASID> SkippedRangesID =
      unwrapID(Tree->lookup("skipped-ranges"));
  MinimizedDataID = unwrapID(Tree->lookup("data"));

  if (!MinimizedDataID)
    return createStringError(std::make_error_code(std::errc::invalid_argument),
                             Twine("missing 'data' in result of minimizing '") +
                                 Identifier + "'");
  if (!SkippedRangesID)
    return createStringError(
        std::make_error_code(std::errc::invalid_argument),
        Twine("missing 'skipped-ranges' in result of minimizing '") +
            Identifier + "'");

  StringRef OutputData = **expectedToOptional(CAS.getBlob(*MinimizedDataID));

  if (!PPSkipMappings)
    return OutputData;

  // Parse the skipped ranges.
  StringRef Ranges = **expectedToOptional(CAS.getBlob(*SkippedRangesID));
  if (Ranges.empty())
    return OutputData;

  PreprocessorSkippedRangeMapping Skip;
  while (!Ranges.empty()) {
    unsigned Offset, Length;
    if (Ranges.consumeInteger(10, Offset) || !Ranges.consume_front(" ") ||
        Ranges.consumeInteger(10, Length) || !Ranges.consume_front("\n"))
      return createStringError(
          std::make_error_code(std::errc::invalid_argument),
          "invalid skipped ranges '" + Identifier + "'");
    addSkippedRange(Skip, Offset, Length);
  }
  (*PPSkipMappings)[OutputData.begin()] = std::move(Skip);
  return OutputData;
}

Expected<StringRef> DependencyScanningCASFilesystem::computeMinimized(
    cas::CASID InputDataID, StringRef Identifier,
    Optional<llvm::cas::CASID> &MinimizedDataID) {
  using namespace llvm;
  using namespace llvm::cas;

  // Get a blob for the clang version string.
  if (!ClangFullVersionID)
    ClangFullVersionID =
        reportAsFatalIfError(CAS.createBlob(getClangFullVersion()));

  // Get a blob for the minimize command.
  if (!MinimizeID)
    MinimizeID = reportAsFatalIfError(CAS.createBlob("minimize"));

  // Get an empty blob.
  if (!EmptyBlobID)
    EmptyBlobID = reportAsFatalIfError(CAS.createBlob(""));

  // Construct a tree for the input.
  Optional<CASID> InputID;
  {
    HierarchicalTreeBuilder Builder;
    Builder.push(*ClangFullVersionID, TreeEntry::Regular, "version");
    Builder.push(*MinimizeID, TreeEntry::Regular, "command");
    Builder.push(InputDataID, TreeEntry::Regular, "data");
    InputID = reportAsFatalIfError(Builder.create(CAS));
  }

  // Check the result cache.
  if (Optional<CASID> OutputID =
          expectedToOptional(CAS.getCachedResult(*InputID))) {
    auto Ex = getMinimized(*OutputID, Identifier, MinimizedDataID);
    return reportAsFatalIfError(std::move(Ex));
  }

  StringRef InputData = *reportAsFatalIfError(CAS.getBlob(InputDataID));

  // Try to minimize.
  llvm::SmallString<1024> Buffer;
  SmallVector<minimize_source_to_dependency_directives::Token, 64> Tokens;
  if (minimizeSourceToDependencyDirectives(InputData, Buffer, Tokens)) {
    // Failure. Cache a self-mapping and return the input data unmodified.
    reportAsFatalIfError(cacheMinimized(*InputID, CAS, InputDataID, *EmptyBlobID));
    return InputData;
  }

  // Success. Add to the CAS and get back persistent output data.
  BlobRef Minimized = reportAsFatalIfError(CAS.createBlob(Buffer));
  MinimizedDataID = Minimized;
  StringRef OutputData = *Minimized;

  // Compute skipped ranges.
  llvm::SmallVector<minimize_source_to_dependency_directives::SkippedRange, 32>
      SkippedRanges;
  minimize_source_to_dependency_directives::computeSkippedRanges(Tokens,
                                                                 SkippedRanges);
  Optional<DenseMap<unsigned, unsigned>> SkipMappingsResults;
  Buffer.clear();
  if (!SkippedRanges.empty()) {
    if (PPSkipMappings)
      SkipMappingsResults.emplace();

    raw_svector_ostream OS(Buffer);
    for (const auto &Range : SkippedRanges) {
      OS << Range.Offset << " " << Range.Length << "\n";
      if (SkipMappingsResults)
        addSkippedRange(*SkipMappingsResults, Range.Offset, Range.Length);
    }
  }

  // Cache the computation.
  CASID SkippedRangesID = reportAsFatalIfError(CAS.createBlob(Buffer));
  reportAsFatalIfError(cacheMinimized(*InputID, CAS, *MinimizedDataID, SkippedRangesID));

  if (SkipMappingsResults)
    (*PPSkipMappings)[OutputData.begin()] = std::move(SkipMappingsResults);
  return OutputData;
}

Expected<StringRef>
DependencyScanningCASFilesystem::getOriginal(cas::CASID InputDataID) {
  Expected<cas::BlobRef> Blob = CAS.getBlob(InputDataID);
  if (Blob)
    return Blob->getData();
  return Blob.takeError();
}

/// Whitelist file extensions that should be minimized, treating no extension as
/// a source file that should be minimized.
///
/// This is kinda hacky, it would be better if we knew what kind of file Clang
/// was expecting instead.
static bool shouldMinimizeBasedOnExtension(StringRef Filename) {
  StringRef Ext = llvm::sys::path::extension(Filename);
  if (Ext.empty())
    return true; // C++ standard library
  return llvm::StringSwitch<bool>(Ext)
    .CasesLower(".c", ".cc", ".cpp", ".c++", ".cxx", true)
    .CasesLower(".h", ".hh", ".hpp", ".h++", ".hxx", true)
    .CasesLower(".m", ".mm", true)
    .CasesLower(".i", ".ii", ".mi", ".mmi", true)
    .CasesLower(".def", ".inc", true)
    .Default(false);
}

static bool shouldCacheStatFailures(StringRef Filename) {
  StringRef Ext = llvm::sys::path::extension(Filename);
  if (Ext.empty())
    return false; // This may be the module cache directory.
  return shouldMinimizeBasedOnExtension(
      Filename); // Only cache stat failures on source files.
}

void DependencyScanningCASFilesystem::disableMinimization(
    StringRef RawFilename) {
  llvm::SmallString<256> Filename;
  llvm::sys::path::native(RawFilename, Filename);
  NotToBeMinimized.insert(Filename);
}

bool DependencyScanningCASFilesystem::shouldMinimize(StringRef RawFilename) {
  if (!shouldMinimizeBasedOnExtension(RawFilename))
    return false;

  llvm::SmallString<256> Filename;
  llvm::sys::path::native(RawFilename, Filename);
  return !NotToBeMinimized.contains(Filename);
}

cas::CachingOnDiskFileSystem &
DependencyScanningCASFilesystem::getCachingFS() {
  return static_cast<cas::CachingOnDiskFileSystem &>(*FS);
}

DependencyScanningCASFilesystem::LookupPathResult
DependencyScanningCASFilesystem::lookupPath(const Twine &Path) {
  SmallString<256> PathStorage;
  StringRef PathRef = Path.toStringRef(PathStorage);

  {
    auto I = Entries.find(PathRef);
    if (I != Entries.end()) {
      // FIXME: Gross hack to ensure this file gets tracked as part of the
      // compilation. Instead, we should add an explicit hook somehow /
      // somewhere.
      (void)getCachingFS().status(PathRef);
      return LookupPathResult{&I->second, std::error_code()};
    }
  }

  Optional<cas::CASID> FileID;
  llvm::ErrorOr<llvm::vfs::Status> MaybeStatus =
      getCachingFS().statusAndFileID(PathRef, FileID);
  if (!MaybeStatus) {
    if (shouldCacheStatFailures(PathRef))
      Entries[PathRef].EC = MaybeStatus.getError();
    return LookupPathResult{nullptr, MaybeStatus.getError()};
  }

  // Underlying file system caches directories. No need to duplicate.
  if (MaybeStatus->isDirectory())
    return LookupPathResult{nullptr, std::move(MaybeStatus)};

  llvm::ErrorOr<StringRef> Buffer = std::error_code();
  llvm::Optional<llvm::cas::CASID> EffectiveID;
  if (shouldMinimize(PathRef)) {
    Buffer = expectedToErrorOr(computeMinimized(*FileID, PathRef, EffectiveID));
  } else {
    Buffer = expectedToErrorOr(getOriginal(*FileID));
    EffectiveID = *FileID;
  }

  auto &Entry = Entries[PathRef];
  if (!Buffer) {
    // Cache CAS failures. Not going to recover later.
    Entry.EC = Buffer.getError();
    return LookupPathResult{&Entry, std::error_code()};
  }
  assert(EffectiveID);

  Entry.Buffer = std::move(*Buffer);
  Entry.Status = llvm::vfs::Status(
      PathRef, MaybeStatus->getUniqueID(),
      MaybeStatus->getLastModificationTime(), MaybeStatus->getUser(),
      MaybeStatus->getGroup(), Entry.Buffer->size(), MaybeStatus->getType(),
      MaybeStatus->getPermissions());
  Entry.ID = EffectiveID;
  return LookupPathResult{&Entry, std::error_code()};
}

llvm::ErrorOr<llvm::vfs::Status>
DependencyScanningCASFilesystem::status(const Twine &Path) {
  LookupPathResult Result = lookupPath(Path);
  if (!Result.Entry)
    return std::move(Result.Status);
  if (Result.Entry->EC)
    return Result.Entry->EC;
  return Result.Entry->Status;
}

Optional<llvm::cas::CASID>
DependencyScanningCASFilesystem::getFileCASID(const Twine &Path) {
  LookupPathResult Result = lookupPath(Path);
  if (!Result.Entry)
    return None;
  if (Result.Entry->EC)
    return None;
  assert(Result.Entry->ID);
  return Result.Entry->ID;
}

IntrusiveRefCntPtr<llvm::cas::ThreadSafeFileSystem>
DependencyScanningCASFilesystem::createThreadSafeProxyFS() {
  llvm::report_fatal_error("not implemented");
}

namespace {

class MinimizedVFSFile final : public llvm::vfs::File {
public:
  MinimizedVFSFile(StringRef Buffer, llvm::vfs::Status Stat)
      : Buffer(Buffer), Stat(std::move(Stat)) {}

  llvm::ErrorOr<llvm::vfs::Status> status() override { return Stat; }

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>>
  getBuffer(const Twine &Name, int64_t FileSize, bool RequiresNullTerminator,
            bool IsVolatile) override {
    SmallString<256> Storage;
    return llvm::MemoryBuffer::getMemBuffer(Buffer, Name.toStringRef(Storage));
  }

  std::error_code close() override { return {}; }

private:
  StringRef Buffer;
  llvm::vfs::Status Stat;
};

} // end anonymous namespace

llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>>
DependencyScanningCASFilesystem::openFileForRead(const Twine &Path) {
  LookupPathResult Result = lookupPath(Path);
  if (!Result.Entry) {
    if (std::error_code EC = Result.Status.getError())
      return EC;
    assert(Result.Status->getType() ==
           llvm::sys::fs::file_type::directory_file);
    return std::make_error_code(std::errc::is_a_directory);
  }
  if (Result.Entry->EC)
    return Result.Entry->EC;

  return std::make_unique<MinimizedVFSFile>(*Result.Entry->Buffer,
                                            Result.Entry->Status);
}
