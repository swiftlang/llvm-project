//===- DirectViewInference.cpp - Direct view type inference ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TODOs (read before changing the rules):
//  - Base subobject capturers: LSA does not treat a base initializer as
//    capturing into `this`, so `Derived(int *q) : Base(q)` has no capture (a
//    false "no member function captures"). Inherited ctors (`using
//    Base::Base`) are not in CXXRecordDecl::ctors() either.
//  - View-type pointees and view-type fields are not owning pointers yet.
//    Self-contained types are only the arithmetic and the explicitly
//    swift_attr("Escapable") ones; none is inferred.
//  - [[gsl::Pointer]] is the attribute of a view type. Mark inferred direct
//    views with an implicit [[gsl::Pointer]] so that other records and TUs see
//    them.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/DirectViewInference.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeAnnotations.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSafety.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSummary.h"
#include "clang/Analysis/CallGraphFixPoint.h"
#include "clang/Basic/DiagnosticSema.h"

namespace clang::lifetimes {
namespace {

enum class NotDirectViewReason {
  // No member function (but dtors) captures an owning parameter in a field:
  MethodNoBorrow,
  // Member functions capture owning parameters in different fields. At is the
  // member function capturing in a field other than P, of the prior ones:
  MethodInconsistentBorrow,
  // A member function captures parameters in multiple fields:
  MethodMultipleBorrow,
  // A member function (dtors included) may invalidate the borrow capturer P:
  BorrowInvalidated,
};

// The results of the analysis. They are emitted together by flush(), after all
// records are analyzed.
class Reports {
public:
  // \p RD is a direct view type.
  void directView(const CXXRecordDecl *RD) { Items.push_back({RD}); }

  // Explains why \p RD is not a direct view type. \p At is the declaration at
  // fault. \p P is the capturer of the prior member functions, if known.
  void whyNotDirectView(const CXXRecordDecl *RD, NotDirectViewReason Why,
                        const Decl *At, FieldT P) {
    Items.push_back({RD, Why, At, P});
  }

  void flush(DiagnosticsEngine &Diags) const {
    for (const Item &I : Items) {
      if (!I.At) {
        Diags.Report(I.RD->getLocation(), diag::remark_interop_direct_view)
            << I.RD;
        continue;
      }
      Diags.Report(I.RD->getLocation(), diag::remark_interop_not_direct_view)
          << I.RD;
      auto DB = Diags.Report(I.At->getLocation(),
                             diag::note_interop_not_direct_view_reason);
      DB << static_cast<unsigned>(I.Why);
      if (const auto *D = dyn_cast_if_present<const Decl *>(I.P))
        DB << cast<NamedDecl>(D);
      else if (const auto *B =
                   dyn_cast_if_present<const CXXBaseSpecifier *>(I.P))
        DB << B->getType();
    }
  }

private:
  struct Item {
    const CXXRecordDecl *RD;
    NotDirectViewReason Why = NotDirectViewReason::MethodNoBorrow;
    const Decl *At = nullptr; // Null if RD is a direct view type.
    FieldT P = {};
  };
  llvm::SmallVector<Item> Items;
};

// \p T is self-contained, i.e., Swift escapable: an arithmetic type, or a
// record explicitly marked swift_attr("Escapable").
bool isSelfContained(QualType T) {
  if (T->isArithmeticType())
    return true;
  const CXXRecordDecl *RD = T->getAsCXXRecordDecl();
  RD = RD ? RD->getDefinition() : nullptr;
  return RD && llvm::any_of(RD->specific_attrs<SwiftAttrAttr>(),
                            [](const SwiftAttrAttr *A) {
                              return A->getAttribute() == "Escapable";
                            });
}

// \p PVD is an owning pointer: a pointer or reference to a self-contained
// type, or a gsl::Owner type.
bool isOwningPointer(const ParmVarDecl *PVD) {
  QualType T = PVD->getType();
  if (isGslOwnerType(T))
    return true;
  return (T->isPointerType() || T->isReferenceType()) &&
         isSelfContained(T->getPointeeType());
}

/// \return the fields that parameter \p Idx of \p M is captured in, or null.
/// A constructor's capture is in its lifetimebound summary, an ordinary
/// method's in its captured-by-`this` summary.
const FieldSet *getCapturingFields(const FunctionSummary &Sum,
                                   const CXXMethodDecl *M, unsigned Idx) {
  ParamOrThis P = ParamOrThis::forParam(Idx);
  if (isa<CXXConstructorDecl>(M)) {
    for (const LifetimeBoundAssertion &A : Sum.getLifetimeBoundFor(P))
      if (A.L == Level::Value && !A.F.isAll())
        return &A.F;
    return nullptr;
  }
  for (const CapturedByAssertion &A : Sum.getCapturedByFor(P))
    if (isSupportedCapturedBySummary(A) && A.Q.isThis() && !A.F.isAll())
      return &A.F;
  return nullptr;
}

/// A member function with its summary, and the parameters that it captures in
/// a field.
struct MethodInfo {
  const CXXRecordDecl *Record;
  const CXXMethodDecl *Method;
  const FunctionSummary &Sum;
  llvm::SmallVector<unsigned, 1> Captured;

