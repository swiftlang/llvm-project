//===- Checker.cpp - C++ Lifetime Safety Checker ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the LifetimeChecker, which detects use-after-free
// errors by checking if live origins hold loans that have expired.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/Checker.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Facts.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeAnnotations.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSummary.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LiveOrigins.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LoanPropagation.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Loans.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/AnalysisDeclContext.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TimeProfiler.h"
#include <optional>

namespace clang::lifetimes::internal {

static bool causingFactDominatesExpiry(LivenessKind K) {
  switch (K) {
  case LivenessKind::Must:
    return true;
  case LivenessKind::Maybe:
  case LivenessKind::Dead:
    return false;
  }
  llvm_unreachable("unknown liveness kind");
}

namespace {

/// Struct to store the complete context for a potential lifetime violation.
struct PendingWarning {
  SourceLocation ExpiryLoc; // Where the loan expired.
  llvm::PointerUnion<const UseFact *, const OriginEscapesFact *> CausingFact;
  const Expr *MovedExpr;
  const Expr *InvalidatedByExpr;
  bool CausingFactDominatesExpiry;
};

/// \return the reference parameters of \p FD. Only they can carry a capture
/// back to the caller; writes to a by-value parameter are local to the callee.
static auto referenceParams(const FunctionDecl *FD) {
  return llvm::make_filter_range(FD->parameters(), [](const ParmVarDecl *PVD) {
    return PVD->getType()->isReferenceType();
  });
}

/// \return true if \p L is a placeholder loan of an inner origin of a
/// parameter (`$p.*`).
bool isInnerPlaceholderLoan(const Loan &L) {
  const AccessPath &Path = L.getAccessPath();
  return Path.getAsPlaceholderBase() && !Path.getElements().empty() &&
         Path.getElements().front().isInterior();
}

/// \return the FieldSet of the placeholder base of \p L. Undefined if the base
/// is not a placeholder.
/// If \p L does not represent fields of the base, the field set is empty.
/// If \p L represents sub-objects of fields of the base, the field set only
/// consists of direct fields of the base.
FieldSet getFieldsOfPlaceholderBaseForSummary(const Loan &L) {
  const AccessPath &Path = L.getAccessPath();
  llvm::ArrayRef<PathElement> Elements = Path.getElements();

  if (Elements.empty() || !Elements.front().isField())
    return FieldSet{};
  return FieldSet::of(Elements.front().getFieldDecl());
}

/// A parameter or `this`, and a Level
using LeveledParamOrThis = std::pair<ParamOrThis, Level>;

using AnnotationTarget =
    llvm::PointerUnion<const ParmVarDecl *, const CXXMethodDecl *>;
using EscapingTarget = LifetimeSafetySemaHelper::EscapingTarget;

LeveledParamOrThis getLeveledParamOrThisFromLoan(const Loan &L) {
  const PlaceholderBase *PB = L.getAccessPath().getAsPlaceholderBase();
  assert(PB && "not a placeholder loan");
  if (const auto *PVD = PB->getParmVarDecl())
    return {ParamOrThis::forParam(PVD),
            isInnerPlaceholderLoan(L) ? Level::ReferentValue : Level::Value};
  assert(PB->getImplicitThisParent() && !isInnerPlaceholderLoan(L) &&
         "not a parameter or `this` loan");
  return {ParamOrThis::forThis(), Level::Value};
}

class LifetimeChecker {
private:
  llvm::DenseMap<LoanID, PendingWarning> FinalWarningsMap;
  llvm::DenseMap<AnnotationTarget, EscapingTarget> AnnotationWarningsMap;
  llvm::DenseMap<const ParmVarDecl *, EscapingTarget> NoescapeWarningsMap;
  llvm::DenseMap<const FieldDecl *, const Expr *> FieldOriginEscapesMap;
  llvm::DenseSet<const Decl *> VerifiedLiftimeboundEscapes;
  // The params/`this`, at a level, that the return value may depend on, with
  // the fields they are stored in.
  llvm::DenseMap<LeveledParamOrThis, FieldSet> LifetimeBoundSumAsserts;
  // The parameters whose loans reach any escape sink, moved or not.
  llvm::DenseSet<const ParmVarDecl *> MayEscapeParams;
  // The loans of parameters and 'this' at function entry and exit, to find
  // what captures them.
  struct PrePostParamLoans {
    // The block-local origins of the value of the reference parameters. Only
    // reference type parameters can capture.
    llvm::DenseSet<OriginID> BlockOriginsOfInterest;
    // Origin of a parameter or 'this' (at any level) -> its placeholder loan,
    // at entry.
    llvm::DenseMap<OriginID, LoanID> PreState;
    // Block-local origin -> its loans after the last fact that defines it. A
    // block-local origin lives in one block, whose facts are visited in flow
    // order, so the last one gives the state at the end of the block.
    // Persistent origins can always be found at the exit block.
    llvm::DenseMap<OriginID, LoanSet> PostState;
  } ParamLoans;
  // The parameters and `this` that the function may invalidate, with the
  // fields of `this` for the invalidated fields.
  llvm::DenseMap<ParamOrThis, FieldSet> InvalidatedEntities;
  const LoanPropagationAnalysis &LoanPropagation;
  const MovedLoansAnalysis &MovedLoans;
  const LiveOriginsAnalysis &LiveOrigins;
  FactManager &FactMgr;
  LifetimeSafetySemaHelper *SemaHelper;
  ASTContext &AST;
  const CFG *Cfg;
  const Decl *FD;
  const LifetimeSafetyOpts &LSOpts;
  // Engaged if the caller wants the summary of the inferred results.
  std::optional<FunctionSummaryBuilder> SummaryBuilder;

