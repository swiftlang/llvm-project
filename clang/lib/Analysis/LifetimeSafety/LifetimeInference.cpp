//===- LifetimeInference.cpp - Inferring lifetimes -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeInference.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Checker.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Facts.h"
#include "clang/Analysis/Analyses/LifetimeSafety/FactsGenerator.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LiveOrigins.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LoanPropagation.h"
#include "clang/Analysis/Analyses/LifetimeSafety/MovedLoans.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticSema.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "lifetime-inference"

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

  FunctionSummariesRef SummariesRef(
      [this](const FunctionDecl *Callee) { return getSummary(Callee); });
  FactManager FactMgr(AC, Cfg, SummariesRef);
  FactsGenerator(FactMgr, AC, Opts, SummariesRef).run();
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
  LLVM_DEBUG(Summaries.lookup(FD).dump(llvm::dbgs(), FD));
}

std::function<bool(const FunctionDecl *)>
LifetimeInference::getLifetimeInferenceAnalyzeLambda(
    const LifetimeSafetyOpts &Opts,
    std::function<bool(const FunctionDecl *)> ShouldSkip,
    std::function<void(AnalysisDeclContext &)> SetupCFG) {
  return [this, Opts, ShouldSkip = std::move(ShouldSkip),
          SetupCFG = std::move(SetupCFG)](const FunctionDecl *FD) {
    if (FD->isImplicit() || ShouldSkip(FD))
      return false;
    AnalysisDeclContext AC(nullptr, FD);
    SetupCFG(AC);
    if (!AC.getCFG())
      return false;
    // A missing summary is an empty one.
    FunctionSummary Old;
    if (const FunctionSummary *Prev = getSummary(FD))
      Old = *Prev;
    analyzeFunction(AC, Opts);
    const FunctionSummary *New = getSummary(FD);
    return New && !(*New == Old);
  };
}

namespace {

// Returns true if FD is declared by a template with at least one defined
// instantiation and Pred holds for all of them.
bool allInstantiationsSatisfy(
    const FunctionDecl *FD,
    llvm::function_ref<bool(const FunctionDecl *)> Pred) {
  bool Exists = false;
  bool ForAll = true;
  auto Visit = [&](const FunctionDecl *Instantiation) {
    if (!Instantiation->isTemplateInstantiation() || !Instantiation->hasBody())
      return;
    Exists = true;
    ForAll &= Pred(Instantiation);
  };
  if (const FunctionTemplateDecl *FTD = FD->getDescribedFunctionTemplate()) {
    for (const FunctionDecl *Specialization : FTD->specializations())
      Visit(Specialization);
  } else if (const auto *RD = dyn_cast<CXXRecordDecl>(FD->getDeclContext())) {
    if (const ClassTemplateDecl *CT = RD->getDescribedClassTemplate())
      for (const ClassTemplateSpecializationDecl *Specialization :
           CT->specializations())
        for (const Decl *D : Specialization->decls())
          if (const auto *Member = dyn_cast<FunctionDecl>(D);
              Member && Member->getInstantiatedFromMemberFunction() &&
              Member->getInstantiatedFromMemberFunction()->getCanonicalDecl() ==
                  FD->getCanonicalDecl())
            Visit(Member);
  }
  return Exists && ForAll;
}

/// Whether \p Sum, the summary of \p Fun, has the contract of the oracle \p A
/// written on \p AttrOwner
bool hasInferred(const FunctionSummary &Sum, const FunctionDecl *Fun,
                 ParamOrThis AttrOwner, const Attr *Attr) {
  if (isa<NoEscapeAttr>(Attr))
    return llvm::is_contained(Sum.getNoEscapeFor(AttrOwner), Level::Value);
  if (isa<LifetimeBoundAttr>(Attr))
    return llvm::any_of(
        Sum.getLifetimeBoundFor(AttrOwner),
        [](const LifetimeBoundAssertion &LB) { return LB.L == Level::Value; });
  const auto *CB = dyn_cast<LifetimeCaptureByAttr>(Attr);
  if (!CB)
    return false;
  // The parameter of \p Fun, or `this`, that \p Id names. A name that is no
  // parameter, e.g., 'global', names no entity.
  auto ToParamOrThis =
      [&](const IdentifierInfo *Id) -> std::optional<ParamOrThis> {
    if (Id->getName() == "this")
      return ParamOrThis::forThis();
    for (const ParmVarDecl *PVD : Fun->parameters())
      if (PVD->getName() == Id->getName())
        return ParamOrThis::forParam(PVD);
    return std::nullopt;
  };
  // Whether \p AttrOwner is captured by the entity that \p Id names.
  auto IsCapturerMatched = [&](const IdentifierInfo *Id) {
    std::optional<ParamOrThis> Capturer = ToParamOrThis(Id);
    return Capturer && llvm::any_of(Sum.getCapturedByFor(AttrOwner),
                                    [&](const CapturedByAssertion &C) {
                                      return C.L == Level::Value &&
                                             C.Q == *Capturer;
                                    });
  };
  // Every named entity must capture.
  return llvm::all_of(CB->getArgIdents(), IsCapturerMatched);
}

// The number of contracts that \p Sum, the summary of \p Def, has: counted per
// assertion kind per Decl node, where the smallest node applies (a parameter,
// or the function for `this`).
unsigned countInferred(const FunctionSummary &Sum, const FunctionDecl *Def) {
  unsigned Count = 0;
  auto CountFor = [&](ParamOrThis P) {
    Count += llvm::count_if(
        Sum.getLifetimeBoundFor(P),
        [](const LifetimeBoundAssertion &LB) { return LB.L == Level::Value; });
    Count += llvm::count_if(
        Sum.getCapturedByFor(P),
        [](const CapturedByAssertion &C) { return C.L == Level::Value; });
    Count += llvm::is_contained(Sum.getNoEscapeFor(P), Level::Value);
  };
  if (const auto *MD = dyn_cast<CXXMethodDecl>(Def);
      MD && MD->isImplicitObjectMemberFunction())
    CountFor(ParamOrThis::forThis());
  for (const ParmVarDecl *PVD : Def->parameters())
    CountFor(ParamOrThis::forParam(PVD));
  return Count;
}

} // namespace

