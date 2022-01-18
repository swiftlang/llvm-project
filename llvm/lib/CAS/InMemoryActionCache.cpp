//===- InMemoryActionCache.cpp ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/HashMappedTrie.h"

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
  Error getWithLifetimeImpl(ActionDescription &Action,
                            Optional<UniqueIDRef> &Result) override;
  Error putImpl(const ActionDescription &Action, UniqueIDRef Result) override;
  InMemoryActionCache(const Namespace &NS) : ActionCache(NS) {}

private:
  /// Cache results (currently storing \a UniqueID) based on the hash of the
  /// action that generated them, indicated by \a ActionHashT.
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
  using ResultCacheT = ThreadSafeHashMappedTrie<UniqueID, ActionHashT>;

  /// Pull out the value type for convenience.
  using ResultCacheValueT = ResultCacheT::HashedDataType;

  /// Map from ActionHashT to the result hashes.
  ResultCacheT Results;
};

} // end namespace

Error InMemoryActionCache::getWithLifetimeImpl(ActionDescription &Action,
                                               Optional<UniqueIDRef> &Result) {
  assert(!Result && "Expected result to be reset before entry");
  ActionHashT ActionHash;
  if (Optional<ArrayRef<uint8_t>> CachedHash = getCachedHash(Action))
    llvm::copy(*CachedHash, ActionHash.begin());
  else
    cacheHash(Action, ActionHash = Action.computeHash<ActionHasherT>());

  if (auto Lookup = Results.lookup(ActionHash))
    Result = Lookup->Data;

  // A cache miss is not an error.
  return Error::success();
}

Error InMemoryActionCache::putImpl(
    const ActionDescription &Action, UniqueIDRef Result) {
  ActionHashT ActionHash;
  if (Optional<ArrayRef<uint8_t>> CachedHash = getCachedHash(Action))
    llvm::copy(*CachedHash, ActionHash.begin());
  else
    ActionHash = Action.computeHash<ActionHasherT>();

  // Do a lookup before inserting to avoid allocating when result is present.
  //
  // FIXME: Update trie to delay allocation until *after* finding the slot.
  Optional<UniqueIDRef> StoredResult;
  auto Lookup = Results.lookup(ActionHash);
  if (Lookup)
    StoredResult = Lookup->Data;
  else
    StoredResult =
        Results.insert(Lookup, ResultCacheValueT(ActionHash, UniqueID(Result)))
            .Data;

  // Check for collision.
  if (StoredResult != Result)
    return make_error<ActionCacheCollisionError>(*StoredResult, Action, Result);
  return Error::success();
}

std::unique_ptr<ActionCache>
cas::createInMemoryActionCache(const Namespace &NS) {
  return std::make_unique<InMemoryActionCache>(NS);
}
