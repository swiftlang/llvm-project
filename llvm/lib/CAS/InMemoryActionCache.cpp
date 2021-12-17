//===- InMemoryActionCache.cpp ----------------------------------*- C++ -*-===//
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

static constexpr size_t NumHashBytes = sizeof(SHA1::hash(None));
using DescriptionHashType = std::array<uint8_t, NumHashBytes>;
using DescriptionHashRef = ArrayRef<uint8_t>;

struct HashInfo {
  template <class HasherT> static auto hash(DescriptionHashType Data) { return Data; }
  template <class HasherT> using HashType = decltype(hash<HasherT>(DescriptionHashType()));
};

class InMemoryActionCache final : public ActionCache {
public:
  Error getImpl(ActionDescription &Action, Optional<UniqueIDRef> &Result) override;
  Error putImpl(const ActionDescription &Action, UniqueIDRef Result) override;
  InMemoryActionCache(const Namespace &NS) : ActionCache(NS) {}

private:
  /// Map from DescriptionHashType to the result hash, owned by \a ResultIDs.
  ThreadSafeHashMappedTrie<DescriptionHashType, const uint8_t *>;
  ResultCacheType Results;
  Optional<ThreadSafeAllocator<SpecificBumpPtrAllocator<uint8_t>>>
      ResultHashes;
};

} // end namespace

static Error createResultCachePoisonedError(CASID InputID, CASID OutputID,
                                            CASID ExistingOutputID) {
  SmallString<64> InputHash, OutputHash, ExistingOutputHash;
  extractPrintableHash(InputID, InputHash);
  extractPrintableHash(OutputID, OutputHash);
  extractPrintableHash(ExistingOutputID, ExistingOutputHash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "cache poisoned for '" + InputHash + "' (new='" +
                               OutputHash + "' vs. existing '" +
                               ExistingOutputHash + "')");
}

static Error createResultCacheCorruptError(CASID InputID) {
  SmallString<64> InputHash;
  extractPrintableHash(InputID, InputHash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "result cache corrupt for '" + InputHash + "'");
}

static ArrayRef<uint8_t> hashAction(const Namespace &NS,
                                    const ActionDescription &Action,
                                    SmallVectorImpl<uint8_t> &Storage) {
  SHA1 Hasher;
  Hasher.update(NS.getHashSize());
  Hasher.update(Action.getIDs().size());
  Hasher.update(Action.getData().size());
  for (UniqueIDRef ID : Action.getIDs())
    Hasher.update(ID.getHash());
  Hasher.update(Action.getData());

  Storage.resize(NumHashBytes);
  llvm::copy(Hasher.final(), Storage.begin());
  return makeArrayRef(Storage);
}

Error InMemoryActionCache::getImpl(
    ActionDescription &Action, Optional<UniqueIDRef> &Result) {
  assert(!Result && "Expected result to be reset be entry");

  SmallVector<uint8_t, 32> HashStorage;
  Optional<ArrayRef<uint8_t>> Hash = getCachedHash(Action);
  if (!Hash) {
    Hash = hashAction(getNamespace(), Action, HashStorage);
    cacheHash(Action, Hash);
  }

  if (auto Lookup = ResultCache.lookup(*Hash))
    Result = UniqueIDRef(getNamespace(),
                         ArrayRef<uint8_t>(Lookup->Data,
                                           Lookup->Data + getNamespace().getHashSize()));

  // A cache miss is not an error.
  return Error::success();
}

Error InMemoryActionCache::putImpl(
    const ActionDescription &Action, UniqueIDRef Result) {
  SmallVector<uint8_t, 32> HashStorage;
  Optional<ArrayRef<uint8_t>> Hash = getCachedHash(Action);
  if (!Hash)
    Hash = hashAction(getNamespace(), Action, HashStorage);


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

Expected<std::unique_ptr<CASDB>> cas::createOnDiskCAS(const Twine &Path) {
  SmallString<256> AbsPath;
  Path.toVector(AbsPath);
  sys::fs::make_absolute(AbsPath);

  if (AbsPath == getDefaultOnDiskCASStableID())
    AbsPath = StringRef(getDefaultOnDiskCASPath());

  if (std::error_code EC = sys::fs::create_directories(AbsPath)) {
    return createFileError(AbsPath, EC);
  }

  std::shared_ptr<OnDiskHashMappedTrie> OnDiskObjects;
  std::shared_ptr<OnDiskHashMappedTrie> OnDiskResults;

  uint64_t GB = 1024ull * 1024ull * 1024ull;
  if (auto Expected = OnDiskHashMappedTrie::create(
          AbsPath.str() + "/objects", NumHashBytes * sizeof(uint8_t), 16 * GB))
    OnDiskObjects = std::move(*Expected);
  else
    return Expected.takeError();
  if (auto Expected = OnDiskHashMappedTrie::create(
          AbsPath.str() + "/results", NumHashBytes * sizeof(uint8_t), GB))
    OnDiskResults = std::move(*Expected);
  else
    return Expected.takeError();

  return std::make_unique<BuiltinCAS>(AbsPath, std::move(OnDiskObjects),
                                      std::move(OnDiskResults));
}

std::unique_ptr<CASDB> cas::createInMemoryCAS() {
  return std::make_unique<BuiltinCAS>(BuiltinCAS::InMemoryOnlyTag());
}

// FIXME: Proxy not portable. Maybe also error-prone?
constexpr StringLiteral DefaultDirProxy = "/^llvm::cas::builtin::default";
constexpr StringLiteral DefaultName = "llvm.cas.builtin.default";

void cas::getDefaultOnDiskCASPath(SmallVectorImpl<char> &Path) {
  if (!llvm::sys::path::cache_directory(Path))
    report_fatal_error("cannot get default cache directory");
  llvm::sys::path::append(Path, DefaultName);
}

std::string cas::getDefaultOnDiskCASPath() {
  SmallString<128> Path;
  getDefaultOnDiskCASPath(Path);
  return Path.str().str();
}

void cas::getDefaultOnDiskCASStableID(SmallVectorImpl<char> &Path) {
  Path.assign(DefaultDirProxy.begin(), DefaultDirProxy.end());
  llvm::sys::path::append(Path, DefaultName);
}

std::string cas::getDefaultOnDiskCASStableID() {
  SmallString<128> Path;
  getDefaultOnDiskCASStableID(Path);
  return Path.str().str();
}