  static SourceLocation
  GetFactLoc(llvm::PointerUnion<const UseFact *, const OriginEscapesFact *> F) {
    if (const auto *UF = dyn_cast<const UseFact *>(F))
      return UF->getUseExpr()->getExprLoc();
    if (const auto *OEF = dyn_cast<const OriginEscapesFact *>(F)) {
      if (auto *ReturnEsc = dyn_cast<ReturnEscapeFact>(OEF))
        return ReturnEsc->getReturnExpr()->getExprLoc();
      if (auto *FieldEsc = dyn_cast<FieldEscapeFact>(OEF))
        return FieldEsc->getFieldDecl()->getLocation();
    }
    llvm_unreachable("unhandled causing fact in PointerUnion");
  }

public:
  LifetimeChecker(const LoanPropagationAnalysis &LoanPropagation,
                  const MovedLoansAnalysis &MovedLoans,
                  const LiveOriginsAnalysis &LiveOrigins, FactManager &FM,
                  AnalysisDeclContext &ADC,
                  LifetimeSafetySemaHelper *SemaHelper,
                  const LifetimeSafetyOpts &LSOpts, bool BuildSummary = false)
      : LoanPropagation(LoanPropagation), MovedLoans(MovedLoans),
        LiveOrigins(LiveOrigins), FactMgr(FM), SemaHelper(SemaHelper),
        AST(ADC.getASTContext()), Cfg(ADC.getCFG()), FD(ADC.getDecl()),
        LSOpts(LSOpts) {
    if (BuildSummary) {
      SummaryBuilder.emplace();
      computeBlockOriginsOfInterest();
    }
    for (const CFGBlock *B : *ADC.getAnalysis<PostOrderCFGView>())
      for (const Fact *F : FactMgr.getFacts(B)) {
        if (const auto *EF = F->getAs<ExpireFact>())
          checkExpiry(EF);
        else if (const auto *IOF = F->getAs<InvalidateOriginFact>())
          checkInvalidation(IOF);
        else if (const auto *OEF = F->getAs<OriginEscapesFact>())
          checkAnnotations(OEF);
        if (SummaryBuilder) {
          recordParamLoans(F);
          if (const auto *IOF = F->getAs<InvalidateOriginFact>())
            recordInvalidatedEntities(IOF);
        }
      }
    if (SummaryBuilder) {
      addToInferenceSummary();
      return;
    }
    issuePendingWarnings();
    suggestAnnotations();
    if (LSOpts.CheckFieldOriginEscape)
      reportFieldOriginEscapes();
    if (LSOpts.CheckNoescapeViolations)
      reportNoescapeViolations();
    if (LSOpts.CheckLifetimeboundViolations)
      reportLifetimeboundViolations();
    if (LSOpts.CheckMisplacedLifetimebound)
      reportMisplacedLifetimebound();
    if (LSOpts.CheckInapplicableLifetimebound)
      reportInapplicableLifetimebound();
    //  Annotation inference is currently guarded by a frontend flag. In the
    //  future, this might be replaced by a design that differentiates between
    //  explicit and inferred findings with separate warning groups.
    if (AST.getLangOpts().EnableLifetimeSafetyInference)
      inferAnnotations();
  }

  /// \return the summary of the function, which must have been requested.
  FunctionSummary takeSummary() {
    assert(SummaryBuilder && "the summary was not requested");
    return SummaryBuilder->build();
  }

