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
//    capturing into `this`, so a direct view base is assumed to borrow, e.g.
//    `Derived() : Base(new int)` passes. Inherited ctors (`using Base::Base`)
//    are not in CXXRecordDecl::ctors() either. Only the first direct view
//    base counts.
//  - View-type pointees and view-type fields are not owning pointers yet.
//    Self-contained types are only the arithmetic and the explicitly
//    swift_attr("Escapable") ones; none is inferred.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/DirectViewInference.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeAnnotations.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSafety.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSummary.h"
#include "clang/Analysis/CallGraphFixPoint.h"
#include "clang/Basic/DiagnosticSema.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"

namespace clang::lifetimes {
namespace {

enum class NotDirectViewReason {
  // No member function (but dtors) captures a pointer-to-self-contained
  // parameter in a field, and there is no single public pointer-like field:
  NoBorrowNoPublicPointer,
  // Member functions capture pointer-to-self-contained parameters in different
  // fields. At is the member function capturing in a field other than P, of
  // the prior ones:
  MethodInconsistentBorrow,
  // A member function captures parameters in multiple fields:
  MethodMultipleBorrow,
  // A member function (dtors included) may invalidate the borrow capturer P:
  BorrowInvalidated,
  // A member function captures a pointer-to-self-contained parameter in a
  // field, while a direct view base class is the borrow capturer P:
  MethodBorrowBesidesBase,
  // A member function captures a pointer to a non-self-contained type in a
  // field:
  MethodCapturesNonSelfContained,
  // A member function captures a pointer-to-self-contained parameter in a
  // field P that is not pointer-like:
  MethodBorrowInNonPointerField,
};

// \p RD is explicitly marked self-contained: swift_attr("Escapable").
bool hasSelfContainedAttr(const CXXRecordDecl *RD) {
  return llvm::any_of(RD->specific_attrs<SwiftAttrAttr>(),
                      [](const SwiftAttrAttr *A) {
                        return A->getAttribute() == "Escapable";
                      });
}

// The results of the analysis. They are emitted together by flush(), after all
// records are analyzed.
class Reports {
public:
  // \p RD is a direct view type.
  void directView(const CXXRecordDecl *RD) { Items[RD] = {RD}; }

  // Explains why \p RD is not a direct view type. \p At is the declaration at
  // fault. \p P is the capturer of the prior member functions, if known.
  void whyNotDirectView(const CXXRecordDecl *RD, NotDirectViewReason Why,
                        const Decl *At, FieldT P) {
    Items[RD] = {RD, Why, At, P};
  }