  const FieldSet &fieldsOf(unsigned Idx) const {
    return *getCapturingFields(Sum, Method, Idx);
  }
};

// Rule: the member function captures parameters in one field only.
bool checkCapturesInOneField(const MethodInfo &I, const FieldT &P,
                             Reports &Out) {
  if (I.Captured.size() == 1 &&
      I.fieldsOf(I.Captured.front()).fields().size() == 1)
    return true;
  Out.whyNotDirectView(
      I.Record, NotDirectViewReason::MethodMultipleBorrow, I.Method, P);
  return false;
}

// Rule: the member function captures in the field \p P that the prior ones
// capture in. Sets \p P if it is the first one.
bool checkCapturesInSameField(const MethodInfo &I, FieldT &P, Reports &Out) {
  FieldT Curr = I.fieldsOf(I.Captured.front()).fields().front();
  if (!P.isNull() && Curr != P) {
    Out.whyNotDirectView(I.Record,
                         NotDirectViewReason::MethodInconsistentBorrow,
                         I.Method, P);
    return false;
  }
  P = Curr;
  return true;
}

/// Checks member function \p M against the capturer \p P of the prior checked
/// ones, and sets \p P if it is the first to capture. Only the captures of
/// owning parameters count; a function without one has nothing to check. The
/// rules are the check* functions, applied in order:
///   CapturesInOneField, CapturesInSameField.
/// The skipped member functions: destructors, implicit and deleted ones,
/// those without parameters, defaulted copy/move operations, those without a
/// summary (no body, or not analyzed), and members of an instantiated record
/// that are not yet instantiated.
/// \return false if \p R is not a direct view because of \p M. Reports why.
bool checkMethod(const CXXRecordDecl *R, const CXXMethodDecl *M,
                 const LifetimeInference &LI, FieldT &P, Reports &Out) {
  if (isa<CXXDestructorDecl>(M) || M->isImplicit() || M->isDeleted() ||
      M->getNumParams() == 0)
    return true;
  // Copy and move operations copy P.
  if (M->isDefaulted() &&
      (M->isCopyAssignmentOperator() || M->isMoveAssignmentOperator() ||
       (isa<CXXConstructorDecl>(M) &&
        cast<CXXConstructorDecl>(M)->isCopyOrMoveConstructor())))
    return true;
  // A member of an instantiated record that is still a template, i.e. not yet
  // instantiated, is ignored.
  if (M->isTemplateInstantiation() && !M->isDefined())
    return true;
  const FunctionDecl *Def = M->getDefinition();
  const FunctionSummary *Sum = Def ? LI.getSummary(Def) : nullptr;
  if (!Sum)
    return true;
  MethodInfo I{R, M, *Sum, {}};
  for (unsigned Idx = 0; Idx < Def->getNumParams(); ++Idx)
    if (getCapturingFields(*Sum, M, Idx) &&
        isOwningPointer(Def->getParamDecl(Idx)))
      I.Captured.push_back(Idx);
  if (I.Captured.empty())
    return true;
  return checkCapturesInOneField(I, P, Out) &&
         checkCapturesInSameField(I, P, Out);
}

// Rule: no member function of \p R, dtors included, may invalidate the borrow
// capturer \p P. The skipped ones: static, implicit and deleted ones, those
// without a summary, and members of an instantiated record that are not yet
// instantiated.
// \return false if one may, reporting why.
bool checkNoBorrowInvalidation(const CXXRecordDecl *R,
                               const LifetimeInference &LI, const FieldT &P,
                               Reports &Out) {
  for (const CXXMethodDecl *M : R->methods()) {
    if (M->isStatic() || M->isImplicit() || M->isDeleted() ||
        (M->isTemplateInstantiation() && !M->isDefined()))
      continue;
    const FunctionDecl *Def = M->getDefinition();
    const FunctionSummary *Sum = Def ? LI.getSummary(Def) : nullptr;
    if (!Sum)
      continue;
    const FieldSet *Invalidated =
        Sum->getInvalidatedFor(ParamOrThis::forThis());
    if (Invalidated && (Invalidated->isAll() ||
                        llvm::is_contained(Invalidated->fields(), P))) {
      Out.whyNotDirectView(R, NotDirectViewReason::BorrowInvalidated, M, P);
      return false;
    }
  }
  return true;
}

// Records whether \p R is a direct view or, if known, why it is not.
void checkRecord(const CXXRecordDecl *R, const LifetimeInference &LI,
                 Reports &Out) {
  if (!R->hasDefinition() || !R->isCompleteDefinition())
    return;
  FieldT P; // The capturer all member functions agree on.
  for (const CXXMethodDecl *M : R->methods())
    if (!checkMethod(R, M, LI, P, Out))
      return;
  if (P.isNull())
    Out.whyNotDirectView(R, NotDirectViewReason::MethodNoBorrow, R, P);
  else if (checkNoBorrowInvalidation(R, LI, P, Out))
    Out.directView(R);
}

} // namespace

void DirectViewInference::run(TranslationUnitDecl *TU,
                              DiagnosticsEngine &Diags) {
  // Only the summaries matter, so the analysis checks nothing.
  LifetimeSafetyOpts Opts;
  // The placeholder loans of the fields tell which field a function
  // invalidates.
  Opts.CheckFieldOriginEscape = true;
  Opts.MaxCFGBlocks =
      TU->getASTContext().getLangOpts().LifetimeSafetyMaxCFGBlocks;
  auto AnalyzeF = LI.getLifetimeInferenceAnalyzeLambda(
      Opts,
      [](const FunctionDecl *FD) {
        return cast<DeclContext>(FD)->isDependentContext();
      },
      setLifetimeSafetyCFGBuildOptions);
  CallGraphFixPoint(TU).run(AnalyzeF);

  Reports Out;
  for (const CXXRecordDecl *R : Records)
    checkRecord(R, LI, Out);
  Out.flush(Diags);
}

} // namespace clang::lifetimes