  /// Checks if an escaping origin holds a placeholder loan, indicating a
  /// missing [[clang::lifetimebound]] annotation or a violation of
  /// [[clang::noescape]].
  void checkAnnotations(const OriginEscapesFact *OEF) {
    OriginID EscapedOID = OEF->getEscapedOriginID();
    LoanSet EscapedLoans = LoanPropagation.getLoans(EscapedOID, OEF);
    auto CheckParam = [&](const ParmVarDecl *PVD, bool IsMoved) {
      // NoEscape param should not escape.
      if (PVD->hasAttr<NoEscapeAttr>()) {
        if (auto *ReturnEsc = dyn_cast<ReturnEscapeFact>(OEF))
          NoescapeWarningsMap.try_emplace(PVD, ReturnEsc->getReturnExpr());
        if (auto *FieldEsc = dyn_cast<FieldEscapeFact>(OEF))
          NoescapeWarningsMap.try_emplace(PVD, FieldEsc->getFieldDecl());
        if (auto *GlobalEsc = dyn_cast<GlobalEscapeFact>(OEF))
          NoescapeWarningsMap.try_emplace(PVD, GlobalEsc->getGlobal());
        return;
      }
      // Skip annotation suggestion for moved loans, as ownership transfer
      // obscures the lifetime relationship (e.g., shared_ptr from unique_ptr).
      if (IsMoved)
        return;
      if (PVD->hasAttr<LifetimeBoundAttr>()) {
        // Track that this lifetimebound parameter correctly escapes
        // (via return or via field assignment in a constructor).
        if (isa<ReturnEscapeFact>(OEF) ||
            (isa<FieldEscapeFact>(OEF) && isa<CXXConstructorDecl>(FD)))
          VerifiedLiftimeboundEscapes.insert(PVD);
      } else {
        // Otherwise, suggest lifetimebound for parameter escaping through
        // return or a field in constructor.
        if (auto *ReturnEsc = dyn_cast<ReturnEscapeFact>(OEF))
          AnnotationWarningsMap.try_emplace(PVD, ReturnEsc->getReturnExpr());
        else if (auto *FieldEsc = dyn_cast<FieldEscapeFact>(OEF);
                 FieldEsc && isa<CXXConstructorDecl>(FD)) {
          // Disable inference for pointers being captured by an owner type,
          // as owners typically consume these pointers rather than borrow them.
          if (!isOwnerPtrCtor(dyn_cast<CXXConstructorDecl>(FD), PVD))
            AnnotationWarningsMap.try_emplace(PVD, FieldEsc->getFieldDecl());
        }
      }
      // TODO: Suggest lifetime_capture_by(this) for parameter escaping to a
      // field!
    };
    auto CheckImplicitThis = [&](const CXXMethodDecl *MD) {
      if (auto *ReturnEsc = dyn_cast<ReturnEscapeFact>(OEF)) {
        if (implicitObjectParamIsLifetimeBound(MD))
          VerifiedLiftimeboundEscapes.insert(MD);
        else
          AnnotationWarningsMap.try_emplace(MD, ReturnEsc->getReturnExpr());
      }
    };
    // Identifies methods whose return value derives from a field's origin
    // rather than *this, e.g., Span::data() or Vector::data().
    auto CheckField = [&](const FieldDecl *Field) {
      if (!LSOpts.CheckFieldOriginEscape)
        return;
      // Lifetimebound marks the field as owned by *this (e.g., Vector::data()).
      const auto *MD = dyn_cast<CXXMethodDecl>(FD);
      if (MD && implicitObjectParamIsLifetimeBound(MD))
        return;
      if (auto *ReturnEsc = dyn_cast<ReturnEscapeFact>(OEF)) {
        // Ignore field loans in inner origins, e.g., returning a reference to
        // a view field borrows *this.
        if (OriginList *RetList = FactMgr.getOriginMgr().getOrCreateList(
                ReturnEsc->getReturnExpr());
            RetList && RetList->getOuterOriginID() == EscapedOID)
          FieldOriginEscapesMap.try_emplace(Field, ReturnEsc->getReturnExpr());
      }
    };
    auto AddLifetimeBoundParamOrThis = [&](const Loan &L, bool IsMoved) {
      if (isa<ReturnEscapeFact>(OEF) && !IsMoved)
        LifetimeBoundSumAsserts[getLeveledParamOrThisFromLoan(L)] = FieldSet{};
      if (auto *CtorD = dyn_cast<CXXConstructorDecl>(FD);
          CtorD && !isGslOwnerType(CtorD->getParent()))
        if (auto *FEF = dyn_cast<FieldEscapeFact>(OEF); FEF && !IsMoved) {
          auto P = getLeveledParamOrThisFromLoan(L);
          auto SingleFieldSet = FieldSet::of(FEF->getFieldDecl());
          auto It = LifetimeBoundSumAsserts.find(P);
          if (It != LifetimeBoundSumAsserts.end())
            It->second.merge(SingleFieldSet);
          else
            LifetimeBoundSumAsserts[P] = SingleFieldSet;
        }
    };
    auto MovedAtEscape = MovedLoans.getMovedLoans(OEF);
    for (LoanID LID : EscapedLoans) {
      const Loan *L = FactMgr.getLoanMgr().getLoan(LID);
      const PlaceholderBase *PB = L->getAccessPath().getAsPlaceholderBase();
      bool IsMoved = bool(MovedAtEscape.lookup(LID));

      if (!PB)
        continue;
      // Collect the params/this, at each level, that the return value or the
      // constructed object may depend on:
      if (!PB->getFieldDecl())
        AddLifetimeBoundParamOrThis(*L, IsMoved);
      if (isInnerPlaceholderLoan(*L))
        continue;
      if (const auto *PVD = PB->getParmVarDecl()) {
        MayEscapeParams.insert(PVD);
        CheckParam(PVD, IsMoved);
      } else if (const auto *MD = PB->getImplicitThisParent())
        CheckImplicitThis(MD);
      else if (const auto *Field = PB->getFieldDecl())
        CheckField(Field);
    }
  }

  /// Adds what the analysis concluded about the function to the summary.
  void addToInferenceSummary() {
    const auto *FDecl = dyn_cast_or_null<FunctionDecl>(FD);

    if (!FDecl)
      return;

    // Add LifetimeBound:
    for (auto &[Key, Fields] : LifetimeBoundSumAsserts)
      SummaryBuilder->addLifetimeBound(Key.first, Key.second,
                                       std::move(Fields));

    // Add NoEscape:
    for (const ParmVarDecl *PVD : FDecl->parameters()) {
      QualType T = PVD->getType();
      if ((isPointerLikeType(T) || T->isReferenceType()) &&
          !MayEscapeParams.contains(PVD))
        SummaryBuilder->addNoEscape(ParamOrThis::forParam(PVD), Level::Value);
    }

    // Add CapturedBy:
    addCapturedByHelper(FDecl);

    // Add Invalidated:
    for (auto &[P, F] : InvalidatedEntities)
      SummaryBuilder->addInvalidated(P, std::move(F));
  }

  /// Records what \p IOF may invalidate, from the placeholder loans that the
  /// invalidated origin holds: a parameter, `this`, or a field of `this`.
  void recordInvalidatedEntities(const InvalidateOriginFact *IOF) {
    auto Add = [&](ParamOrThis P, FieldSet F) {
      InvalidatedEntities.try_emplace(P, F).first->second.merge(F);
    };
    for (LoanID LID :
         LoanPropagation.getLoans(IOF->getInvalidatedOrigin(), IOF)) {
      const Loan *L = FactMgr.getLoanMgr().getLoan(LID);
      const auto *PB = L->getAccessPath().getAsPlaceholderBase();

      // Inner-level placeholder loans (`$p.*`) are not recorded.
      if (!PB || isInnerPlaceholderLoan(*L))
        continue;

      FieldSet Fields = getFieldsOfPlaceholderBaseForSummary(*L);

      if (const auto *PVD = PB->getParmVarDecl()) {
        Add(ParamOrThis::forParam(PVD), Fields);
      } else if (PB->getImplicitThisParent())
        Add(ParamOrThis::forThis(), Fields);
      else if (const FieldDecl *Field = PB->getFieldDecl())
        Add(ParamOrThis::forThis(), FieldSet::of(Field));
    }
  }