  void flush(DiagnosticsEngine &Diags) const {
    for (const auto &[_, I] : Items) {
      if (!I.At) {
        if (hasSelfContainedAttr(I.RD))
          Diags.Report(I.RD->getLocation(),
                       diag::warn_interop_self_contained_direct_view)
              << I.RD;
        Diags.Report(I.RD->getLocation(), diag::remark_interop_direct_view)
            << I.RD;
        continue;
      }
      if (I.RD->hasAttr<PointerAttr>())
        Diags.Report(I.RD->getLocation(),
                     diag::warn_interop_gsl_pointer_not_direct_view)
            << I.RD;
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
    NotDirectViewReason Why = NotDirectViewReason::NoBorrowNoPublicPointer;
    const Decl *At = nullptr; // Null if RD is a direct view type.
    FieldT P = {};
  };
  // The latest result of each record, in the order of the first one.
  llvm::MapVector<const CXXRecordDecl *, Item> Items;
};

// \p T is self-contained, i.e., Swift escapable: an arithmetic type, or a
// record explicitly marked swift_attr("Escapable").
bool isSelfContained(QualType T) {
  if (T->isArithmeticType())
    return true;
  const CXXRecordDecl *RD = T->getAsCXXRecordDecl();
  RD = RD ? RD->getDefinition() : nullptr;
  return RD && hasSelfContainedAttr(RD);
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
  const FunctionDecl *Def;
  const FunctionSummary &Sum;
  llvm::SmallVector<unsigned, 1> Captured;

  const FieldSet &fieldsOf(unsigned Idx) const {
    return *getCapturingFields(Sum, Method, Idx);
  }
};

// Rule: the member function captures no pointer to a non-self-contained type.
// Copy and move operations copy P, so they are skipped. Then drops the
// captures of non-owning parameters: only owning ones count from here on.
bool checkNoNonSelfContainedCapture(MethodInfo &I, const FieldT &P,
                                    Reports &Out) {
  bool IsCopyOrMove =
      I.Method->isCopyAssignmentOperator() ||
      I.Method->isMoveAssignmentOperator() ||
      (isa<CXXConstructorDecl>(I.Method) &&
       cast<CXXConstructorDecl>(I.Method)->isCopyOrMoveConstructor());
  for (unsigned Idx : I.Captured) {
    const ParmVarDecl *PVD = I.Def->getParamDecl(Idx);
    QualType T = PVD->getType();
    if (!IsCopyOrMove && (T->isPointerType() || T->isReferenceType()) &&
        !isOwningPointer(PVD)) {
      Out.whyNotDirectView(
          I.Record, NotDirectViewReason::MethodCapturesNonSelfContained,
          I.Method, P);
      return false;
    }
  }
  llvm::erase_if(I.Captured, [&](unsigned Idx) {
    return !isOwningPointer(I.Def->getParamDecl(Idx));
  });
  return true;
}

// Rule: no member function captures in a field if a direct view base class is
// the borrow capturer \p P.
bool checkNoBorrowBesidesBase(const MethodInfo &I, const FieldT &P,
                              Reports &Out) {
  if (!dyn_cast_if_present<const CXXBaseSpecifier *>(P))
    return true;
  Out.whyNotDirectView(I.Record, NotDirectViewReason::MethodBorrowBesidesBase,
                       I.Method, P);
  return false;
}

// Rule: the member function captures parameters in one field only.
bool checkCapturesInOneField(const MethodInfo &I, const FieldT &P,
                             Reports &Out) {
  if (I.Captured.size() == 1 &&
      I.fieldsOf(I.Captured.front()).fields().size() == 1)
    return true;
  Out.whyNotDirectView(I.Record, NotDirectViewReason::MethodMultipleBorrow,
                       I.Method, P);
  return false;
}

// Rule: the field \p P that the member function captures in is pointer-like: a
// pointer or a gsl::Pointer type. A base class is a gsl::Pointer.
bool checkCapturesInPointerLikeField(const MethodInfo &I, const FieldT &P,
                                     Reports &Out) {
  const auto *FD =
      dyn_cast_if_present<FieldDecl>(dyn_cast_if_present<const Decl *>(P));
  if (!FD || isPointerLikeType(FD->getType()))
    return true;
  Out.whyNotDirectView(I.Record,
                       NotDirectViewReason::MethodBorrowInNonPointerField,
                       I.Method, P);
  return false;
}

// Rule: the member function captures in the field \p P that the prior ones
// capture in. Sets \p P if it is the first one.
bool checkCapturesInSameField(const MethodInfo &I, FieldT &P, Reports &Out) {
  FieldT Curr = I.fieldsOf(I.Captured.front()).fields().front();
  if (!P.isNull() && Curr != P) {
    Out.whyNotDirectView(
        I.Record, NotDirectViewReason::MethodInconsistentBorrow, I.Method, P);
    return false;
  }
  P = Curr;
  return true;
}

/// Checks member function \p M against the capturer \p P of the prior checked
/// ones, and sets \p P if it is the first to capture.
/// The skipped member functions: destructors, implicit and deleted ones, those
/// without parameters, defaulted copy/move operations, those without a summary
/// (no body, or not analyzed), and members of an instantiated record that are
/// not yet instantiated.
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
  MethodInfo I{R, M, Def, *Sum, {}};
  for (unsigned Idx = 0; Idx < Def->getNumParams(); ++Idx)
    if (getCapturingFields(*Sum, M, Idx))
      I.Captured.push_back(Idx);
  if (!checkNoNonSelfContainedCapture(I, P, Out))
    return false;
  if (I.Captured.empty())
    return true;
  return checkNoBorrowBesidesBase(I, P, Out) &&
         checkCapturesInOneField(I, P, Out) &&
         checkCapturesInSameField(I, P, Out) &&
         checkCapturesInPointerLikeField(I, P, Out);
}

// \p F is \p P, or \p F is a subobject of \p P.
bool isPartOf(FieldT F, FieldT P) {
  if (F == P)
    return true;
  const auto *B = dyn_cast_if_present<const CXXBaseSpecifier *>(P);
  const auto *FD =
      dyn_cast_if_present<FieldDecl>(dyn_cast_if_present<const Decl *>(F));
  if (!B || !FD)
    return false;
  const CXXRecordDecl *BaseRD = B->getType()->getAsCXXRecordDecl();
  const auto *Parent = dyn_cast<CXXRecordDecl>(FD->getParent());
  BaseRD = BaseRD ? BaseRD->getDefinition() : nullptr;
  return BaseRD && Parent &&
         (BaseRD->getCanonicalDecl() == Parent->getCanonicalDecl() ||
          BaseRD->isDerivedFrom(Parent));
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
    if (Invalidated &&
        (Invalidated->isAll() ||
         llvm::any_of(Invalidated->fields(),
                      [&](FieldT F) { return isPartOf(F, P); }))) {
      Out.whyNotDirectView(R, NotDirectViewReason::BorrowInvalidated, M, P);
      return false;
    }
  }
  return true;
}

// Marks the direct view \p R, unless it is already a gsl::Owner or Pointer.
void addImplicitGslPointerAttr(const CXXRecordDecl *R) {
  if (R->hasAttr<OwnerAttr>() || R->hasAttr<PointerAttr>())
    return;
  ASTContext &Ctx = R->getASTContext();
  for (Decl *Redecl : const_cast<CXXRecordDecl *>(R)->redecls())
    Redecl->addAttr(PointerAttr::CreateImplicit(Ctx, /*DerefType=*/nullptr));
}

// \return the only public pointer-like field of \p R, or null if there is none
// or more than one. A pointer-like type is as isPointerLikeType says, minus
// function pointers; a raw pointer must point to a self-contained type.
const FieldDecl *getSinglePublicPointerField(const CXXRecordDecl *R) {
  const FieldDecl *Found = nullptr;
  for (const FieldDecl *F : R->fields()) {
    QualType T = F->getType();
    if (F->getAccess() != AS_public || !isPointerLikeType(T) ||
        T->isFunctionPointerType() ||
        (T->isPointerType() && !isSelfContained(T->getPointeeType())))
      continue;
    if (Found)
      return nullptr;
    Found = F;
  }
  return Found;
}

// Records whether \p R is a direct view or, if known, why it is not. Unions
// are skipped.
// A direct view gets an implicit [[gsl::Pointer]].
// \return true if \p R is a direct view.
bool checkRecord(const CXXRecordDecl *R, const LifetimeInference &LI,
                 Reports &Out) {
  if (!R->hasDefinition() || !R->isCompleteDefinition() || R->isUnion())
    return false;
  FieldT P; // The capturer all member functions agree on.
  // A direct view base is the borrow capturer. Bases are checked first, so an
  // inferred one is marked.
  for (const CXXBaseSpecifier &B : R->bases())
    if (isGslPointerType(B.getType())) {
      P = &B;
      break;
    }
  for (const CXXMethodDecl *M : R->methods())
    if (!checkMethod(R, M, LI, P, Out))
      return false;
  // Without a capturing member function, the public pointer field is P.
  if (P.isNull())
    if (const FieldDecl *F = getSinglePublicPointerField(R))
      P = F;
  if (P.isNull()) {
    Out.whyNotDirectView(R, NotDirectViewReason::NoBorrowNoPublicPointer, R, P);
    return false;
  }
  if (!checkNoBorrowInvalidation(R, LI, P, Out))
    return false;
  Out.directView(R);
  addImplicitGslPointerAttr(R);
  return true;
}

// Calls \p Use on each record that type \p T mentions: through pointers,
// references, arrays, and the type arguments of template specializations.
void forEachRecordIn(
    QualType T, llvm::function_ref<void(const CXXRecordDecl *)> CollectFn) {
  while (T->isPointerType() || T->isReferenceType())
    T = T->getPointeeType();
  if (const ArrayType *AT = T->getAsArrayTypeUnsafe())
    return forEachRecordIn(AT->getElementType(), CollectFn);
  const CXXRecordDecl *RD = T->getAsCXXRecordDecl();
  if (!RD)
    return;
  CollectFn(RD);
  if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(RD))
    for (const TemplateArgument &A : Spec->getTemplateArgs().asArray()) {
      if (A.getKind() == TemplateArgument::Type)
        forEachRecordIn(A.getAsType(), CollectFn);
      else if (A.getKind() == TemplateArgument::Pack)
        for (const TemplateArgument &E : A.pack_elements())
          if (E.getKind() == TemplateArgument::Type)
            forEachRecordIn(E.getAsType(), CollectFn);
    }
}

