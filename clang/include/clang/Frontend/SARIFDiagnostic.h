//===--- SARIFDiagnostic.h - SARIF Diagnostic Formatting -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is a utility class that provides support for constructing a SARIF object
// containing diagnostics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FRONTEND_SARIFDIAGNOSTIC_H
#define LLVM_CLANG_FRONTEND_SARIFDIAGNOSTIC_H

#include "clang/Basic/Sarif.h"
#include "clang/Frontend/DiagnosticRenderer.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <optional>

namespace clang {

class SARIFDiagnostic : public DiagnosticRenderer {
public:
  SARIFDiagnostic(raw_ostream &OS, const LangOptions &LangOpts,
                  DiagnosticOptions &DiagOpts, SarifDocumentWriter *Writer);

  ~SARIFDiagnostic() = default;

  SARIFDiagnostic &operator=(const SARIFDiagnostic &&) = delete;
  SARIFDiagnostic(SARIFDiagnostic &&) = delete;
  SARIFDiagnostic &operator=(const SARIFDiagnostic &) = delete;
  SARIFDiagnostic(const SARIFDiagnostic &) = delete;

  /// Emit the buffered primary result, if any, with the notes that followed it
  /// folded into its code flow. Must be called once the run of notes belonging
  /// to a primary diagnostic has ended -- both when the next primary arrives
  /// and when the source file ends -- so no buffered result is lost.
  void flush();

protected:
  void emitDiagnosticMessage(FullSourceLoc Loc, PresumedLoc PLoc,
                             DiagnosticsEngine::Level Level, StringRef Message,
                             ArrayRef<CharSourceRange> Ranges,
                             DiagOrStoredDiag D) override;

  void emitDiagnosticLoc(FullSourceLoc Loc, PresumedLoc PLoc,
                         DiagnosticsEngine::Level Level,
                         ArrayRef<CharSourceRange> Ranges) override;

  void emitCodeContext(FullSourceLoc Loc, DiagnosticsEngine::Level Level,
                       SmallVectorImpl<CharSourceRange> &Ranges,
                       ArrayRef<FixItHint> Hints) override {}

  void emitIncludeLocation(FullSourceLoc Loc, PresumedLoc PLoc) override;

  void emitImportLocation(FullSourceLoc Loc, PresumedLoc PLoc,
                          StringRef ModuleName) override;

  void emitBuildingModuleLocation(FullSourceLoc Loc, PresumedLoc PLoc,
                                  StringRef ModuleName) override;

private:
  // Shared between SARIFDiagnosticPrinter and this renderer.
  SarifDocumentWriter *Writer;

  // The most recent non-note result, held back so that the notes that follow it
  // can be folded into its code flow rather than emitted as sibling results.
  // Emitted by flush().
  std::optional<SarifResult> BufferedResult;
  // The notes seen since BufferedResult, as ordered analysis steps to attach to
  // it as a code flow.
  llvm::SmallVector<ThreadFlow> PendingSteps;

  SarifResult addLocationToResult(SarifResult Result, FullSourceLoc Loc,
                                  PresumedLoc PLoc,
                                  ArrayRef<CharSourceRange> Ranges,
                                  const Diagnostic &Diag);

  SarifResult addRelatedLocationToResult(SarifResult Result, FullSourceLoc Loc,
                                         PresumedLoc PLoc);

  llvm::SmallVector<CharSourceRange>
  getSarifLocation(FullSourceLoc Loc, PresumedLoc PLoc,
                   ArrayRef<CharSourceRange> Ranges);

  SarifRule addDiagnosticLevelToRule(SarifRule Rule,
                                     DiagnosticsEngine::Level Level);

  llvm::StringRef emitFilename(StringRef Filename, const SourceManager &SM);

  llvm::SmallVector<std::pair<FullSourceLoc, PresumedLoc>>
      RelatedLocationsCache;
};

} // end namespace clang

#endif
