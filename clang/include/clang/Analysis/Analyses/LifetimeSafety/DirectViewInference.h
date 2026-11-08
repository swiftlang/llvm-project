//===- DirectViewInference.h - Direct view type inference ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Infers C++ record types that are direct views.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_DIRECTVIEWINFERENCE_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_DIRECTVIEWINFERENCE_H

#include "clang/AST/DeclCXX.h"
#include "llvm/ADT/SmallVector.h"

namespace clang::lifetimes {

class DirectViewInference {
public:
  /// Adds \p RD, a complete non-dependent record definition, to the records to
  /// analyze.
  void addRecord(const CXXRecordDecl *RD) { Records.push_back(RD); }

private:
  llvm::SmallVector<const CXXRecordDecl *> Records;
};

} // namespace clang::lifetimes

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_DIRECTVIEWINFERENCE_H
