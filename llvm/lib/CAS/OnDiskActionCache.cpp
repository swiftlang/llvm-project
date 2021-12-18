//===- OnDiskActionCache.cpp ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/HashMappedTrie.h"
#include "llvm/CAS/NamespaceHelpers.h"
#include "llvm/CAS/OnDiskHashMappedTrie.h"
#include "llvm/CAS/ThreadSafeAllocator.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include "llvm/Support/StringSaver.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

/// The hasher to use for actions. This need not match the hasher for the
/// Namespace.
///
/// FIXME: Update to BLAKE3 at some point and use a subset of the bits (since
/// 32B is overkill). That's faster than SHA1. Note that for in-memory use,
/// collisions seem exceedingly unlikely, so even the 20B in SHA1 are probably
/// overkill.
using ActionHasherT = SHA1;

static constexpr size_t ActionHashNumBytes = sizeof(ActionHasherT::hash(None));
using ActionHashT = std::array<uint8_t, ActionHashNumBytes>;

/// This is an on-disk action cache, acting as a map from \a ActionDescription
/// to \a UniqueIDRef.
///
/// The current implementation uses a strong hash (\a SHA1) of the serialized
/// \a ActionDescription as a key for an \a OnDiskHashMappedTrie stored at the
/// given path.
///
/// FIXME: We should have multiple variants, with different options for the
/// underlying storage. Another obvious one is a directory with one file per
/// action, files named by the action hash.
///
/// FIXME: Consider storing the serialized action somewhere instead of just the
/// hash.
class OnDiskActionCache : public ActionCache {
public:
  Error getImpl(ActionDescription &Action, Optional<UniqueIDRef> &Result) override;
  Error putImpl(const ActionDescription &Action, UniqueIDRef Result) override;

  OnDiskActionCache(StringRef Path, std::shared_ptr<OnDiskHashMappedTrie> Results)
      : Path(Path.str()), Results(std::move(Results)) {}

private:
  using MappedContentReference = OnDiskHashMappedTrie::MappedContentReference;

  /// Path to file. Not strictly needed, but useful for debugging.
  std::string Path;

  /// On-disk cache.
  std::shared_ptr<OnDiskHashMappedTrie> Results;
};

} // end namespace

Expected<CASID> BuiltinCAS::getCachedResult(CASID InputID) {
  auto Lookup = ResultCache.lookup(InputID.getHash());
  if (Lookup)
    return CASID(Lookup->Data);

  // FIXME: Odd to have an error for a cache miss. Maybe this function should
  // just return Optional.
  if (isInMemoryOnly())
    return createResultCacheMissError(InputID);

  Optional<MappedContentReference> File =
      openOnDisk(*OnDiskResults, InputID.getHash());
  if (!File)
    return createResultCacheMissError(InputID);
  if (File->Metadata != "results") // FIXME: Should cache this object.
    return createCorruptObjectError(InputID);

  // Drop File.Map on the floor since we're storing a std::string.
  if (File->Data.size() != NumHashBytes)
    reportAsFatalIfError(createResultCacheCorruptError(InputID));
  HashRef OutputHash = bufferAsRawHash(File->Data);
  return CASID(ResultCache
                   .insert(Lookup, ResultCacheType::HashedDataType(
                                       makeHash(InputID), makeHash(OutputHash)))
                   .Data);
}

Error BuiltinCAS::putCachedResult(CASID InputID, CASID OutputID) {
  auto Lookup = ResultCache.lookup(InputID.getHash());

  auto checkOutput = [&](HashRef ExistingOutput) -> Error {
    if (ExistingOutput == OutputID.getHash())
      return Error::success();
    // FIXME: probably...
    reportAsFatalIfError(createResultCachePoisonedError(InputID, OutputID,
                                                        CASID(ExistingOutput)));
    return Error::success();
  };
  if (Lookup)
    return checkOutput(Lookup->Data);

  if (!isInMemoryOnly()) {
    // Store on-disk and check any existing data matches.
    HashRef OutputHash = OutputID.getHash();
    MappedContentReference File = OnDiskResults->insert(
        InputID.getHash(), "results", rawHashAsBuffer(OutputHash));
    if (Error E = checkOutput(bufferAsRawHash(File.Data)))
      return E;
  }

  // Store in-memory.
  return checkOutput(
      ResultCache
          .insert(Lookup, ResultCacheType::HashedDataType(makeHash(InputID),
                                                          makeHash(OutputID)))
          .Data);
}

Optional<BuiltinCAS::MappedContentReference>
BuiltinCAS::openOnDisk(OnDiskHashMappedTrie &Trie, HashRef Hash) {
  if (auto Lookup = Trie.lookup(Hash))
    return std::move(Lookup.get());
  return None;
}

Expected<std::unique_ptr<MemoryBuffer>> BuiltinCAS::openFile(StringRef Path) {
  assert(!isInMemoryOnly());
  return errorOrToExpected(llvm::MemoryBuffer::getFile(Path));
}

