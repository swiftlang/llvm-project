//===- LifetimeInference.cpp - Inferring lifetimes -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeInference.h"
#include "clang/AST/Attr.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Checker.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Facts.h"
#include "clang/Analysis/Analyses/LifetimeSafety/FactsGenerator.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LiveOrigins.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LoanPropagation.h"
#include "clang/Analysis/Analyses/LifetimeSafety/MovedLoans.h"
#include "clang/Analysis/CFG.h"

namespace clang::lifetimes {

void LifetimeInference::addParamOracle(const ParmVarDecl *PVD, const Attr *A) {
  ParamOracles[PVD].push_back(A);
}

void LifetimeInference::addThisOracles(
    const FunctionDecl *FD, llvm::SmallVector<const Attr *, 2> Attrs) {
  ThisOracles[FD] = std::move(Attrs);
}

void LifetimeInference::analyzeFunction(AnalysisDeclContext &AC,
                                        const LifetimeSafetyOpts &Opts) {
  using namespace internal;
  const auto *FD = dyn_cast_or_null<FunctionDecl>(AC.getDecl());
  if (!FD)
    return;
  const CFG &Cfg = *AC.getCFG();
  if (Opts.MaxCFGBlocks > 0 && Cfg.getNumBlockIDs() > Opts.MaxCFGBlocks)
    return;
  FactManager FactMgr(AC, Cfg);
  FactsGenerator(FactMgr, AC, Opts).run();
  LifetimeFactory Factory;
  LoanPropagationAnalysis LoanPropagation(
      Cfg, AC, FactMgr, Factory.OriginMapFactory, Factory.LoanSetFactory);
  LiveOriginsAnalysis LiveOrigins(Cfg, AC, FactMgr, Factory.LivenessMapFactory);
  MovedLoansAnalysis MovedLoans(Cfg, AC, FactMgr, LoanPropagation, LiveOrigins,
                                FactMgr.getLoanMgr(),
                                Factory.MovedLoansMapFactory);
  // No SemaHelper: the run must not report anything.
  Summaries.insert_or_assign(
      FD,
      runLifetimeAnalysisForSummary(LoanPropagation, MovedLoans, LiveOrigins,
                                    FactMgr, AC, /*SemaHelper=*/nullptr, Opts));
}

} // namespace clang::lifetimes