OracleComparisonReport LifetimeInference::compareWithOracles() const {
  OracleComparisonReport C;
  auto CheckAttrInferred = [&](const FunctionDecl *FD, ParamOrThis Owner,
                               const Attr *Attr) {
    auto HasAttr = [&](const FunctionDecl *F) {
      const FunctionSummary *Sum = getSummary(F);
      if (!Sum)
        return false;
      return hasInferred(*Sum, F->getDefinition(), Owner, Attr);
    };
    if (HasAttr(FD) || allInstantiationsSatisfy(FD, HasAttr))
      ++C.Matched;
    else
      C.Missed.push_back({Attr, FD, Owner});
  };

  for (const auto &[PVD, Oracle] : ParamOracles)
    if (const auto *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext()))
      for (const Attr *A : Oracle)
        CheckAttrInferred(FD, ParamOrThis::forParam(PVD), A);
  for (const auto &[FD, Oracle] : ThisOracles)
    for (const Attr *A : Oracle)
      CheckAttrInferred(FD, ParamOrThis::forThis(), A);
  for (const auto &[Def, Sum] : Summaries)
    C.Inferred += countInferred(Sum, Def);
  return C;
}

void LifetimeInference::reportOracleSummary(DiagnosticsEngine &Diags) const {
  OracleComparisonReport C = compareWithOracles();
  auto CountMissed = [&](attr::Kind K) -> unsigned {
    return llvm::count_if(C.Missed, [K](const MissedOracle &M) {
      return M.A->getKind() == K;
    });
  };
  Diags.Report(diag::remark_interop_lifetime_inference_oracle_summary)
      << C.Matched << CountMissed(attr::LifetimeBound)
      << CountMissed(attr::NoEscape) << CountMissed(attr::LifetimeCaptureBy)
      << C.Inferred;
  for (const MissedOracle &M : C.Missed)
    Diags.Report(M.A->getLocation(),
                 diag::note_interop_lifetime_inference_missed_oracle)
        << M.A;
}

} // namespace clang::lifetimes
