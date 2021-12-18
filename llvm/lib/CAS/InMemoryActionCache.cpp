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

/// This is an in-memory action cache, acting as a map from \a
/// ActionDescription to \a UniqueIDRef.
///
/// The current implementation uses a strong hash (\a SHA1) of the serialized
/// \a ActionDescription as a key for a \a ThreadSafeHashMappedTrie.
///
/// FIXME: We should have multiple variants, with different options for the
/// underlying storage. Another obvious one is StringMap + BumpPtrAllocator,
/// which guards every put() or get() by locking a \a std::mutex. The \a
/// ActionDescription could be serialized and the string used as the key. For
/// better multi-threading performance, the map (and allocator) could
/// optionally be sharded.
class InMemoryActionCache final : public ActionCache {
public:
  Error getImpl(ActionDescription &Action, Optional<UniqueIDRef> &Result) override;
  Error putImpl(const ActionDescription &Action, UniqueIDRef Result) override;
  InMemoryActionCache(const Namespace &NS) : ActionCache(NS) {}

private:
  /// Map from \a ActionHashT, which is a hash of the serialized \a
  /// ActionDescription, to the result.
  ///
  /// FIXME: Ideally, the stored data for the result ID would be \c
  /// std::array<uint8_t> (the bytes from \a UniqueID::getHash()). This avoids
  /// storing the \a Namespace pointer redundantly. However, the size of the
  /// hash depends on the \a Namespace and currently the allocation size is
  /// configured at compile-time via the template parameter.
  ///
  /// A possible solution is to change ThreadSafeHashMappedTrie to
  /// tail-allocate the stored data, with allocation size configured at
  /// runtime; probably not too hard.
  ///
  /// A rejected solution is using a BumpPtrAllocator here. The trie is already
  /// managing allocations for inserted content; we should improve those
  /// allocations, not add another layer.
  ///
  /// For now, store \a UniqueID. This wastes some space, but it's simple.
  using ResultCacheT = ThreadSafeHashMappedTrie<ActionHashT, UniqueID>;

  /// Pull out the value type for convenience.
  using ResultCacheValueT = ResultCacheT::HashedDataType;

  /// Map from ActionHashT to the result hashes.
  ResultCacheT Results;
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

static ActionHashT hashAction(const ActionDescription &Action) {
  ActionHasherT Hasher;
  Action.serialize([&Hasher](ArrayRef<uint8_t> Bytes) { Hasher.update(Bytes); });
  Storage.resize(ActionHashNumBytes);

  ActionHashT Hash;
  llvm::copy(Hasher.final(), Hash.begin());
  return Hash;
}

Error InMemoryActionCache::getImpl(
    ActionDescription &Action, Optional<UniqueIDRef> &Result) {
  assert(!Result && "Expected result to be reset be entry");
  ActionHashT Hash;
  if (Optional<ArrayRef<uint8_t>> CachedHash = getCachedHash(Action))
    llvm::copy(*CachedHash, Hash.begin());
  else
    cacheHash(Action, Hash = hashAction(Action));

  if (auto Lookup = Results.lookup(Hash))
    Result = Lookup->Data;

  // A cache miss is not an error.
  return Error::success();
}

Error InMemoryActionCache::putImpl(
    const ActionDescription &Action, UniqueIDRef Result) {
  // Ensure we're only storing hashes from a single namespace.
  assert(&getNamespace() == &Result.getNamespace() && "Mismatched namespace");

  // Check that the inputs are in the same namespace. Not strictly necessary
  // for correctness of the cache, but if this fails there's probably a bug...
  //
  // Note: Only check the first ID here. ActionDescription::serialize() asserts
  // that they all match. This must be called at some point to generate the
  // hash.
  assert((Action.getIDs().empty() ||
          &getNamespace() == Action.getIDs().front().getNamespace()) &&
         "Mismatched namespace");

  ActionHashT Hash;
  if (Optional<ArrayRef<uint8_t>> CachedHash = getCachedHash(Action))
    llvm::copy(*CachedHash, Hash.begin());
  else
    Hash = hashAction(Action);

  // Do a lookup before inserting to avoid allocating when result is present.
  //
  // FIXME: Update trie to delay allocation until *after* finding the slot.
  Optional<UniqueIDRef> StoredID;
  auto Lookup = Results.lookup(InputID.getHash());
  if (Lookup)
    StoredID = Lookup->Data;
  else
    StoredID = ResultCache.insert(Lookup, ResultCacheValueT(Hash, UniqueID(Result))).Data;

  // Check for a cache collision.
  if (StoredID != Result)
    return make_error<ActionCacheCollisionError>(*StoredID, Action, Result);
  return Error::success();
}

std::unique_ptr<CASDB> cas::createInMemoryActionCache(const Namespace &NS) {
  return std::make_unique<InMemoryActionCache>(NS);
}
