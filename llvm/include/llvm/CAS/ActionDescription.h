//===- llvm/CAS/ActionDescription.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_ACTIONDESCRIPTION_H
#define LLVM_CAS_ACTIONDESCRIPTION_H

#include "llvm/ADT/StringRef.h"
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

  explicit ActionDescription(StringRef Identifier, const UniqueIDRef &ID,
                             StringRef ExtraData = "")
      : ActionDescription(Identifier, makeArrayRef(ID), ExtraData) {}

  StringRef getIdentifier() const { return Identifier; }
  ArrayRef<UniqueIDRef> getIDs() const { return IDs; }
  StringRef getExtraData() const { return ExtraData; }

  void print(raw_ostream &OS, bool PrintIDs = true,
             bool PrintExtraData = false) const;
  void dump() const;
  friend raw_ostream &operator<<(raw_ostream &OS,
                                 const ActionDescription &Action) {
    Action.print(OS);
    return OS;
  }

  /// Serialize. This can be used to hash the description.
  ///
  /// This does not serialize the \a Namespace of IDs, which must be known by
  /// context.
  ///
  /// TODO: Consider adding a deserialize() free function that takes \a
  /// Namespace as an argument.
  void serialize(function_ref<void(ArrayRef<uint8_t>)> AppendBytes) const;

  template <class HasherT> void updateHasher(HasherT &Hasher) const {
    serialize([&Hasher](ArrayRef<uint8_t> Bytes) { Hasher.update(Bytes); });
  }
  template <class HasherT,
            class HashT = std::array<uint8_t, sizeof(HasherT::hash(None))>>
  HashT computeHash() const {
    HasherT Hasher;
    updateHasher(Hasher);
    HashT Hash;
    llvm::copy(Hasher.final(), Hash.begin());
    return Hash;
  }

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

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_ACTIONDESCRIPTION_H
