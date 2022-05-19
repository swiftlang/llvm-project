//===- llvm/CAS/CASActionCaches.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASACTIONCACHES_H
#define LLVM_CAS_CASACTIONCACHES_H

#include "llvm/CAS/CASActionCache.h"
#include "llvm/CAS/CASID.h"

namespace llvm {
namespace cas {

/// Create an action cache in memory.
std::unique_ptr<ActionCache> createInMemoryActionCache(CASDB &CAS);

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
  ArrayRef<uint8_t> resolveKey(const CacheKey &ActionKey,
                               SmallVectorImpl<char> &Storage) const override;

  /// Put mapping from \p ActionKey to \p Result. Calls \a CASDB::getObjectID()
  /// and then \a putID(). Does not store the content of \p Result.
  Error putImpl(ArrayRef<uint8_t> ActionKey, const ObjectRef &Result) override;

  /// Put mapping from \p ActionKey to \p Result.
  Optional<CASID> putID(ArrayRef<uint8_t> ActionKey, const CASID &Result);

  /// Get mapped result for \a ActionKey. Calls \a getID() and then \a
  /// CASDB::getReference().
  Optional<ObjectRef> getImpl(ArrayRef<uint8_t> ActionKey) const override;

  /// Get the ID for the result of \p ActionKey.
  Optional<CASID> getID(ArrayRef<uint8_t> ActionKey) const;

  AbstractStringMap &getMap() const { return Map; };

private:
  AbstractStringMap &Map;
};

/// Adapts an abstract StringMap to an ActionCache, storing the result as the
/// ObjectID of the cache. This also caches the content of objects themselves.
class StringMapActionCacheAdaptorWithObjectStorage : public ActionCache {
public:
  /// Put mapping from \p ActionKey to \p Result, also storing the transitive
  /// content of \p Result in the map.
  Error put(const CacheKey &ActionKey, const ObjectRef &Result) override;

  /// Get mapped result for \a ActionKey. Stores the transitive content of the
  /// result in the CAS as well.
  Optional<ObjectRef> get(const CacheKey &ActionKey) const override;

  AbstractStringMap &getMap() const { return UnderlyingAdaptor.getMap(); };

private:
  StringMapActionCacheAdaptor UnderlyingAdaptor;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_CASACTIONCACHES_H
