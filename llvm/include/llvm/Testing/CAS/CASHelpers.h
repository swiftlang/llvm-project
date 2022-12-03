//===- Testing/CAS/CASHelpers.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TESTING_CAS_CASHELPERS_H
#define LLVM_TESTING_CAS_CASHELPERS_H

#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/Error.h"

namespace llvm::cas::unittest {

/// An \c ObjectStore wrapper that injects \c Error values into load/store
/// operations based on a counter. Useful for ensuring that \c Error values are
/// handled or propogated correctly from code that uses a CAS.
class ErroringCAS : public llvm::cas::ProxyObjectStore {
public:
  ErroringCAS(std::unique_ptr<ObjectStore> CAS)
      : ProxyObjectStore(std::move(CAS)) {}

  /// Trigger errors after \c Count load or store operations.
  void setErrorCounter(unsigned Count) { ErrorCounter = Count; }

  Expected<ObjectRef> store(ArrayRef<ObjectRef> Refs,
                            ArrayRef<char> Data) override {
    if (ErrorCounter == 0)
      return llvm::createStringError(std::errc::operation_canceled, "store");
    ErrorCounter -= 1;
    return ProxyObjectStore::store(Refs, Data);
  }

private:
  Expected<llvm::cas::ObjectHandle> load(ObjectRef Ref) override {
    if (ErrorCounter == 0)
      return llvm::createStringError(std::errc::operation_canceled, "load");
    ErrorCounter -= 1;
    return ProxyObjectStore::load(Ref);
  }

  unsigned ErrorCounter = std::numeric_limits<unsigned>::max();
};

} // namespace llvm::cas::unittest

#endif // LLVM_TESTING_CAS_CASHELPERS_H