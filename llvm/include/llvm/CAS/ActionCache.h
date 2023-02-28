//===- llvm/CAS/ActionCache.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASACTIONCACHE_H
#define LLVM_CAS_CASACTIONCACHE_H

#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/CASReference.h"
#include "llvm/Support/Error.h"
#include <future>

namespace llvm::cas {

class ObjectStore;
class CASID;
class ObjectProxy;

/// A key for caching an operation.
/// It is implemented as a bag of bytes and provides a convenient constructor
/// for CAS types.
class CacheKey {
public:
  StringRef getKey() const { return Key; }

  // TODO: Support CacheKey other than a CASID but rather any array of bytes.
  // To do that, ActionCache need to be able to rehash the key into the index,
  // which then `getOrCompute` method can be used to avoid multiple calls to
  // has function.
  CacheKey(const CASID &ID);
  CacheKey(const ObjectProxy &Proxy);
  CacheKey(const ObjectStore &CAS, const ObjectRef &Ref);

private:
  std::string Key;
};

/// An abstraction for a dictionary of string names that are associated with
/// \c ObjectRefs.
///
/// While this can be backed by a local-only implementation, it is designed to
/// provide reduced latency when combined with an underlying implementation that
/// uses a distributed network cache. Specifically:
///
/// * The objects of the map can be downloaded concurrently with asynchronous
/// queries.
///
/// * Clients can request to download only specific objects (e.g. only the
/// "main" output, instead of downloading all the outputs of a compilation).
///
/// * The outputs of a compilation can be bundled together in an
/// \c ActionCacheMap and associated with an \c ActionKey via \c putMap and
/// \c getMap. Then to get the outputs from the key, only 2 lookups are needed
/// (key -> map, map -> outputs), instead of 3 (key -> ID, ID -> bundle,
/// bundle -> outputs).
class ActionCacheMap {
  virtual void anchor();

public:
  virtual ~ActionCacheMap() = default;

  using FutureValue = std::future<Expected<std::optional<ObjectRef>>>;

  /// Get all the names that are present in this dictionary map.
  virtual std::vector<std::string> getAllNames() = 0;

  /// Materialize the \c ObjectRef associated with \p Name asynchronously. It is
  /// invalid to pass a string that doesn't exist as a name in this map.
  virtual FutureValue getValueAsync(StringRef Name) = 0;

  /// Materialize all the associated \c ObjectRefs asynchronously.
  std::vector<std::pair<std::string, FutureValue>> getAllValuesAsync();
};

/// A cache from a key describing an action to the result of doing it.
///
/// Actions are expected to be pure (collision is an error).
class ActionCache {
  virtual void anchor();

public:
  /// Get a previously computed result for \p ActionKey.
  ///
  /// \param Globally if true it is a hint to the underlying implementation that
  /// the lookup is profitable to be done on a distributed caching level, not
  /// just locally. The implementation is free to ignore this flag.
  Expected<Optional<CASID>> get(const CacheKey &ActionKey,
                                bool Globally = false) const {
    return getImpl(arrayRefFromStringRef(ActionKey.getKey()), Globally);
  }

  /// Cache \p Result for the \p ActionKey computation.
  ///
  /// \param Globally if true it is a hint to the underlying implementation that
  /// the association is profitable to be done on a distributed caching level,
  /// not just locally. The implementation is free to ignore this flag.
  Error put(const CacheKey &ActionKey, const CASID &Result,
            bool Globally = false) {
    assert(Result.getContext().getHashSchemaIdentifier() ==
               getContext().getHashSchemaIdentifier() &&
           "Hash schema mismatch");
    return putImpl(arrayRefFromStringRef(ActionKey.getKey()), Result, Globally);
  }

  /// Get the \c ObjectRef mappings that were associated with \p ActionKey by an
  /// earlier \c putMap call. It is invalid to call \c getMap with an
  /// \p ActionKey that was passed to \c put.
  ///
  /// \param Globally if true it is a hint to the underlying implementation that
  /// the lookup is profitable to be done on a distributed caching level, not
  /// just locally. The implementation is free to ignore this flag.
  virtual Expected<std::optional<std::unique_ptr<ActionCacheMap>>>
  getMap(const CacheKey &ActionKey, ObjectStore &CAS,
         bool Globally = false) const;

  /// Associate a mapping of name -> \c ObjectRef with an \p ActionKey. This
  /// mapping can be read back via \c getMap.
  ///
  /// \param Globally if true it is a hint to the underlying implementation that
  /// the association is profitable to be done on a distributed caching level,
  /// not just locally. The implementation is free to ignore this flag.
  virtual Error putMap(const CacheKey &ActionKey,
                       const StringMap<ObjectRef> &Mappings, ObjectStore &CAS,
                       bool Globally = false);

  virtual ~ActionCache() = default;

protected:
  virtual Expected<Optional<CASID>> getImpl(ArrayRef<uint8_t> ResolvedKey,
                                            bool Globally) const = 0;
  virtual Error putImpl(ArrayRef<uint8_t> ResolvedKey, const CASID &Result,
                        bool Globally) = 0;

  ActionCache(const CASContext &Context) : Context(Context) {}

  const CASContext &getContext() const { return Context; }

private:
  const CASContext &Context;
};

/// Create an action cache in memory.
std::unique_ptr<ActionCache> createInMemoryActionCache();

/// Get a reasonable default on-disk path for a persistent ActionCache for the
/// current user.
std::string getDefaultOnDiskActionCachePath();

/// Create an action cache on disk.
Expected<std::unique_ptr<ActionCache>> createOnDiskActionCache(StringRef Path);
} // end namespace llvm::cas

#endif // LLVM_CAS_CASACTIONCACHE_H
