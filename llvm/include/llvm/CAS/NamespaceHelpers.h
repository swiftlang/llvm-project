//===- llvm/CAS/NamespaceHelpers.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_NAMESPACEHELPERS_H
#define LLVM_CAS_NAMESPACEHELPERS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Error.h"

namespace llvm {

class raw_ostream;

namespace cas {

class Namespace;

class ParseHashError : public ErrorInfo<ParseHashError, StringError> {
public:
  static char ID;

  ParseHashError(const Twine &S)
      : ParseHashError::ErrorInfo(S, inconvertibleErrorCode()) {}
};

namespace hexadecimal {

/// Print \p Hash as hexadecimal.
void printHash(ArrayRef<uint8_t> Hash, raw_ostream &OS);

/// Parse hash from \p Printed to invert \a printHash().
///
/// Errors if any of \p Printed cannot be converted, or if \p Dest is the wrong
/// size.
Error parseHash(StringRef Printed, MutableArrayRef<uint8_t> Dest);

/// Parse hash from \p Printed into \p Dest for a hash in \p NS.
Error parseHashForID(const Namespace &NS, StringRef Printed, UniqueID &Dest);

} // end namespace hexadecimal

} // end namespace cas
} // end namespace llvm

#endif LLVM_CAS_NAMESPACEHELPERS_H
