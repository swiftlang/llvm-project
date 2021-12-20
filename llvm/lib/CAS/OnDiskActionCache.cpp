//===- OnDiskActionCache.cpp ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ScopeExit.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/OnDiskHashMappedTrie.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

/// The hasher to use for actions. This need not match the hasher for the
/// Namespace.
///
/// FIXME: Update to BLAKE3 at some point, which is faster and stronger than
/// SHA1.
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
  Error getWithLifetimeImpl(ActionDescription &Action,
                            Optional<UniqueIDRef> &Result) override;
  Error putImpl(const ActionDescription &Action, UniqueIDRef Result) override;

  OnDiskActionCache(const Namespace &NS, StringRef Path,
                    std::shared_ptr<OnDiskHashMappedTrie> Results)
      : ActionCache(NS), Path(Path.str()), Results(std::move(Results)) {}

private:
  using MappedContentReference = OnDiskHashMappedTrie::MappedContentReference;

  /// Path to file. Not strictly needed, but useful for debugging.
  std::string Path;

  /// On-disk cache.
  std::shared_ptr<OnDiskHashMappedTrie> Results;
};

} // end namespace

static ArrayRef<uint8_t> toArrayRef(StringRef Data) {
  return makeArrayRef(reinterpret_cast<const uint8_t *>(Data.begin()),
                      reinterpret_cast<const uint8_t *>(Data.end()));
}

Error OnDiskActionCache::getWithLifetimeImpl(ActionDescription &Action,
                                             Optional<UniqueIDRef> &Result) {
  assert(!Result && "Expected result to be reset before entry");
  ActionHashT ActionHash;
  if (Optional<ArrayRef<uint8_t>> CachedHash = getCachedHash(Action))
    llvm::copy(*CachedHash, ActionHash.begin());
  else
    cacheHash(Action, ActionHash = Action.computeHash<ActionHasherT>());

  Optional<MappedContentReference> File =
      openOnDisk(*OnDiskResults, ActionHash);
  if (!File)
    return Error::success(); // Cache miss is not an error.

  // Check for corruption.
  ArrayRef<uint8_t> ResultHash = toArrayRef(File->Data);
  if (File->Metadata != "results" ||
      ResultHash.size() != getNamespace().getHashSize())
    return make_error<CorruptActionCacheResultError>(Action);

  // Found.
  Result.emplace(getNamespace(), ResultHash);
  return Error::success();
}

Error OnDiskActionCache::putImpl(const ActionDescription &Action,
                                 const UniqueIDRef &Result) {
  ActionHashT ActionHash;
  if (Optional<ArrayRef<uint8_t>> CachedHash = getCachedHash(Action))
    llvm::copy(*CachedHash, ActionHash.begin());
  else
    ActionHash = Action.computeHash<ActionHasherT>();

  // Insert and check for corruption of existing result.
  StringRef ResultData(reinterpret_cast<const char *>(Result.getHash().begin()),
                       reinterpret_cast<const char *>(Result.getHash().end()));
  MappedContentReference File =
      OnDiskResults->insert(ActionHash, "results", ResultData);
  if (File->Metadata != "results" ||
      File->Data.size() != getNamespace().getHashSize())
    return make_error<CorruptActionCacheResultError>(Action);

  // Check for collision.
  UniqueIDRef StoredResult(getNamespace(), toArrayRef(File->Data));
  if (StoredResult != Result)
    return make_error<ActionCacheCollisionError>(StoredResult, Action, Result);
  return Error::success();
}

/// Need to check that the namespace is the same so the same action cache isn't
/// used for two incompatible CAS instances (where the \a UniqueID has a
/// different size and/or meaning).
///
/// FIXME: This sanity check should be stored inside an existing file but
/// OnDiskHashMappedTrie doesn't currently have support for top-level metadata
/// like that (see FIXME there). This function relies on knowing that the
/// on-disk trie is actually a directory...
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
    return createFileError(NamespaceFilePath, EC);
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

  /// Limit how much of the file to read, in case it's very large.
  SmallString<1024> ExistingContent;
  const size_t SizeLimit = Name.size() + 10;
  if (Error E = sys::fs::readNativeFileToLimit(FD, ExistingContent, SizeLimit))
    return createFileError(E, NamespaceFilePath);
  if (ExistingContent != Name)
    return createFileError(TriePath,
                           make_error<WrongActionCacheNamespaceError>(
                               NS, ExistingContent.size() == SizeLimit
                                       ? ExistingContent.drop_back() + "..."
                                       : ExistingContent));
  return Error::success();
}

Expected<std::unique_ptr<ActionCache>> cas::createOnDiskActionCache(
    const Namespace &NS, const Twine &Path) {
  SmallString<256> AbsPath;
  Path.toVector(AbsPath);
  sys::fs::make_absolute(AbsPath);

  if (AbsPath == getDefaultOnDiskActionCacheStableID())
    AbsPath = StringRef(getDefaultOnDiskActionCachePath());

  // Append a subdirectory for CAS namespace if using the default path.
  //
  // FIXME: This enables using a default action cache path location with a
  // variety of CAS implementations (could use a non-default CAS). Is that
  // important/useful?
  if (AbsPath == getDefaultOnDiskActionCachePath())
    sys::path::append(NS.getName());

  // FIXME: Only call create_directories() as a fallback.
  if (std::error_code EC = sys::fs::create_directories(AbsPath))
    return createFileError(AbsPath, EC);

  // FIXME: Sink into (new) validation support in OnDiskHashMappedTrie.
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
constexpr StringLiteral DefaultDirProxy =
    "/^llvm::cas::builtin::default::cache";
constexpr StringLiteral DefaultName = "llvm.cas.builtin.default.cache";

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
