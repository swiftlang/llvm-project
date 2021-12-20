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

/// Structure for describing an action and its inputs. This is used as a key in
/// the \a ActionCache.
///
/// - \a getIdentifier(), which is required, is a printable identifier.
/// - \a getIDs() contains action inputs.
/// - \a getExtraData() is arbitrary data that's not printable, such as a
///   context hash, or serialized data not stored in a CAS.
class ActionDescription {
public:
  ActionDescription() = delete;

  explicit ActionDescription(StringRef Identifier,
                             ArrayRef<UniqueIDRef> IDs = None,
                             StringRef ExtraData = "")
      : Identifier(Identifier), ExtraData(ExtraData), IDs(IDs) {
    assert(!Identifier.empty());
  }

  explicit ActionDescription(StringRef Identifier, UniqueIDRef ID,
                             StringRef ExtraData = "")
      : ActionDescription(Identifier, makeArrayRef(ID), ExtraData) {}

  StringRef getIdentifier() const { return Identifier; }
  ArrayRef<UniqueIDRef> getIDs() const { return IDs; }
  StringRef getExtraData() const { return ExtraData; }

  void print(raw_ostream &OS, bool PrintIDs = true, PrintExtraData = false) const;
  void dump() const;
  friend raw_ostream &operator<<(raw_ostream &OS, const ActionDescription &Action) {
    Action.print(OS);
    return OS;
  }

  /// Serialize. This can be used to hash the description.
  void serialize(function_ref<void (ArrayRef<uint8_t>)> AppendBytes) const;

private:
  const StringRef Identifier;
  const StringRef ExtraData;
  const ArrayRef<UniqueIDRef> IDs;

  friend class ActionCache;
  enum : size_t { MaxCachedHashSize = 32 };

  /// A cache for the hash of this action. Mutators and accessors are available
  /// for implementations of \a ActionCache via \a ActionCache::cacheHash() and
  /// \a ActionCache::getCachedHash(). This allows duplicate
  /// serialization/hashing to be avoided in the case: \a ActionCache::get() is
  /// a cache miss, result computed, \a ActionCache::put() called with result.
  ///
  /// This does not just store a \a UniqueID, since an \a ActionCache may use a
  /// different hash algorithm than the namespace of the \a UniqueIDs... or no
  /// hash algorithm at all.
  ///
  /// TODO: Also allow a \a std::string to be cached instead (via a variant of
  /// some sort), in case an \a ActionCache implementation is using the full,
  /// serialized ActionDescription directly rather than hashing it.
  struct {
    /// Track the \a ActionCache that cached here to catch errors where \a
    /// ActionCache::get() is called with one cache and \a ActionCache::put()
    /// is called with another, where the latter potentially uses a different
    /// map strategy.
    const ActionCache *Creator = nullptr;

    /// FIXME: Do we care about the layout of \a ActionDescription? If not,
    /// maybe this should just be SmallVector<uint8_t, 32>.
    uint8_t Hash[MaxCachedHashSize] = {0};
    size_t HashSize = 0;
  } Cached;
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

    // Check that the inputs are in the same namespace. Not strictly necessary
    // for correctness of the cache, but if this fails there's probably a bug...
    //
    // Note: Only check the first ID here. ActionDescription::serialize()
    // asserts that they all match; assume this will be called at some point to
    // generate a hash for it.
    assert((Action.getIDs().empty() ||
            &getNamespace() == Action.getIDs().front().getNamespace()) &&
           "Mismatched namespace");

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
    Action.Cached.Creator = this;
    llvm::copy(Hash, Action.Cached.Hash);
    Action.Cached.HashSize = Hash.size();
  }

  /// Access the stored hash computed for \p Action.
  Optional<ArrayRef<uint8_t>> getCachedHash(ActionDescription &Action) {
    if (Action.Cached.Creator != this)
      return None;
    return makeArrayRef(Action.Cached.Hash, Action.Cached.HashSize);
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

/// An error for opening an \a ActionCache with the wrong namespace.
class WrongActionCacheNamespaceError final
    : public ErrorInfo<WrongActionCacheNamespaceError> {
  void anchor() override;

public:
  static char ID;
  void log(raw_ostream &OS) const override;
  std::error_code convertToErrorCode() const override {
    return make_error_code(std::errc::invalid_argument);
  }

  StringRef getExpectedNamespaceName() const { return ExpectedNamespaceName; }
  StringRef getObservedNamespaceName() const { return ObservedNamespaceName; }

  WrongActionCacheNamespaceError(const Namespace &ExpectedNamespace,
                                 const Twine &ObservedNamespaceName)
      : ExpectedNamespaceName(ExpectedNamespace.getName().str()),
        ObservedNamespaceName(ObservedNamespaceName.str()) {}

private:
  std::string ExpectedNamespaceName;
  std::string ObservedNamespaceName;
};

/// Error involving an \a ActionDescription.
///
/// Constructor prints out the description to protect from lifetime issues in
/// case the description's references go out of scope. This is expensive, so
/// these errors should be reserved for error paths that are rare or can be
/// slow.
class ActionDescriptionError
    : public ErrorInfo<ActionDescriptionError, ECError> {
  void anchor() override;

public:
  static char ID;
  void log(raw_ostream &OS) const override;

  StringRef getAction() const { return Action; }

  ActionDescriptionError(std::error_code EC, const ActionDescription &Action)
      : ActionDescriptionError::ErrorInfo(EC) {
    saveAction(Action);
  }

protected:
  void saveAction(const ActionDescription &Action);

private:
  std::string Action;
};

/// ActionCache collision error.
///
/// Expected to be extremely rare, indicating a programming error where context
/// was missing from the key.
class ActionCacheCollisionError final
    : public ErrorInfo<ActionCacheCollisionError, ActionDescriptionError> {
  void anchor() override;

public:
  static char ID;
  void log(raw_ostream &OS) const override;

  UniqueIDRef getNewResult() const { return NewResult; }
  UniqueIDRef getExistingResult() const { return ExistingResult; }

  ActionCacheCollisionError(UniqueIDRef ExistingResult,
                            const ActionDescription &Action,
                            UniqueIDRef NewResult)
      : ActionCacheCollisionError::ErrorInfo(
            make_error_code(std::errc::file_exists), Action),
        ExistingResult(ExistingResult), NewResult(NewResult) {}

private:
  UniqueID ExistingResult;
  UniqueID NewResult;
};

class CorruptActionCacheResultError final
    : public ErrorInfo<CorruptActionCacheResultError, ActionDescriptionError> {
  void anchor() override;

public:
  static char ID;
  void log(raw_ostream &OS) const override;

  CorruptActionCacheResultError(const ActionDescription &Action)
      : CorruptActionCacheResultError::ErrorInfo(
            make_error_code(std::errc::result_out_of_range), Action) {}
};

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_ACTIONCACHE_H
