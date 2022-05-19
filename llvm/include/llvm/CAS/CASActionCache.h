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
#include "llvm/CAS/CASCacheKey.h"
#include "llvm/CAS/CASReference.h"
#include "llvm/Support/Error.h"
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
  Optional<ObjectRef> get(const CacheKey &ActionKey) const {
    SmallVector<char, 32> Storage;
    return getImpl(resolveKey(ActionKey));
  }

  /// Cache \p Result for the \p ActionKey computation.
  Error put(const CacheKey &ActionKey, const ObjectRef &Result) {
    SmallVector<char, 32> Storage;
    return putImpl(resolveKey(ActionKey), Result);
  }

  /// Poison the cache for \p ActionKey.
  Error poison(const CacheKey &ActionKey) {
    SmallVector<char, 32> Storage;
    return poisonImpl(resolveKey(ActionKey));
  }

  /// Get or compute a result for \p ActionKey. Equivalent to calling \a get()
  /// (followed by \a compute() and \a put() on failure), but avoids calling \a
  /// resolveKey() twice.
  Expected<ObjectRef> getOrCompute(
      const CacheKey &ActionKey,
      function_ref<Expected<ObjectRef> ()> Compute);

  CASDB &getCAS() const { return CAS; }

protected:
  /// Convert \p ActionKey to internal key, using \p Storage if necessary for
  /// storage. Default implementation uses CacheKey's string representation
  /// directly.
  virtual ArrayRef<uint8_t> resolveKey(const CacheKey &ActionKey,
                                       SmallVectorImpl<char> &Storage) const;

  virtual Optional<ObjectRef> getImpl(ArrayRef<uint8_t> ResolvedKey) const = 0;
  virtual Error putImpl(ArrayRef<uint8_t> ResolvedKey, const ObjectRef &Result) = 0;
  virtual Error poisonImpl(ArrayRef<uint8_t> ResolvedKey) = 0;

  ActionCache(CASDB &CAS) : CAS(CAS) {}

private:
  CASDB &CAS;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_CASACTIONCACHE_H
