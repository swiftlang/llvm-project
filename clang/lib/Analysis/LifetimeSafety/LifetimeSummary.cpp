//===- LifetimeSummary.cpp - Inferred lifetimes ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSummary.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/raw_ostream.h"

namespace clang::lifetimes {

FieldSet FieldSet::of(FieldT S) {
  FieldSet F;
  F.Fields.push_back(S);
  return F;
}

bool FieldSet::isAll() const { return Fields.empty(); }

llvm::ArrayRef<FieldT> FieldSet::fields() const { return Fields; }

void FieldSet::merge(const FieldSet &Other) {
  if (isAll())
    return;
  if (Other.isAll()) {
    Fields.clear();
    return;
  }
  for (FieldT S : Other.Fields)
    if (!llvm::is_contained(Fields, S))
      Fields.push_back(S);
}

bool FieldSet::operator<=(const FieldSet &F2) const {
  if (F2.isAll())
    return true;
  if (isAll())
    return false;
  return llvm::all_of(
      F2.Fields, [&](FieldT S) { return llvm::is_contained(Fields, S); });
}

ParamOrThis ParamOrThis::forThis() { return ParamOrThis(-1); }

ParamOrThis ParamOrThis::forParam(unsigned Idx) { return ParamOrThis(Idx); }

ParamOrThis ParamOrThis::forParam(const ParmVarDecl *P) {
  assert(P && "a parameter is expected");
  return forParam(P->getFunctionScopeIndex());
}

ParamOrThis::ParamOrThis(int Index) : Index(Index) {}

bool ParamOrThis::isThis() const { return Index < 0; }

unsigned ParamOrThis::getParamIndex() const {
  assert(!isThis() && "`this` is no parameter");
  return Index;
}

bool ParamOrThis::operator==(const ParamOrThis &Other) const {
  return Index == Other.Index;
}

static bool sameKey(const LifetimeBoundAssertion &A,
                    const LifetimeBoundAssertion &A2) {
  return A.L == A2.L;
}

static bool sameKey(const CapturedByAssertion &A,
                    const CapturedByAssertion &A2) {
  return A.L == A2.L && A.Q == A2.Q && A.QL == A2.QL;
}

/// Every assertion in \p S2 has an assertion in \p S that is <= it.
template <typename Assertion>
static bool refinesAll(llvm::ArrayRef<Assertion> S,
                       llvm::ArrayRef<Assertion> S2) {
  return llvm::all_of(S2, [&](const Assertion &A2) {
    return llvm::any_of(
        S, [&](const Assertion &A) { return sameKey(A, A2) && A.F <= A2.F; });
  });
}

static const ParameterThisSummary &lookup(const FunctionSummary &S,
                                          ParamOrThis P) {
  static const ParameterThisSummary Empty;
  auto It = S.ParamOrThisSummaries.find(P);
  return It == S.ParamOrThisSummaries.end() ? Empty : It->second;
}

bool operator<=(const ParameterThisSummary &S, const ParameterThisSummary &S2) {
  return refinesAll<LifetimeBoundAssertion>(S.LifetimeBound,
                                            S2.LifetimeBound) &&
         refinesAll<CapturedByAssertion>(S.CapturedBy, S2.CapturedBy) &&
         (!S2.Invalidated ||
          (S.Invalidated && *S.Invalidated <= *S2.Invalidated)) &&
         llvm::all_of(S2.NoEscape, [&](Level L) {
           return llvm::is_contained(S.NoEscape, L);
         });
}

bool operator==(const ParameterThisSummary &S, const ParameterThisSummary &S2) {
  return S <= S2 && S2 <= S;
}

bool operator<=(const FunctionSummary &S, const FunctionSummary &S2) {
  // A missing entry is an empty summary.
  return llvm::all_of(S2.ParamOrThisSummaries, [&](const auto &Entry) {
    return lookup(S, Entry.first) <= Entry.second;
  });
}

bool operator==(const FunctionSummary &S, const FunctionSummary &S2) {
  return S <= S2 && S2 <= S;
}

llvm::ArrayRef<LifetimeBoundAssertion>
FunctionSummary::getLifetimeBoundFor(ParamOrThis P) const {
  return lookup(*this, P).LifetimeBound;
}

llvm::ArrayRef<CapturedByAssertion>
FunctionSummary::getCapturedByFor(ParamOrThis P) const {
  return lookup(*this, P).CapturedBy;
}

llvm::ArrayRef<Level> FunctionSummary::getNoEscapeFor(ParamOrThis P) const {
  return lookup(*this, P).NoEscape;
}

const FieldSet *FunctionSummary::getInvalidatedFor(ParamOrThis P) const {
  const auto &Invalidated = lookup(*this, P).Invalidated;
  return Invalidated ? &*Invalidated : nullptr;
}

static unsigned levelNumber(Level L) { return static_cast<unsigned>(L); }

static std::string name(const FunctionDecl *FD, ParamOrThis P) {
  if (P.isThis())
    return "this";
  unsigned Idx = P.getParamIndex();
  if (Idx < FD->getNumParams() && !FD->getParamDecl(Idx)->getName().empty())
    return FD->getParamDecl(Idx)->getName().str();
  return ("#" + llvm::Twine(Idx)).str();
}

static std::string name(FieldT F) {
  if (const auto *D = dyn_cast<const Decl *>(F))
    return cast<NamedDecl>(D)->getNameAsString();
  return cast<const CXXBaseSpecifier *>(F)->getType().getAsString();
}

void FunctionSummary::dump(llvm::raw_ostream &OS,
                           const FunctionDecl *FD) const {
  OS << FD->getQualifiedNameAsString() << ":\n";
  auto DumpOne = [&](ParamOrThis P) {
    OS << "  " << name(FD, P) << ":";
    for (const LifetimeBoundAssertion &A : getLifetimeBoundFor(P))
      OS << " lifetimebound(" << levelNumber(A.L) << ")";
    for (const CapturedByAssertion &A : getCapturedByFor(P))
      OS << " captured_by(" << levelNumber(A.L) << " -> " << name(FD, A.Q)
         << "@" << levelNumber(A.QL) << ")";
    for (Level L : getNoEscapeFor(P))
      OS << " noescape(" << levelNumber(L) << ")";
    if (const FieldSet *F = getInvalidatedFor(P)) {
      OS << " invalidated";
      if (!F->isAll()) {
        OS << "(";
        llvm::ListSeparator Sep;
        for (FieldT Field : F->fields())
          OS << Sep << name(Field);
        OS << ")";
      }
    }
    OS << "\n";
  };
  if (const auto *MD = dyn_cast<CXXMethodDecl>(FD);
      MD && MD->isImplicitObjectMemberFunction())
    DumpOne(ParamOrThis::forThis());
  for (const ParmVarDecl *PVD : FD->parameters())
    DumpOne(ParamOrThis::forParam(PVD));
}

/// Adds the assertion unless one with the same key exists.
template <typename Assertion>
static bool addAssertion(llvm::SmallVectorImpl<Assertion> &Assertions,
                         Assertion New) {
  if (llvm::any_of(Assertions,
                   [&](const Assertion &A) { return sameKey(A, New); }))
    return false;
  Assertions.push_back(std::move(New));
  return true;
}

// An asserted flow is an escape, so it ends the NoEscape of the level.
static void removeNoEscape(ParameterThisSummary &S, Level L) {
  llvm::erase_if(S.NoEscape, [&](Level NL) { return NL == L; });
}

bool FunctionSummaryBuilder::addLifetimeBound(ParamOrThis P, Level L,
                                              FieldSet F) {
  ParameterThisSummary &S = Summary.ParamOrThisSummaries[P];
  if (!addAssertion(S.LifetimeBound, LifetimeBoundAssertion{L, std::move(F)}))
    return false;
  removeNoEscape(S, L);
  return true;
}

bool FunctionSummaryBuilder::addCapturedByAssertion(ParamOrThis P, Level L,
                                                    ParamOrThis Q, Level QL,
                                                    FieldSet F) {
  ParameterThisSummary &S = Summary.ParamOrThisSummaries[P];
  if (!addAssertion(S.CapturedBy, CapturedByAssertion{L, Q, QL, std::move(F)}))
    return false;
  removeNoEscape(S, L);
  return true;
}

bool FunctionSummaryBuilder::addNoEscape(ParamOrThis P, Level L) {
  ParameterThisSummary &S = Summary.ParamOrThisSummaries[P];
  if (llvm::is_contained(S.NoEscape, L))
    return false;
  S.NoEscape.push_back(L);
  return true;
}

bool FunctionSummaryBuilder::addInvalidated(ParamOrThis P, FieldSet F) {
  std::optional<FieldSet> &Invalidated =
      Summary.ParamOrThisSummaries[P].Invalidated;
  if (Invalidated)
    return false;
  Invalidated = std::move(F);
  return true;
}

FunctionSummary FunctionSummaryBuilder::build() { return std::move(Summary); }

std::optional<const FunctionSummary *>
FunctionSummariesRef::synthesizeHeuristicSummaryForOpaque(
    const FunctionDecl *FD) const {
  FD = FD->getCanonicalDecl();
  if (auto It = synthesizeHeuristicSummaries.find(FD);
      It != synthesizeHeuristicSummaries.end())
    return &It->second;

  // `free`-like: an opaque function taking one raw pointer invalidates it. The
  // names: any 'free' or 'destroy', and 'WTF::fastFree'.
  if (FD->isDefined() || !FD->getIdentifier() || FD->getNumParams() != 1 ||
      !FD->getParamDecl(0)->getType()->isPointerType())
    return std::nullopt;
  StringRef Name = FD->getName();
  if (Name == "free" || Name == "destroy" ||
      FD->getQualifiedNameAsString() == "WTF::fastFree") {
    FunctionSummaryBuilder Builder;
    Builder.addInvalidated(ParamOrThis::forParam(FD->getParamDecl(0)));
    return &synthesizeHeuristicSummaries.try_emplace(FD, Builder.build())
                .first->second;
  }
  return std::nullopt;
}

} // namespace clang::lifetimes

namespace llvm {
using clang::lifetimes::ParamOrThis;

unsigned DenseMapInfo<ParamOrThis>::getHashValue(const T &V) {
  return DenseMapInfo<int>::getHashValue(V.Index);
}

bool DenseMapInfo<ParamOrThis>::isEqual(const T &A, const T &B) {
  return A == B;
}
} // namespace llvm
