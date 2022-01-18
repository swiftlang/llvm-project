//===- llvm/CAS/CASID.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASID_H
#define LLVM_CAS_CASID_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/CAS/UniqueID.h"

namespace llvm {
namespace cas {

/// Wrapper around a raw hash-based identifier for a CAS object.
///
/// TODO: Replace usage with \a UniqueIDRef and \a UniqueID as appropriate,
/// then remove this.
class CASID {
public:
  ArrayRef<uint8_t> getHash() const { return Hash; }

  friend bool operator==(CASID LHS, CASID RHS) {
    return LHS.getHash() == RHS.getHash();
  }
  friend bool operator!=(CASID LHS, CASID RHS) {
    return LHS.getHash() != RHS.getHash();
  }

  CASID() = delete;
  CASID(UniqueIDRef ID) : Hash(ID.getHash()) {}
  explicit CASID(ArrayRef<uint8_t> Hash) : Hash(Hash) {}
  explicit operator ArrayRef<uint8_t>() const { return Hash; }

  friend hash_code hash_value(CASID ID) { return hash_value(ID.getHash()); }

private:
  ArrayRef<uint8_t> Hash;
};

} // namespace cas

template <> struct DenseMapInfo<cas::CASID> {
  static cas::CASID getEmptyKey() {
    return cas::CASID(DenseMapInfo<ArrayRef<uint8_t>>::getEmptyKey());
  }

  static cas::CASID getTombstoneKey() {
    return cas::CASID(DenseMapInfo<ArrayRef<uint8_t>>::getTombstoneKey());
  }

  static unsigned getHashValue(cas::CASID ID) {
    return DenseMapInfo<ArrayRef<uint8_t>>::getHashValue(ID.getHash());
  }

  static bool isEqual(cas::CASID LHS, cas::CASID RHS) {
    return DenseMapInfo<ArrayRef<uint8_t>>::isEqual(LHS.getHash(),
                                                    RHS.getHash());
  }
};

} // namespace llvm

#endif // LLVM_CAS_CASID_H
