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
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeInference.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/ADT/SmallVector.h"

namespace clang::lifetimes {

class DirectViewInference {
public:
  /// Adds \p RD, a complete non-dependent record definition, to the records to
  /// analyze.
  void addRecord(const CXXRecordDecl *RD) { Records.push_back(RD); }

  /// Runs the lifetime safety analysis on the functions of \p TU to a
  /// call-graph fix-point, then analyzes the records from the function
  /// summaries and reports the results as remarks to \p Diags.
  void run(TranslationUnitDecl *TU, DiagnosticsEngine &Diags);

private:
  /// Keeps the summaries of the functions of the translation unit.
  LifetimeInference LI;
  llvm::SmallVector<const CXXRecordDecl *> Records;
};

} // namespace clang::lifetimes

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_DIRECTVIEWINFERENCE_H
