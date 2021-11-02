//===- llvm/CAS/Namespace.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_NAMESPACE_H
#define LLVM_CAS_NAMESPACE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {
namespace cas {

class UniqueID;
class UniqueIDRef;

/// Namespace for the CAS. Has an identifier and knows its hash size.
///
/// Instances should be declared \a ManagedStatic to compare by pointer
/// identity.
class Namespace {
  virtual void anchor();

public:
  /// Returns an identifying name for the namespace.
  StringRef getName() const { return Name; }

  /// Returns the size of a hash in bytes.
  size_t getHashSize() const { return HashSize; }

  /// Print \p ID to \p OS.
  ///
  /// Reports a fatal error if \a ID is from a different namespace.
  void printID(const UniqueIDRef &ID, raw_ostream &OS) const;

protected:
  /// Print \p ID to \p OS. \p ID is guaranteed to be from this namespace.
  virtual void printIDImpl(const UniqueIDRef &ID, raw_ostream &OS) const = 0;

public:
  /// Parse an ID.
  virtual Error parseID(StringRef Reference, UniqueID &ID) const = 0;

  Namespace() = delete;
  Namespace(Namespace &&) = delete;
  Namespace(const Namespace &) = delete;
  virtual ~Namespace() = default;

protected:
  virtual void printIDImpl(const UniqueIDRef &ID, raw_ostream &OS) const = 0;

  Namespace(StringRef Name, size_t HashSize)
      : Name(Name.str()), HashSize(HashSize) {
    assert(HashSize >= sizeof(size_t) && "Expected strong hash");
  }

private:
  const std::string Name;
  const size_t HashSize;
};

} // end namespace cas
} // end namespace llvm

#endif // LLVM_CAS_NAMESPACE_H
