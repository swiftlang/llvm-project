//===--- DiagnosticUtils.h - Diagnostic enablement helpers ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Helpers for querying whether a diagnostic is enabled, shared by the
// experimental analyses that must not be switched on by -Weverything.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_DIAGNOSTICUTILS_H
#define LLVM_CLANG_BASIC_DIAGNOSTICUTILS_H

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"

namespace clang {

/// Returns true if the user explicitly enabled \p DiagID at \p Loc, e.g. via
/// -W<group> or a pragma, as opposed to it being implied by -Weverything.
/// See rdar://190622221.
inline bool IsDiagExplicitlyEnabled(const DiagnosticsEngine &Diags,
                                    unsigned DiagID, SourceLocation Loc) {
  DiagnosticMapping Mapping = Diags.getDiagnosticMapping(DiagID, Loc);
  return Mapping.isUser() && Mapping.getSeverity() != diag::Severity::Ignored;
}

/// Returns true if \p DiagID is enabled at \p Loc, ignoring enablement that
/// comes only from -Weverything.
inline bool IsDiagEnabledExcludingWeverything(const DiagnosticsEngine &Diags,
                                              unsigned DiagID,
                                              SourceLocation Loc) {
  // Fast path for the common case, evaluated once per function body: these
  // are plain warnings, so a mapping of Ignored at Loc is final, and a mapping
  // that is not user-specified cannot count as explicitly enabled. This avoids
  // the full severity computation.
  DiagnosticMapping Mapping = Diags.getDiagnosticMapping(DiagID, Loc);
  if (Mapping.getSeverity() == diag::Severity::Ignored)
    return false;

  if (Diags.isIgnored(DiagID, Loc))
    return false;
  return !Diags.getEnableAllWarnings() ||
         IsDiagExplicitlyEnabled(Diags, DiagID, Loc);
}

} // namespace clang

#endif // LLVM_CLANG_BASIC_DIAGNOSTICUTILS_H
