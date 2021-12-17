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
#include "llvm/CAS/Namespace.h"
#include "llvm/CAS/UniqueID.h"
#include "llvm/Support/Error.h"
#include <cstddef>

namespace llvm {
namespace cas {

/// Structure for describing an action and its inputs.
class ActionDescription {
public:
  explicit ActionDescription(StringRef Data, ArrayRef<UniqueIDRef> IDs = None)
      : Data(Data), IDs(ID) {
    assert(!Data.empty());
  }

  explicit ActionDescription(StringRef Data, UniqueIDRef ID)
      : Action(Data, makeArrayRef(ID)) {}

private:
  StringRef Data;
  ArrayRef<UniqueIDRef> IDs;
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
  /// \post \p MaybeResult is set to the cached result for \p Action, if any.
  /// Otherwise, \a UniqueID::isValid() is false.
  Error get(const ActionDescription &Action, UniqueID &MaybeResult) {
    MaybeResult = UniqueID();
    return getImpl(Action, Result);
  }

  /// Get a result from the cache with extended lifetime.
  ///
  /// \post \p MaybeResult is set to the cached result for \p Action, if
  /// any. Otherwise, it has \a None.
  /// \post \p MaybeResult is valid to reference as long as the \a ActionCache
  /// is alive.
  Error get(const ActionDescription &Action,
            Optional<UniqueIDRef> &MaybeResult) {
    MaybeResult = None;
    return getWithLifetimeImpl(Action, MaybeResult);
  }

  /// Get the CAS namespace for this ActionCache.
  const Namespace &getNamespace() const { return NS; }

protected:
  ActionCache(const Namespace &NS) : NS(NS) {}

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
  virtual Error getImpl(const ActionDescription &Action,
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
  virtual Error getWithLifetimeImpl(const ActionDescription &Action,
                                    Optional<UniqueIDRef> &MaybeResult) = 0;

private:
  const Namespace &NS;
};

///     - put: (StringRef, UniqueIDRef)           => Error
///     - get: (StringRef, UniqueID&)             => Error
///     - get: (StringRef, Optional<UniqueIDRef>) => Error // provides lifetime
///     - Concrete subclasses:
///         - InMemory -- can use ThreadSafeHashMappedTrie
///         - OnDisk -- can use OnDiskHashMappedTrie, but lots of options; needs
///           configuration
///         - Plugin -- simple plugin API for the cache itself

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_ACTIONCACHE_H