  /// \return the origins of the value of \p Q
  OriginList *getParamValueOrigins(const ParmVarDecl *Q) {
    OriginList *List = FactMgr.getOriginMgr().getOrCreateList(Q);
    return List ? List->peelOuterOrigin() : nullptr;
  }

  /// Populates BlockOriginsOfInterest.
  void computeBlockOriginsOfInterest() {
    const auto *FDecl = dyn_cast_or_null<FunctionDecl>(FD);
    if (!FDecl)
      return;
    for (const ParmVarDecl *PVD : referenceParams(FDecl)) {
      if (OriginList *QValue = getParamValueOrigins(PVD))
        if (OriginID OID = QValue->getOuterOriginID();
            !FactMgr.getPersistentOrigins().test(OID.Value))
          ParamLoans.BlockOriginsOfInterest.insert(OID);
    }
  }

  /// Records the placeholder loan issued by \p F into PreState (once, at
  /// creation). Separately, if the origin that \p F defines is one of
  /// BlockOriginsOfInterest, records its current loans into PostState,
  /// overwriting any earlier entry.
  ///
  /// These information has to be collected along the analysis (Fact walk).
  void recordParamLoans(const Fact *F) {
    OriginID OID;
    if (const auto *IF = F->getAs<IssueFact>()) {
      OID = IF->getOriginID();
      const Loan *L = FactMgr.getLoanMgr().getLoan(IF->getLoanID());
      if (const PlaceholderBase *PB =
              L->getAccessPath().getAsPlaceholderBase()) {
        if (PB->getParmVarDecl() || PB->getImplicitThisParent())
          ParamLoans.PreState.try_emplace(OID, IF->getLoanID());
      }
    } else if (const auto *OFF = F->getAs<OriginFlowFact>()) {
      OID = OFF->getDestOriginID();
    } else if (const auto *KOF = F->getAs<KillOriginFact>()) {
      OID = KOF->getKilledOrigin();
    } else
      return;
    if (ParamLoans.BlockOriginsOfInterest.contains(OID))
      ParamLoans.PostState.insert_or_assign(OID,
                                            LoanPropagation.getLoans(OID, F));
  }

  /// \return the loans of the origin of a parameter at function exit.
  LoanSet getParamLoansAtExit(OriginID OID) {
    if (FactMgr.getPersistentOrigins().test(OID.Value))
      return LoanPropagation.getLoansAtFunctionExit(OID);
    // A block-local origin is not in the exit state; see PostState.
    auto It = ParamLoans.PostState.find(OID);
    return It != ParamLoans.PostState.end() ? It->second : LoanSet(nullptr);
  }

  /// \return the entry loan of \p P at \p Lvl, if it has one. For a reference
  /// parameter, Level::Value is the loan of the reference, and
  /// Level::ReferentValue is the loan of the referenced object.
  std::optional<LoanID> getEntryLoan(ParamOrThis P,
                                     Level Lvl = Level::Value) {
    OriginList *List = nullptr;
    if (P.isThis()) {
      if (auto ThisOrigins = FactMgr.getOriginMgr().getThisOrigins())
        List = *ThisOrigins;
    } else if (const auto *FDecl = dyn_cast_or_null<FunctionDecl>(FD))
      List = FactMgr.getOriginMgr().getOrCreateList(
          FDecl->getParamDecl(P.getParamIndex()));
    if (List && Lvl == Level::ReferentValue)
      List = List->peelOuterOrigin();
    if (!List)
      return std::nullopt;
    auto It = ParamLoans.PreState.find(List->getOuterOriginID());
    if (It == ParamLoans.PreState.end())
      return std::nullopt;
    return It->second;
  }

  /// \return the fields of `this` that hold the entry loan of \p P at function
  /// exit, or nothing if no field does.
  std::optional<FieldSet> getCapturingFields(ParamOrThis P,
                                             Level Lvl = Level::Value) {
    std::optional<LoanID> EntryLoan = getEntryLoan(P, Lvl);
    if (!EntryLoan)
      return std::nullopt;

    std::optional<FieldSet> Fields;

    for (const Fact *F : FactMgr.getFacts(&Cfg->getExit()))
      if (const auto *FEF = F->getAs<FieldEscapeFact>();
          FEF && LoanPropagation.getLoans(FEF->getEscapedOriginID(), FEF)
                     .contains(*EntryLoan)) {
        FieldSet Field = FieldSet::of(FEF->getFieldDecl());
        if (Fields)
          Fields->merge(Field);
        else
          Fields = std::move(Field);
      }
    return Fields;
  }

  /// Adds the CapturedBy assertions of the parameters and `this` of \p FDecl:
  /// by the reference parameters, and, for a parameter, by `this`.
  void addCapturedByHelper(const FunctionDecl *FDecl) {
    // P, a parameter or `this`, is captured by the reference parameters that
    // hold its entry loan at exit.
    auto AddCapturedByRefParam = [&](ParamOrThis P) {
      std::optional<LoanID> EntryLoan = getEntryLoan(P);
      if (!EntryLoan)
        return;
      for (const ParmVarDecl *Q : referenceParams(FDecl)) {
        if (ParamOrThis::forParam(Q) == P)
          continue;
        OriginList *QValue = getParamValueOrigins(Q);
        if (!QValue || !getParamLoansAtExit(QValue->getOuterOriginID())
                            .contains(*EntryLoan))
          continue;
        SummaryBuilder->addCapturedByAssertion(
            P, Level::Value, ParamOrThis::forParam(Q), Level::Value);
      }
    };

    const auto *MD = dyn_cast<CXXMethodDecl>(FDecl);
    bool IsMethod = MD && MD->isImplicitObjectMemberFunction();
    // A constructor's field capture is lifetimebound instead; a destructor's
    // object is going away, so it captures nothing.
    bool IsMethodButNotCtorDtor =
        IsMethod && !isa<CXXConstructorDecl, CXXDestructorDecl>(MD);

    // A parameter is captured by `this`, through the fields of `this` that hold
    // its entry loan at exit.
    auto AddParamCapturedByThis = [&](const ParmVarDecl *PVD) {
      if (!IsMethodButNotCtorDtor)
        return;

      ParamOrThis P = ParamOrThis::forParam(PVD);
      if (std::optional<FieldSet> Fields = getCapturingFields(P))
        SummaryBuilder->addCapturedByAssertion(
            P, Level::Value, ParamOrThis::forThis(), Level::Value,
            std::move(*Fields));
    };

    for (const ParmVarDecl *PVD : FDecl->parameters()) {
      AddCapturedByRefParam(ParamOrThis::forParam(PVD));
      AddParamCapturedByThis(PVD);
    }
    if (IsMethod)
      AddCapturedByRefParam(ParamOrThis::forThis());
  }