// Orders \p Records, dependencies first. A record depends on the records that
// its declaration uses: base classes, fields, and the parameter and return
// types of its member functions.
FixPoint<const CXXRecordDecl *>
buildRecordDependencyGraph(llvm::ArrayRef<const CXXRecordDecl *> Records) {
  llvm::DenseSet<const CXXRecordDecl *> InSet(Records.begin(), Records.end());
  llvm::DenseMap<const CXXRecordDecl *,
                 llvm::SmallVector<const CXXRecordDecl *, 2>>
      Dependents;

  for (const CXXRecordDecl *R : Records) {
    llvm::SmallPtrSet<const CXXRecordDecl *, 8> Seen;
    auto Collect = [&](const CXXRecordDecl *V) {
      V = V->getDefinition();
      if (V && V != R && InSet.contains(V) && Seen.insert(V).second)
        Dependents[V].push_back(R);
    };
    for (const CXXBaseSpecifier &B : R->bases())
      forEachRecordIn(B.getType(), Collect);
    for (const FieldDecl *F : R->fields())
      forEachRecordIn(F->getType(), Collect);
    for (const CXXMethodDecl *M : R->methods()) {
      if (M->isImplicit())
        continue;
      forEachRecordIn(M->getReturnType(), Collect);
      for (const ParmVarDecl *PVD : M->parameters())
        forEachRecordIn(PVD->getType(), Collect);
    }
  }
  return FixPoint<const CXXRecordDecl *>(Records, std::move(Dependents));
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

  // TODO: the status of a record is not monotonic yet, so no fix-point: each
  // record is checked once, dependencies first.
  Reports Out;
  // A record with an undefined member function cannot be judged. Implicit,
  // defaulted, deleted and pure virtual ones have no body to wait for.
  llvm::SmallVector<const CXXRecordDecl *> Analyzable;
  llvm::copy_if(Records, std::back_inserter(Analyzable),
                [](const CXXRecordDecl *R) {
                  return llvm::all_of(R->methods(), [](const CXXMethodDecl *M) {
                    return M->isImplicit() || M->isDefaulted() ||
                           M->isDeleted() || M->isPureVirtual() ||
                           M->isDefined();
                  });
                });
  auto Graph = buildRecordDependencyGraph(Analyzable);
  for (const CXXRecordDecl *R : Graph.getOrder())
    checkRecord(R, LI, Out);
  Out.flush(Diags);
}

} // namespace clang::lifetimes
