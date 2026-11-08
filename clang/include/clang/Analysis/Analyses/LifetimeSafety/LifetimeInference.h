//===- LifetimeInference.h - Inferring lifetimes ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The lifetime attributes/properties inference module. It runs the lifetime
// safety analysis (LSA) on the CFG of a function and keeps the inferred
// FunctionSummary of each analyzed function for the translation unit.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMEINFERENCE_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMEINFERENCE_H

#include "clang/AST/Decl.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSafety.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSummary.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Compiler.h"
#include <functional>

namespace clang {
class Attr;
class Declarator;
class DiagnosticsEngine;
} // namespace clang

namespace clang::lifetimes {

/// The attributes written in the source, kept by the test mode instead of
/// being attached, for the parameters and for `this`.
using ParamOracleMap =
    llvm::DenseMap<const ParmVarDecl *, llvm::SmallVector<const Attr *, 2>>;
using ThisOracleMap =
    llvm::DenseMap<const FunctionDecl *, llvm::SmallVector<const Attr *, 2>>;

/// An oracle that was not inferred: \p A, written on \p Owner of \p FD.
struct MissedOracle {
  const Attr *A;
  const FunctionDecl *FD;
  ParamOrThis Owner;
};

/// How the inferred function contracts compare with the oracles.
struct OracleComparisonReport {
  /// The number of oracles that were inferred.
  unsigned Matched = 0;
  /// The oracles that were not inferred.
  llvm::SmallVector<MissedOracle> Missed;
  /// The number of contracts inferred, with or without an oracle.
  unsigned Inferred = 0;
};

class LifetimeInference {
public:
  /// Runs the lifetime safety analysis on the function of \p AC, silently, and
  /// records the inferred summary, replacing the summary of an earlier
  /// analysis of the function.
  void analyzeFunction(AnalysisDeclContext &AC, const LifetimeSafetyOpts &Opts);

  /// Returns a lambda of type bool(const FunctionDecl *) that runs
  /// analyzeFunction on the function and returns true iff its summary changed.
  /// The function is skipped if it is implicit or \p ShouldSkip holds for it;
  /// \p SetupCFG sets the CFG build options of its context. The lambda must not
  /// outlive this.
  std::function<bool(const FunctionDecl *)> getLifetimeInferenceAnalyzeLambda(
      const LifetimeSafetyOpts &Opts,
      std::function<bool(const FunctionDecl *)> ShouldSkip,
      std::function<void(AnalysisDeclContext &)> SetupCFG) LLVM_LIFETIME_BOUND;

  /// \return the summary of the definition of \p FD, or null. Do not keep the
  /// pointer: analyzing a function can invalidate it.
  const FunctionSummary *getSummary(const FunctionDecl *FD) const {
    const FunctionDecl *Def = FD->getDefinition();
    auto It = Def ? Summaries.find(Def) : Summaries.end();
    return It == Summaries.end() ? nullptr : &It->second;
  }

  /// Test mode: compares the oracles with the inferred summaries. An oracle
  /// contract of a function without a summary counts as not inferred.
  OracleComparisonReport compareWithOracles() const;

  /// Test mode: reports the comparison as one remark.
  void reportOracleSummary(DiagnosticsEngine &Diags) const;

  /// Test mode: keeps the attribute \p A written on the parameter \p PVD as an
  /// oracle, instead of keeping it in AST.
  void addParamOracle(const ParmVarDecl *PVD, const Attr *A);

  /// Test mode: keeps the attributes written on the implicit object parameter
  /// of \p FD as oracles, instead of keeping it in AST.
  void addThisOracles(const FunctionDecl *FD,
                      llvm::SmallVector<const Attr *, 2> Attrs);

  /// Test mode: the lifetime attributes of the implicit object parameter are
  /// dropped while the declarator's type is built, before any FunctionDecl
  /// exists. They wait here, with their declarator, until the declared
  /// function is known and they are saved as its oracles.
  struct PendingThisOracleSlot {
    const Declarator *D = nullptr;
    llvm::SmallVector<const Attr *, 2> Attrs;

    void add(const Declarator *Dcl, const Attr *A) {
      // A stale slot of another declarator, e.g. a lambda's, is dropped.
      if (D != Dcl)
        reset();
      D = Dcl;
      Attrs.push_back(A);
    }
    void reset() {
      D = nullptr;
      Attrs.clear();
    }
  } PendingThisOracle;

private:
  ParamOracleMap ParamOracles;
  ThisOracleMap ThisOracles;
  /// The summaries of the analyzed definitions.
  llvm::DenseMap<const FunctionDecl *, FunctionSummary> Summaries;
};

} // namespace clang::lifetimes

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMEINFERENCE_H