  /// Checks for use-after-free & use-after-return errors when an access path
  /// expires (e.g., a variable goes out of scope).
  ///
  /// When a path expires, all loans prefixed by that path expire. For example,
  /// if `x` expires, loans to `x`, `x.field`, and `x.field.*` all expire.
  /// This method examines all live origins and reports warnings for loans they
  /// hold that are prefixed by the expired path.
  void checkExpiry(const ExpireFact *EF) {
    const AccessPath &ExpiredPath = EF->getAccessPath();
    LiveOriginSet Origins = LiveOrigins.getLiveOriginsAt(EF);
    for (const LivenessMap &Live : {Origins.Persistent, Origins.BlockLocal})
      for (auto &[OID, LiveInfo] : Live) {
        LoanSet HeldLoans = LoanPropagation.getLoans(OID, EF);
        for (LoanID HeldLoanID : HeldLoans) {
          const Loan *HeldLoan = FactMgr.getLoanMgr().getLoan(HeldLoanID);
          if (!ExpiredPath.isPrefixOf(HeldLoan->getAccessPath()))
            continue;
          // HeldLoan is expired because its base or itself is expired.
          PendingWarning &CurWarning = FinalWarningsMap[HeldLoan->getID()];
          const Expr *MovedExpr = nullptr;
          if (auto *ME = MovedLoans.getMovedLoans(EF).lookup(HeldLoanID))
            MovedExpr = *ME;
          // Skip if we already have a dominating causing fact.
          if (CurWarning.CausingFactDominatesExpiry)
            continue;
          if (causingFactDominatesExpiry(LiveInfo.Kind))
            CurWarning.CausingFactDominatesExpiry = true;
          CurWarning.CausingFact = LiveInfo.CausingFact;
          CurWarning.ExpiryLoc = EF->getExpiryLoc();
          CurWarning.MovedExpr = MovedExpr;
          CurWarning.InvalidatedByExpr = nullptr;
        }
      }
  }

  /// Checks for use-after-invalidation errors when a container is modified.
  ///
  /// When a container is invalidated, loans pointing into its interior are
  /// invalidated. For example, if container `v` is invalidated, iterators with
  /// loans to `v.*` are invalidated. This method finds live origins holding
  /// such loans and reports warnings. A loan is invalidated if its path extends
  /// an invalidated container's path (e.g., `v.*` extends `v`).
  void checkInvalidation(const InvalidateOriginFact *IOF) {
    OriginID InvalidatedOrigin = IOF->getInvalidatedOrigin();
    /// Get loans directly pointing to the invalidated container
    LoanSet DirectlyInvalidatedLoans =
        LoanPropagation.getLoans(InvalidatedOrigin, IOF);
    auto IsInvalidated = [&](const Loan *L) {
      for (LoanID InvalidID : DirectlyInvalidatedLoans) {
        const Loan *InvalidL = FactMgr.getLoanMgr().getLoan(InvalidID);
        if (InvalidL->getAccessPath().isPrefixOf(L->getAccessPath()))
          return true;
      }
      return false;
    };
    // For each live origin, check if it holds an invalidated loan and report.
    LiveOriginSet Origins = LiveOrigins.getLiveOriginsAt(IOF);
    for (const LivenessMap &Live : {Origins.Persistent, Origins.BlockLocal})
      for (auto &[OID, LiveInfo] : Live) {
        LoanSet HeldLoans = LoanPropagation.getLoans(OID, IOF);
        for (LoanID LiveLoanID : HeldLoans)
          if (const Loan *LiveLoan = FactMgr.getLoanMgr().getLoan(LiveLoanID);
              !isInnerPlaceholderLoan(*LiveLoan) && IsInvalidated(LiveLoan)) {
            bool CurDomination = causingFactDominatesExpiry(LiveInfo.Kind);
            bool LastDomination =
                FinalWarningsMap.lookup(LiveLoanID).CausingFactDominatesExpiry;
            if (!LastDomination) {
              FinalWarningsMap[LiveLoanID] = {
                  /*ExpiryLoc=*/{},
                  /*CausingFact=*/LiveInfo.CausingFact,
                  /*MovedExpr=*/nullptr,
                  /*InvalidatedByExpr=*/IOF->getInvalidationExpr(),
                  /*CausingFactDominatesExpiry=*/CurDomination};
            }
          }
      }
  }

