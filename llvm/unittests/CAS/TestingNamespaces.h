//===- TestingNamespaces.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UNITTESTS_CAS_TESTINGNAMESPACES_H
#define LLVM_UNITTESTS_CAS_TESTINGNAMESPACES_H

#include "llvm/CAS/Namespace.h"

namespace llvm {
namespace cas {
namespace testing {

/// Helper for unit tests here to define a \a Namespace without depending on a
/// specific CAS implementation or hash function. \a Namespace::getHashSize()
/// will return \p HashSize and \a Namespace::getName() will include \p
/// NameSuffix.
const Namespace &getNamespace(size_t HashSize, StringRef NameSuffix = "");

} // end namespace testing
} // end namespace cas
} // end namespace llvm

#endif // LLVM_UNITTESTS_CAS_TESTINGNAMESPACES_H
