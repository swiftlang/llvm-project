//===- llvm/CAS/CASActionCache.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASACTIONCACHE_H
#define LLVM_CAS_CASACTIONCACHE_H

#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/CASReference.h"
#include "llvm/CAS/TreeEntry.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h" // FIXME: Split out sys::fs::file_status.
#include <cstddef>

namespace llvm {
namespace cas {

class CASDB;

/// A cache from a key describing an action to the result of doing it.
///
/// Actions are expected to be pure (write-once, collision is an error).
class ActionCache {
protected:
  virtual void anchor();

public:
  /// Get a previously computed result for \p ActionKey.
  virtual Optional<ObjectRef> get(const CacheKey &ActionKey) const = 0;

  /// Cache \p Result for the \p ActionKey computation.
  virtual Error put(const CacheKey &ActionKey, const ObjectRef &Result) = 0;

  /// Poison the cache for \p ActionKey.
  virtual Error poison(const CacheKey &ActionKey) = 0;

  /// Get or compute a result for \p ActionKey. The default implementation
  /// calls \a get(), and if not found calls \p Compute and \a put().
  ///
  /// Overrides should match this basic pattern, but is free to optimize or
  /// audit. For example, it might share an internal lookup of \p ActionKey
  /// between \a get() and \a put(). Or, it might probabilistically call \p
  /// Compute even after \a get() returns a result, calling \a poision() on a
  /// mismatch.
  virtual Expected<ObjectRef> getOrCompute(
      const CacheKey &ActionKey,
      function_ref<Expected<ObjectRef> ()> Compute);
};

/// Abstract string-based map.
class AbstractStringMap {
public:
  virtual Error put(StringRef Key, StringRef Value);

  /// Return \c true on missing.
  virtual bool get(StringRef Key, raw_ostream &ValueOS);
};

/// Adapts an abstract StringMap to an ActionCache, storing the result as the
/// ObjectID of the cache.
class StringMapActionCacheAdaptor : public ActionCache {
public:
  /// Put mapping from \p ActionKey to \p Result. Calls \a CASDB::getObjectID()
  /// and then \a putID(). Does not store the content of \p Result.
  Error put(const CacheKey &ActionKey, const ObjectRef &Result) override;

  /// Put mapping from \p ActionKey to \p Result.
  Optional<CASID> putID(const CacheKey &ActionKey, const CASID &Result);

  /// Get mapped result for \a ActionKey. Calls \a getID() and then \a
  /// CASDB::getReference().
  Optional<ObjectRef> get(const CacheKey &ActionKey) const override;

  /// Get the ID for the result of \p ActionKey.
  Optional<CASID> getID(const CacheKey &ActionKey) const;

  CASDB &getCAS() const { return CAS; };
  AbstractStringMap &getMap() const { return Map; };

private:
  CASDB &CAS;
  AbstractStringMap &Map;
};

/// Adapts an abstract StringMap to an ActionCache, storing the result as the
/// ObjectID of the cache. This also caches the content of objects themselves.
class StringMapActionCacheAdaptorWithObjectStorage : public {
public:
  /// Put mapping from \p ActionKey to \p Result, also storing the transitive
  /// content of \p Result in the map.
  Error put(const CacheKey &ActionKey, const ObjectRef &Result) override;

  /// Get mapped result for \a ActionKey. Stores the transitive content of the
  /// result in the CAS as well.
  Optional<ObjectRef> get(const CacheKey &ActionKey) const override;

  CASDB &getCAS() const { return UnderlyingAdaptor.getCAS(); };
  AbstractStringMap &getMap() const { return UnderlyingAdaptor.getMap(); };

private:
  StringMapActionCacheAdaptor UnderlyingAdaptor;
};

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_CASACTIONCACHE_H