  void issuePendingWarnings() {
    llvm::TimeTraceScope TimeTrace("IssuePendingWarnings");
    if (!SemaHelper)
      return;
    for (const auto &[LID, Warning] : FinalWarningsMap) {
      const Loan *L = FactMgr.getLoanMgr().getLoan(LID);
      const Expr *IssueExpr = L->getIssueExpr();
      const ParmVarDecl *InvalidatedPVD = nullptr;
      if (const PlaceholderBase *PB = L->getAccessPath().getAsPlaceholderBase())
        InvalidatedPVD = PB->getParmVarDecl();

      llvm::PointerUnion<const UseFact *, const OriginEscapesFact *>
          CausingFact = Warning.CausingFact;
      const Expr *MovedExpr = Warning.MovedExpr;
      SourceLocation ExpiryLoc = Warning.ExpiryLoc;

      if (const auto *UF = dyn_cast<const UseFact *>(CausingFact)) {
        llvm::SmallVector<const Expr *> ExprChain =
            getExprChain(LoanPropagation.buildOriginFlowChain(UF, LID, Cfg));
        const UseSite &Use = UF->getUseSite();
        if (Warning.InvalidatedByExpr) {
          if (IssueExpr)
            // Use-after-invalidation of an object on stack.
            SemaHelper->reportUseAfterInvalidation(
                IssueExpr, Use, Warning.InvalidatedByExpr, ExprChain);
          else if (InvalidatedPVD)
            // Use-after-invalidation of a parameter.
            SemaHelper->reportUseAfterInvalidation(
                InvalidatedPVD, Use, Warning.InvalidatedByExpr, ExprChain);

        } else
          // Scope-based expiry (use-after-scope).
          SemaHelper->reportUseAfterScope(IssueExpr, Use, MovedExpr, ExpiryLoc,
                                          ExprChain);

      } else if (const auto *OEF =
                     dyn_cast<const OriginEscapesFact *>(CausingFact)) {
        llvm::SmallVector<const Expr *> ExprChain =
            getExprChain(LoanPropagation.buildOriginFlowChain(OEF, LID, Cfg));
        if (Warning.InvalidatedByExpr) {
          if (const auto *FieldEscape = dyn_cast<FieldEscapeFact>(OEF)) {
            // Invalidated object escapes to a field.
            if (IssueExpr)
              // Invalidated object on stack escapes to a field.
              SemaHelper->reportInvalidatedField(
                  IssueExpr, FieldEscape->getFieldDecl(),
                  Warning.InvalidatedByExpr, ExprChain);
            else if (InvalidatedPVD)
              // Invalidated parameter escapes to a field.
              SemaHelper->reportInvalidatedField(
                  InvalidatedPVD, FieldEscape->getFieldDecl(),
                  Warning.InvalidatedByExpr, ExprChain);
          } else if (const auto *GlobalEscape =
                         dyn_cast<GlobalEscapeFact>(OEF)) {
            // Invalidated object escapes to global or static storage.
            if (IssueExpr)
              // Invalidated object on stack escapes to global or static
              // storage.
              SemaHelper->reportInvalidatedGlobal(
                  IssueExpr, GlobalEscape->getGlobal(),
                  Warning.InvalidatedByExpr, ExprChain);
            else if (InvalidatedPVD)
              // Invalidated parameter escapes to global or static storage.
              SemaHelper->reportInvalidatedGlobal(
                  InvalidatedPVD, GlobalEscape->getGlobal(),
                  Warning.InvalidatedByExpr, ExprChain);
          } else if (isa<ReturnEscapeFact>(OEF)) {
            // FIXME: Diagnose invalidated return escapes separately.
          } else if (const auto *ThisEscape = dyn_cast<ThisEscapeFact>(OEF)) {
            // A loan still held by the object at the method's exit was
            // invalidated.
            llvm::SmallVector<const Expr *> ExprChain = getExprChain(
                LoanPropagation.buildOriginFlowChain(OEF, LID, Cfg));
            UseSite Use = UseSite::at(ThisEscape->getLoc());
            if (IssueExpr)
              SemaHelper->reportUseAfterInvalidation(
                  IssueExpr, Use, Warning.InvalidatedByExpr, ExprChain);
            else if (InvalidatedPVD)
              SemaHelper->reportUseAfterInvalidation(
                  InvalidatedPVD, Use, Warning.InvalidatedByExpr, ExprChain);
          } else
            llvm_unreachable("Unhandled OriginEscapesFact type");
        } else if (const auto *RetEscape = dyn_cast<ReturnEscapeFact>(OEF)) {
          // Return stack address.
          SemaHelper->reportUseAfterReturn(
              IssueExpr, RetEscape->getReturnExpr(), MovedExpr, ExprChain);
        } else if (const auto *FieldEscape = dyn_cast<FieldEscapeFact>(OEF)) {
          // Dangling field.
          bool IsCapturedByLambda =
              FactMgr.isFieldCapturedByLambda(FieldEscape->getFieldDecl());
          SemaHelper->reportDanglingField(
              IssueExpr, FieldEscape->getFieldDecl(), MovedExpr,
              IsCapturedByLambda, ExpiryLoc, ExprChain);
        } else if (const auto *GlobalEscape = dyn_cast<GlobalEscapeFact>(OEF)) {
          // Global escape.
          bool IsMain = false;
          if (const auto *Func = dyn_cast_if_present<FunctionDecl>(FD))
            IsMain = Func->isMain();
          SemaHelper->reportDanglingGlobal(IssueExpr, GlobalEscape->getGlobal(),
                                           MovedExpr, ExpiryLoc, ExprChain,
                                           IsMain);
        } else if (const auto *ThisEscape = dyn_cast<ThisEscapeFact>(OEF))
          // A borrow is still held by the object (e.g. captured via
          // lifetime_capture_by_this) when the captured local goes out of
          // scope; reuse the use-after-scope diagnostic at the capturing
          // method.
          SemaHelper->reportUseAfterScope(
              IssueExpr, UseSite::at(ThisEscape->getLoc()), MovedExpr,
              ExpiryLoc,
              getExprChain(
                  LoanPropagation.buildOriginFlowChain(OEF, LID, Cfg)));
        else
          llvm_unreachable("Unhandled OriginEscapesFact type");
      } else
        llvm_unreachable("Unhandled CausingFact type");
    }
  }