Expected<std::unique_ptr<MemoryBuffer>>
BuiltinCAS::openFileWithID(StringRef BaseDir, CASID ID) {
  assert(!isInMemoryOnly());
  SmallString<256> Path = StringRef(*RootPath);
  sys::path::append(Path, BaseDir);

  SmallString<64> PrintableHash;
  extractPrintableHash(ID, PrintableHash);
  sys::path::append(Path, PrintableHash);

  Expected<std::unique_ptr<MemoryBuffer>> Buffer = openFile(Path);
  if (Buffer)
    return Buffer;

  // FIXME: Should we sometimes return the error directly?
  consumeError(Buffer.takeError());
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "unknown blob '" + PrintableHash + "'");
}

StringRef BuiltinCAS::getPathForID(StringRef BaseDir, CASID ID,
                                   SmallVectorImpl<char> &Storage) {
  Storage.assign(RootPath->begin(), RootPath->end());
  sys::path::append(Storage, BaseDir);

  SmallString<64> PrintableHash;
  extractPrintableHash(ID, PrintableHash);
  sys::path::append(Storage, PrintableHash);

  return StringRef(Storage.begin(), Storage.size());
}

/// Need to check that the namespace is the same so the same action cache isn't
/// used for two incompatible CAS instances (where the \a UniqueID has a
/// different size and/or meaning).
///
/// FIXME: This sanity check should be stored inside an existing file but
/// OnDiskHashMappedTrie doesn't currently have support for that. This function
/// relies on knowing that the on-disk trie is actually a directory...
static Error createOrCheckNamespaceFile(const Namespace &NS, StringRef TriePath) {
  SmallString<256> NamespaceFilePath = TriePath;
  sys::path::append(NamespaceFilePath, "namespace");

  StringRef Name = NS.getName();

  // FIXME: Check for create directories error and retry.
  sys::fs::file_t FD;
  if (Error E = sys::fs::openNativeFileForReadWrite(
          NamespaceFilePath, sys::fs::CD_OpenAlways, sys::fs::OF_None))
    return E;
  auto Close = make_scope_exit([&]() { sys::fs::close(FD); });

  // Lock the file so we can initialize it.
  if (std::error_code EC = sys::fs::lockFile(*FD)) {
    sys::fs::closeFile(FD);
    return createFileError(Path, EC);
  }
  auto Unlock = make_scope_exit([&]() { sys::fs::unlockFile(FD); });

  sys::fs::file_status Status;
  if (std::error_code EC = sys::fs::status(*FD, Status))
    return errorCodeToError(EC);

  uint64_t FileSize = Status.getSize();
  if (!FileSize) {
    raw_fd_ostream(FD, /*shouldClose=*/false, /*unbuffered=*/true) << Name;
    return Error::success();
  }

  /// FIXME: Only read up to Name.size().
  SmallString<1024> ExistingContent;
  if (Error E = sys::fs::readNativeFileToEOF(FD, ExistingContent, /*ChunkSize=*/Name.size()))
    return E;
  if (ExistingContent != Name)
    return createStringError(...);
  return Error::success();
}

Expected<std::unique_ptr<ActionCache>> cas::createOnDiskActionCache(
    const Namespace &NS, const Twine &Path) {
  SmallString<256> AbsPath;
  Path.toVector(AbsPath);
  sys::fs::make_absolute(AbsPath);

  if (AbsPath == getDefaultOnDiskActionCacheStableID()) {
    AbsPath = StringRef(getDefaultOnDiskActionCachePath());
    sys::path::append(NS.getName());
  }

  // FIXME: Only call create_directories() as a fallback. Can do this in
  // createOrCheckNamespaceFile().
  if (std::error_code EC = sys::fs::create_directories(AbsPath))
    return createFileError(AbsPath, EC);

  if (Error E = createOrCheckNamespaceFile(AbsPath))
    return std::move(E);

  std::shared_ptr<OnDiskHashMappedTrie> Results;
  constexpr uint64_t GB = 1024ull * 1024ull * 1024ull;
  if (Error E = OnDiskHashMappedTrie::create(
          AbsPath, NumHashBytes * sizeof(uint8_t), GB).moveInto(Results))
    return std::move(E);

  return std::make_unique<ActionCache>(AbsPath, std::move(OnDiskObjects),
                                      std::move(OnDiskResults));
}

// FIXME: Proxy not portable. Maybe also error-prone?
constexpr StringLiteral DefaultDirProxy = "/^llvm::cas::actions::default";
constexpr StringLiteral DefaultName = "llvm.cas.actions.default";

void cas::getDefaultOnDiskActionCachePath(SmallVectorImpl<char> &Path) {
  if (!llvm::sys::path::cache_directory(Path))
    report_fatal_error("cannot get default cache directory");
  llvm::sys::path::append(Path, DefaultName);
}

std::string cas::getDefaultOnDiskActionCachePath() {
  SmallString<128> Path;
  getDefaultOnDiskActionCachePath(Path);
  return Path.str().str();
}

void cas::getDefaultOnDiskActionCacheStableID(SmallVectorImpl<char> &Path) {
  Path.assign(DefaultDirProxy.begin(), DefaultDirProxy.end());
  llvm::sys::path::append(Path, DefaultName);
}

std::string cas::getDefaultOnDiskActionCacheStableID() {
  SmallString<128> Path;
  getDefaultOnDiskActionCacheStableID(Path);
  return Path.str().str();
}
