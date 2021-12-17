//===- llvm/CAS/ActionCache.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_ACTIONCACHE_H
#define LLVM_CAS_ACTIONCACHE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CAS/Namespace.h"
#include "llvm/CAS/UniqueID.h"
#include "llvm/Support/Error.h"
#include <cstddef>

namespace llvm {
namespace cas {

class ActionCache;

/// Structure for describing an action and its inputs.
class ActionDescription {
public:
  ActionDescription() = delete;

  explicit ActionDescription(StringRef Data, ArrayRef<UniqueIDRef> IDs = None)
      : Data(Data), IDs(IDs) {
    assert(!Data.empty());
  }

  explicit ActionDescription(StringRef Data, UniqueIDRef ID)
      : ActionDescription(Data, makeArrayRef(ID)) {}

  StringRef getData() const { return Data; }
  ArrayRef<UniqueIDRef> getIDs() const { return IDs; }

private:
  StringRef Data;
  ArrayRef<UniqueIDRef> IDs;

  friend class ActionCache;
  enum : size_t { MaxCachedHashSize = 32 };

  /// A cache for the hash of this action. See \a ActionCache::cacheHash() and
  /// \a ActionCache::getCachedHash().
  ///
  /// FIXME: Should this just be UniqueID? Or is it useful to allow (e.g.) an
  /// in-memory ActionCache to use a weak hash and compare the description for
  /// equality?
  struct {
    const ActionCache *Creator = nullptr;
    uint8_t Bytes[MaxCachedHashSize] = {0};
    size_t Size = 0;
  } CachedHash;
};

/// A cache of actions and results, mapping from an arbitrary StringRef to
/// UniqueID.
///
/// Belongs to a specific \a Namespace. Must be in the same namespace as the
/// IDs sent to \a put().
class ActionCache {
  virtual void anchor();

public:
  /// Put an action in the cache, such that \p Action maps to \p Result. Care
  /// should be taken that the action always produces the same result by
  /// including all inputs (e.g., a UniqueID for the clang binary's blob should
  /// be included if caching something in the Clang compiler).
  ///
  /// Thread-safe.
  ///
  /// \return Error if there's a collision or some problem storing the result.
  /// \pre \c Result.getNamespace() is the same as \a getNamespace().
  /// \post Subsequent call to \a get() with \p Action returns \p Result.
  /// Returns an error if there is a \a get() will not return
  Error put(const ActionDescription &Action, UniqueIDRef Result) {
    assert(&Result.getNamespace() == &getNamespace());
    return putImpl(Action, Result);
  }

  /// Get a result from the cache.
  ///
  /// May update \p Action to store a cached hash for a later to call to \a
  /// put().
  ///
  /// Thread-safe.
  ///
  /// \post \p MaybeResult is set to the cached result for \p Action, if any.
  /// Otherwise, \a UniqueID::isValid() is false.
  Error get(ActionDescription &Action, UniqueID &MaybeResult) {
    MaybeResult = UniqueID();
    return getImpl(Action, MaybeResult);
  }

  /// Get a result from the cache with extended lifetime.
  ///
  /// May update \p Action to store a cached hash for a later to call to \a
  /// put().
  ///
  /// Thread-safe.
  ///
  /// \post \p MaybeResult is set to the cached result for \p Action, if
  /// any. Otherwise, it has \a None.
  /// \post \p MaybeResult is valid to reference as long as the \a ActionCache
  /// is alive.
  Error get(ActionDescription &Action,
            Optional<UniqueIDRef> &MaybeResult) {
    MaybeResult = None;
    return getWithLifetimeImpl(Action, MaybeResult);
  }

  /// Get the CAS namespace for this ActionCache.
  const Namespace &getNamespace() const { return NS; }

  virtual ~ActionCache() = default;

protected:
  ActionCache(const Namespace &NS) : NS(NS) {}

  /// Cache a hash computed for \p Action.
  void cacheHash(ActionDescription &Action, ArrayRef<uint8_t> Hash) {
    assert(Hash.size() <= ActionDescription::MaxCachedHashSize);
    Action.CachedHash.Creator = this;
    llvm::copy(Hash, Action.CachedHash.Bytes);
    Action.CachedHash.Size = Hash.size();
  }

  /// Access the stored hash computed for \p Action.
  Optional<ArrayRef<uint8_t>> getCachedHash(ActionDescription &Action) {
    if (Action.CachedHash.Creator != this)
      return None;
    return makeArrayRef(Action.CachedHash.Bytes, Action.CachedHash.Size);
  }

  /// Cache \p Action to \p Result (implementation).
  ///
  /// \pre \p Result has been validated.
  /// \pre \p Action is not empty.
  virtual Error putImpl(const ActionDescription &Action,
                        UniqueIDRef Result) = 0;

  /// Get cached \p Action as \p Result (implementation). By default,
  /// defers to \a getWithLifetimeImpl(), but if that has overhead this can
  /// be overridden instead.
  ///
  /// \pre \p Action is not empty.
  /// \pre \p !MaybeResult.isValid().
  virtual Error getImpl(ActionDescription &Action,
                        UniqueID &MaybeResult) {
    Optional<UniqueIDRef> Ref;
    if (Error E = getWithLifetimeImpl(Action, Ref))
      return E;
    if (Ref)
      MaybeResult = *Ref;
    return Error::success();
  }

  /// Get cached \p Action as \p Result (implementation).
  ///
  /// \pre \p Action is not empty.
  /// \pre \p !MaybeResult.isValid().
  virtual Error getWithLifetimeImpl(ActionDescription &Action,
                                    Optional<UniqueIDRef> &MaybeResult) = 0;

private:
  const Namespace &NS;
};

/// Create an in-memory action cache for \a UniqueID from \p NS.
std::unique_ptr<ActionCache> createInMemoryActionCache(const Namespace &NS);

/// Create an on-disk action cache for \a UniqueID from \p NS.
Expected<std::unique_ptr<ActionCache>>
createOnDiskActionCache(const Namespace &NS, const Twine &Path);

/// Create a plugin action cache for \a UniqueID from \p NS.
Expected<std::unique_ptr<ActionCache>>
createPluginActionCache(const Namespace &NS, StringRef PluginPath,
                        ArrayRef<std::string> PluginArgs = None);

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_ACTIONCACHE_H