  // Returns declarations that should be annotated with lifetime attributes
  // in order to annotate FDef: the canonical declaration and the earliest
  // redeclarations in each other file. This defines the placement policy for
  // lifetime annotations. Each target is paired with its corresponding warning
  // scope.
  llvm::SmallVector<std::pair<const FunctionDecl *, WarningScope>, 2>
  getTargetDeclsForAttr(const FunctionDecl *FDef) {
    if (!FDef)
      return {};

    assert(FDef->isThisDeclarationADefinition() &&
           "Expected FunctionDecl to be a definition");

    const auto &SM = FDef->getASTContext().getSourceManager();

    auto GetFile = [&SM](const FunctionDecl *FD) {
      return SM.getFileID(SM.getExpansionLoc(FD->getLocation()));
    };

    const FileID DefFile = GetFile(FDef);
    const FunctionDecl *CanonicalDecl = FDef->getCanonicalDecl();
    llvm::SmallVector<std::pair<const FunctionDecl *, WarningScope>, 2> Targets{
        {CanonicalDecl, GetFile(CanonicalDecl) == DefFile
                            ? WarningScope::IntraTU
                            : WarningScope::CrossTU}};

    // Find the earliest redeclaration in each file other than the definition
    // file.
    auto AddCrossTUDecl = [&](const FunctionDecl *FD) {
      FileID File = GetFile(FD);
      if (File == DefFile)
        return;
      for (auto [SeenFD, _] : Targets)
        if (GetFile(SeenFD) == File)
          return;
      Targets.push_back({FD, WarningScope::CrossTU});
    };

    // We iterate in reverse order (from most recent to oldest) to find
    // the first declaration in each file.

    // Store in temporary variable to manually extend lifetime
    auto redecls = llvm::to_vector(FDef->redecls());

    for (const FunctionDecl *Redecl : llvm::reverse(redecls))
      AddCrossTUDecl(Redecl);

    return Targets;
  }

  void suggestWithScopeForParmVar(const ParmVarDecl *PVD,
                                  EscapingTarget EscapeTarget) {
    if (llvm::isa<const VarDecl *>(EscapeTarget))
      return;

    for (auto [Decl, Scope] : getTargetDeclsForAttr(cast<FunctionDecl>(FD))) {
      const auto *ParmToAnnotate =
          Decl->getParamDecl(PVD->getFunctionScopeIndex());
      SemaHelper->suggestLifetimeboundToParmVar(Scope, ParmToAnnotate,
                                                EscapeTarget);
    }
  }

  void suggestWithScopeForImplicitThis(const CXXMethodDecl *MD,
                                       const Expr *EscapeExpr) {
    for (auto [Decl, Scope] : getTargetDeclsForAttr(MD)) {
      SemaHelper->suggestLifetimeboundToImplicitThis(
          Scope, cast<CXXMethodDecl>(Decl), EscapeExpr);
    }
  }

  void suggestAnnotations() {
    if (!SemaHelper)
      return;
    if (!LSOpts.SuggestAnnotations)
      return;
    llvm::TimeTraceScope TimeTrace("SuggestAnnotations");
    for (auto [Target, EscapeTarget] : AnnotationWarningsMap) {
      if (const auto *PVD = dyn_cast<const ParmVarDecl *>(Target))
        suggestWithScopeForParmVar(PVD, EscapeTarget);
      else if (const auto *MD = dyn_cast<const CXXMethodDecl *>(Target)) {
        if (const auto *EscapeExpr = dyn_cast<const Expr *>(EscapeTarget))
          suggestWithScopeForImplicitThis(MD, EscapeExpr);
        else
          llvm_unreachable("Implicit this can only escape via Expr (return)");
      }
    }
  }

  void reportFieldOriginEscapes() {
    if (!SemaHelper)
      return;
    const auto *MD = dyn_cast<CXXMethodDecl>(FD);
    if (!MD)
      return;
    llvm::TimeTraceScope TimeTrace("ReportFieldOriginEscapes");
    for (auto [Field, EscapeExpr] : FieldOriginEscapesMap)
      SemaHelper->reportFieldOriginEscape(MD, Field, EscapeExpr);
  }

  void reportNoescapeViolations() {
    llvm::TimeTraceScope TimeTrace("ReportNoescapeViolations");
    for (auto [PVD, EscapeTarget] : NoescapeWarningsMap) {
      if (const auto *E = dyn_cast<const Expr *>(EscapeTarget))
        SemaHelper->reportNoescapeViolation(PVD, E);
      else if (const auto *FD = dyn_cast<const FieldDecl *>(EscapeTarget))
        SemaHelper->reportNoescapeViolation(PVD, FD);
      else if (const auto *G = dyn_cast<const VarDecl *>(EscapeTarget))
        SemaHelper->reportNoescapeViolation(PVD, G);
      else
        llvm_unreachable("Unhandled EscapingTarget type");
    }
  }

  void reportLifetimeboundViolations() {
    llvm::TimeTraceScope TimeTrace("ReportLifetimeboundViolations");
    if (!isa<FunctionDecl>(FD))
      return;
    if (const auto *MD = dyn_cast<CXXMethodDecl>(FD);
        MD && getImplicitObjectParamLifetimeBoundAttr(MD) &&
        !VerifiedLiftimeboundEscapes.contains(MD))
      SemaHelper->reportLifetimeboundViolation(MD);
    for (const ParmVarDecl *PVD : cast<FunctionDecl>(FD)->parameters()) {
      if (!PVD->hasAttr<LifetimeBoundAttr>())
        continue;
      bool isImplicit = PVD->getAttr<LifetimeBoundAttr>()->isImplicit();
      bool Escapes = VerifiedLiftimeboundEscapes.contains(PVD);
      assert((!isImplicit || Escapes || isInStlNamespace(FD)) &&
             "Implicit lifetimebound parameters "
             "should escape through return");
      if (!isImplicit && !Escapes)
        SemaHelper->reportLifetimeboundViolation(PVD);
    }
  }

  // Reports lifetimebound attributes that are placed on a function definition
  // but not on the corresponding declaration.
  void reportMisplacedLifetimebound() {
    llvm::TimeTraceScope TimeTrace("ReportMisplacedLifetimebound");
    const FunctionDecl *FDef = dyn_cast<FunctionDecl>(FD);
    if (!FDef)
      return;

    auto TargetDecls = getTargetDeclsForAttr(FDef);
    // Check if implicit 'this' has lifetimebound on definition but not on
    // declaration.
    if (const auto *MDef = dyn_cast<CXXMethodDecl>(FDef);
        MDef && getDirectImplicitObjectLifetimeBoundAttr(MDef))
      for (auto [Decl, Scope] : TargetDecls) {
        const auto *MDecl = cast<CXXMethodDecl>(Decl);
        if (!getDirectImplicitObjectLifetimeBoundAttr(MDecl))
          SemaHelper->reportMisplacedLifetimebound(Scope, MDef, MDecl);
      }

    // Check each parameter for explicit lifetimebound on definition but not on
    // declaration.
    for (const auto *PDef : FDef->parameters()) {
      const auto *Attr = PDef->getAttr<LifetimeBoundAttr>();
      if (!Attr || Attr->isImplicit())
        continue;
      for (auto [Decl, Scope] : TargetDecls) {
        const auto *PDecl = Decl->getParamDecl(PDef->getFunctionScopeIndex());
        if (!PDecl->hasAttr<LifetimeBoundAttr>())
          SemaHelper->reportMisplacedLifetimebound(Scope, PDef, PDecl);
      }
    }
  }

  void reportInapplicableLifetimebound() {
    llvm::TimeTraceScope TimeTrace("ReportInapplicableLifetimebound");
    const auto *FDef = dyn_cast<FunctionDecl>(FD);
    if (!FDef)
      return;

    // If analyzed function is a template definition or an implicit
    // instantiation, skip.
    if (FDef->getTemplatedKind() == FunctionDecl::TK_FunctionTemplate ||
        FDef->getTemplateSpecializationKind() == TSK_ImplicitInstantiation)
      return;

    for (const auto &PVD : FDef->parameters())
      if (PVD->hasAttr<LifetimeBoundAttr>() &&
          !FactMgr.getOriginMgr().hasOrigins(PVD->getType(),
                                             /*IntrinsicOnly=*/true))
        SemaHelper->reportInapplicableLifetimebound(PVD);
  }

  void inferAnnotations() {
    for (auto [Target, EscapeTarget] : AnnotationWarningsMap) {
      if (const auto *MD = dyn_cast<const CXXMethodDecl *>(Target)) {
        if (!implicitObjectParamIsLifetimeBound(MD))
          SemaHelper->addLifetimeBoundToImplicitThis(cast<CXXMethodDecl>(MD));
      } else if (const auto *PVD = dyn_cast<const ParmVarDecl *>(Target)) {
        const auto *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
        if (!FD)
          continue;
        // Propagates inferred attributes via the most recent declaration to
        // ensure visibility for callers in post-order analysis.
        FD = getDeclWithMergedLifetimeBoundAttrs(FD);
        ParmVarDecl *InferredPVD = const_cast<ParmVarDecl *>(
            FD->getParamDecl(PVD->getFunctionScopeIndex()));
        if (!InferredPVD->hasAttr<LifetimeBoundAttr>())
          InferredPVD->addAttr(
              LifetimeBoundAttr::CreateImplicit(AST, PVD->getLocation()));
      }
    }
  }

  /// Extract expressions from the origin flow chain for diagnostic purposes.
  ///
  /// Given a chain of origins that shows how a loan propagates, this function
  /// extracts the corresponding expressions for each origin. Origins that refer
  /// to declarations (rather than expressions) are skipped.
  llvm::SmallVector<const Expr *>
  getExprChain(llvm::ArrayRef<OriginID> OriginFlowChain) {
    llvm::SmallVector<const Expr *> rs;
    for (const OriginID CurrOID : OriginFlowChain)
      if (const Expr *CurrExpr =
              FactMgr.getOriginMgr().getOrigin(CurrOID).getExpr())
        rs.push_back(CurrExpr);
    return rs;
  }
};
} // namespace

FunctionSummary runLifetimeAnalysisForSummary(
    const LoanPropagationAnalysis &LP, const MovedLoansAnalysis &MovedLoans,
    const LiveOriginsAnalysis &LO, FactManager &FactMgr,
    AnalysisDeclContext &ADC, LifetimeSafetySemaHelper *SemaHelper,
    const LifetimeSafetyOpts &LSOpts) {
  llvm::TimeTraceScope TimeProfile("LifetimeChecker");
  LifetimeChecker Checker(LP, MovedLoans, LO, FactMgr, ADC, SemaHelper, LSOpts,
                          /*WantSummary=*/true);
  return Checker.takeSummary();
}

void runLifetimeChecker(const LoanPropagationAnalysis &LP,
                        const MovedLoansAnalysis &MovedLoans,
                        const LiveOriginsAnalysis &LO, FactManager &FactMgr,
                        AnalysisDeclContext &ADC,
                        LifetimeSafetySemaHelper *SemaHelper,
                        const LifetimeSafetyOpts &LSOpts) {
  llvm::TimeTraceScope TimeProfile("LifetimeChecker");
  LifetimeChecker Checker(LP, MovedLoans, LO, FactMgr, ADC, SemaHelper, LSOpts);
}

} // namespace clang::lifetimes::internal
