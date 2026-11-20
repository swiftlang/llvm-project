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
//  - Self-contained types are only the arithmetic, the explicitly
//    swift_attr("Escapable") and the gsl::Owner ones.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/DirectViewInference.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeAnnotations.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSafety.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeSummary.h"
#include "clang/Analysis/CallGraphFixPoint.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticSema.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <optional>
#include <variant>

namespace clang::lifetimes {
namespace {

// Direct view inference is based on the generic rules below. A type that
// violates any rule is not a direct view. The rules are sound: when it is
// unknown whether an object or a type has a property, the rule assumes that it
// does not.
//
// Vocabulary:
//   RefOrPointerLike type:  a pointer, a reference, or a gsl::Pointer type.
//   Self-contained type:    an arithmetic type, a Swift "Escapable" type, a
//                           gsl::Owner type, or an array of such.
//   Direct pointer:         a pointer or reference to a self-contained type,
//                           or a gsl::Pointer type.
//
// Rules:
// 1. One borrow field. Exactly one of the following holds:
//    a) The member functions, except the destructor, capture direct pointer
//       parameters in exactly one field, P.
//    b) The type has a single public non-const direct pointer field, P.
// 2. No sub-borrow: no member function captures a parameter that is not a
//    direct pointer.
// 3. No stealing: the borrow field P is RefOrPointerLike. For example,
//    capturing a direct pointer parameter in a gsl::Owner field borrows and
//    then owns.
// 4. No invalidation of the borrow field: no member function invalidates P.
// 5. A wrapper of a direct view is a direct view: P can be a base class that is
//    a direct view, provided that no rule above is violated.

// Immutable pair: the field and where it is established
struct EstablishedBorrowField {
  FieldT const F = nullptr;
  // Where F is established: the member function capturing in it, the field
  // itself for a public direct pointer field, or the record for a base.
  const NamedDecl *const At = nullptr;
};

// The reasons why a record is not a direct view, each with the arguments that
// its note needs.
namespace reason {
struct NoUniqueBorrowField {};

struct MethodInconsistentBorrow {
  EstablishedBorrowField EstablishedBF;
  FieldT AnotherBF;
};

struct MethodCapturesNonSelfContained {};

struct BorrowFieldMaySteal {
  FieldT P;
};

struct MethodInvalidatesBorrow {
  FieldT P;
};
} // namespace reason

// The index of a reason selects its text in
// note_interop_not_direct_view_reason, so the order must match the %select
// there.
using NotDirectViewReason =
    std::variant<reason::NoUniqueBorrowField, reason::MethodInconsistentBorrow,
                 reason::MethodCapturesNonSelfContained,
                 reason::BorrowFieldMaySteal, reason::MethodInvalidatesBorrow>;
static_assert(std::variant_size_v<NotDirectViewReason> == 5,
              "update the %select in note_interop_not_direct_view_reason");

// \p RD is explicitly marked self-contained: swift_attr("Escapable"), or
// [[gsl::Owner]].
bool hasSelfContainedAttr(const CXXRecordDecl *RD) {
  return isGslOwnerType(RD) ||
         llvm::any_of(RD->specific_attrs<SwiftAttrAttr>(),
                      [](const SwiftAttrAttr *A) {
                        return A->getAttribute() == "Escapable";
                      });
}

// \p P is a field of reference type.
bool isReferenceTypedField(const FieldT &P) {
  if (const auto *FD = dyn_cast_if_present<const FieldDecl *>(P))
    return FD->getType()->isReferenceType();
  return false;
}

// Streams the field or the base class \p F to \p DB.
void streamField(const StreamingDiagnostic &DB, FieldT F) {
  assert(F);
  if (const auto *D = dyn_cast<const FieldDecl *>(F))
    DB << D;
  else
    DB << cast<const CXXBaseSpecifier *>(F)->getType();
}

/// The results of the analysis. They are emitted together by flush(), after all
/// records are analyzed.
class Reports {
public:
  /// \p RD is a direct view type.
  void isDirectView(const CXXRecordDecl *RD) { set(RD, ReportItem{}); }

  /// Explains why \p RD is not a direct view type: \p At is the declaration at
  /// fault.
  void whyNotDirectView(const CXXRecordDecl *RD, const NamedDecl *At,
                        NotDirectViewReason Why) {
    assert(RD && At);
    set(RD, ReportItem{At, std::move(Why)});
  }

  void flush(DiagnosticsEngine &Diags) const {
    for (const auto &[RD, I] : Items) {
      if (!I.At) {
        if (hasSelfContainedAttr(RD))
          Diags.Report(RD->getLocation(),
                       diag::warn_interop_self_contained_direct_view)
              << RD;
        Diags.Report(RD->getLocation(), diag::remark_interop_direct_view) << RD;
        continue;
      }
      if (RD->hasAttr<PointerAttr>())
        Diags.Report(RD->getLocation(),
                     diag::warn_interop_gsl_pointer_not_direct_view)
            << RD;
      Diags.Report(RD->getLocation(), diag::remark_interop_not_direct_view)
          << RD;

      // A structured binding cannot be captured before C++20.
      const NamedDecl *At = I.At;
      auto DB = Diags.Report(At->getLocation(),
                             diag::note_interop_not_direct_view_reason);

      DB << static_cast<unsigned>(I.Why.index());
      // Streams the arguments of each reason, in the order of its %select text.
      // Every reason has a handler, so a new one must decide its arguments.
      std::visit(llvm::makeVisitor(
                     [](const reason::NoUniqueBorrowField &) {},
                     [&](const reason::MethodInconsistentBorrow &R) {
                       streamField(DB, R.EstablishedBF.F);
                       DB << R.EstablishedBF.At;
                       streamField(DB, R.AnotherBF);
                       DB << isa<const CXXBaseSpecifier *>(R.EstablishedBF.F)
                          << At;
                     },
                     [](const reason::MethodCapturesNonSelfContained &) {},
                     [&](const reason::BorrowFieldMaySteal &R) {
                       streamField(DB, R.P);
                     },
                     [&](const reason::MethodInvalidatesBorrow &R) {
                       streamField(DB, R.P);
                     }),
                 I.Why);
    }
  }

private:
  struct ReportItem {
    /// The declaration at fault, or null if the record is a direct view.
    const NamedDecl *At = nullptr;
    /// Why the record is not a direct view. Only meaningful if \p At is set.
    NotDirectViewReason Why;
  };

  // A reason may be immutable, so is ReportItem: replace instead of assigning.
  void set(const CXXRecordDecl *RD, ReportItem Item) {
    Items.erase(RD);
    Items.try_emplace(RD, std::move(Item));
  }

  llvm::DenseMap<const CXXRecordDecl *, ReportItem> Items;
};

/// \return the member functions of \p R: its methods, and the specializations
/// of its member function templates in this TU.
llvm::SmallVector<const CXXMethodDecl *>
memberFunctions(const CXXRecordDecl *R) {
  llvm::SmallVector<const CXXMethodDecl *> Result(R->methods());
  for (const Decl *D : R->decls())
    if (const auto *FTD = dyn_cast<FunctionTemplateDecl>(D))
      for (const FunctionDecl *Spec : FTD->specializations())
        if (const auto *MD = dyn_cast<CXXMethodDecl>(Spec))
          Result.push_back(MD);
  return Result;
}

/// \p T is arithmetic, swift_attr("Escapable"), gsl::Owner, or an array of
/// such.
bool isSelfContainedType(QualType T) {
  if (T.isNull())
    return false;
  return true;
  if (const ArrayType *AT = T->getAsArrayTypeUnsafe())
    return isSelfContainedType(AT->getElementType());
  if (T->isArithmeticType())
    return true;
  const CXXRecordDecl *RD = T->getAsCXXRecordDecl();
  RD = RD ? RD->getDefinition() : nullptr;
  return RD && hasSelfContainedAttr(RD);
}

bool isDirectPointerType(const QualType T) {
  if (T.isNull())
    return false;
  if (isGslPointerType(T))
    return true;
  return isReferenceOrPointerLikeType(T) &&
         isSelfContainedType(T->getPointeeType());
}

/// \return the fields that parameter \p ParamIdx of \p M is captured in.
llvm::SetVector<FieldT> getCapturingFields(const FunctionSummary &Sum,
                                           const CXXMethodDecl *M,
                                           unsigned ParamIdx) {
  ParamOrThis P = ParamOrThis::forParam(ParamIdx);
  llvm::SetVector<FieldT> Results;

  if (isa<CXXConstructorDecl>(M)) {
    for (const LifetimeBoundAssertion &A : Sum.getLifetimeBoundFor(P))
      if (!A.F.isAll())
        Results.insert_range(A.F.fields());
  }
  for (const CapturedByAssertion &A : Sum.getCapturedByFor(P))
    if (A.Q.isThis() && !A.F.isAll())
      Results.insert_range(A.F.fields());
  return Results;
}

/// A member function with its summary, and the parameters that it captures in
/// a field.
struct MethodInfo {
  const CXXRecordDecl *Record;
  const CXXMethodDecl *Method;
  const FunctionDecl *Def;
  const FunctionSummary &Sum;
  llvm::SmallVector<unsigned, 1> CapturedParams;

  /// \return a finite set of borrowing fields
  llvm::SetVector<FieldT> borrowFieldsOf(unsigned Idx) const {
    return getCapturingFields(Sum, Method, Idx);
  }
};

/// Checks that a member function captures parameters in one field only. If the
/// check passes and \p P is unset, the function establishes \p P.
/// \return true iff the check passes.
bool checkCapturesInOneField(const MethodInfo &I,
                             std::optional<EstablishedBorrowField> &P,
                             Reports &Out) {
  // P, if set, goes first, so a conflicting field is reported against it.
  llvm::SetVector<FieldT> BFields;

  if (P)
    BFields.insert(P->F);
  for (unsigned CapturedParmIdx : I.CapturedParams)
    BFields.insert_range(I.borrowFieldsOf(CapturedParmIdx));
  if (BFields.size() > 1) {
    // Without a prior P, this function captures in both fields.
    EstablishedBorrowField First =
        P ? *P : EstablishedBorrowField{BFields[0], I.Def};
    Out.whyNotDirectView(I.Record, I.Def,
                         reason::MethodInconsistentBorrow{First, BFields[1]});
    return false;
  }
  if (!P && !BFields.empty())
    P.emplace(EstablishedBorrowField{BFields.front(), I.Def});
  return true;
}

/// Check that a member function does not sub-borrow, i.e., capturing a
/// parameter that is not a direct pointer.
/// \return true if check passes
bool checkNoSubBorrowCapture(const MethodInfo &I, Reports &Out) {
  auto *SubBorrowParm = llvm::find_if(I.CapturedParams, [&I](unsigned Idx) {
    return !isDirectPointerType(I.Def->getParamDecl(Idx)->getType());
  });

  if (SubBorrowParm != I.CapturedParams.end()) {
    Out.whyNotDirectView(I.Record, I.Def->getParamDecl(*SubBorrowParm),
                         reason::MethodCapturesNonSelfContained{});
    return false;
  }
  return true;
}

/// Checks member function \p M while ignoring destructor or copy/move
/// constructors/assignment.
/// \param P is the sole borrow field, or unset if not found yet. It is set by
/// this function if the check passes.
/// \return true if the check passes.
bool checkMethod(const CXXRecordDecl *R, const CXXMethodDecl *M,
                 const LifetimeInference &LI,
                 std::optional<EstablishedBorrowField> &P, Reports &Out) {
  if (isa<CXXDestructorDecl>(M) || M->isImplicit() || M->isDeleted() ||
      M->isCopyAssignmentOperator() || M->isMoveAssignmentOperator() ||
      M->getNumParams() == 0)
    return true;
  if (auto *Ctor = dyn_cast<CXXConstructorDecl>(M);
      Ctor && Ctor->isCopyOrMoveConstructor())
    return true;

  const FunctionDecl *Def = M->getDefinition();
  const FunctionSummary *Sum = Def ? LI.getSummary(Def) : nullptr;

  if (!Sum)
    return true;

  MethodInfo I{R, M, Def, *Sum, {}};

  for (unsigned Idx = 0; Idx < Def->getNumParams(); ++Idx)
    if (!getCapturingFields(*Sum, M, Idx).empty())
      I.CapturedParams.push_back(Idx);
  if (I.CapturedParams.empty())
    return true;
  // Report sub-borrow capture if any first:
  if (!checkNoSubBorrowCapture(I, Out))
    return false;
  return checkCapturesInOneField(I, P, Out);
}

// \p F is \p P, or \p F is a subobject of \p P.
bool isPartOf(FieldT F, FieldT P) {
  if (F == P)
    return true;
  const auto *B = dyn_cast_if_present<const CXXBaseSpecifier *>(P);
  const auto *FD = dyn_cast_if_present<const FieldDecl *>(F);
  if (!B || !FD)
    return false;
  const CXXRecordDecl *BaseRD = B->getType()->getAsCXXRecordDecl();
  const auto *Parent = dyn_cast<CXXRecordDecl>(FD->getParent());
  BaseRD = BaseRD ? BaseRD->getDefinition() : nullptr;
  return BaseRD && Parent &&
         (BaseRD->getCanonicalDecl() == Parent->getCanonicalDecl() ||
          BaseRD->isDerivedFrom(Parent));
}

/// Checks that the borrow field \p P, which must be set, is RefOrPointerLike.
/// \return true if the check passes
bool checkCapturesInPointerLikeField(const CXXRecordDecl *RD, const FieldT &P,
                                     Reports &Out) {
  assert(P);
  const auto *FD = dyn_cast<const FieldDecl *>(P);
  if (!FD || isReferenceOrPointerLikeType(FD->getType()))
    return true;
  // Base cannot capture if it is not a gsl::Pointer, so no need to check.
  Out.whyNotDirectView(RD, FD, reason::BorrowFieldMaySteal{P});
  return false;
}

/// Checks that no member function invalidates the borrow field \p P, which
/// must be set.
/// \return true if the check passes
bool checkNoBorrowInvalidation(const CXXRecordDecl *R,
                               const LifetimeInference &LI, const FieldT &P,
                               Reports &Out) {
  for (const CXXMethodDecl *M : memberFunctions(R)) {
    if (M->isStatic() || M->isImplicit() || M->isDeleted() ||
        (M->isTemplateInstantiation() && !M->isDefined()))
      continue;

    const FunctionDecl *Def = M->getDefinition();
    const FunctionSummary *Sum = Def ? LI.getSummary(Def) : nullptr;

    if (!Sum)
      continue;

    const FieldSet *Invalidated =
        Sum->getInvalidatedFor(ParamOrThis::forThis());

    if (!Invalidated)
      continue;

    auto IsPartOfP = [&P](FieldT F) { return isPartOf(F, P); };

    if (Invalidated->isAll() ||
        llvm::any_of(Invalidated->fields(), IsPartOfP)) {
      Out.whyNotDirectView(R, M, reason::MethodInvalidatesBorrow{P});
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

/// \return true iff there is exactly one public non-const direct pointer field
/// of \p R. Populates \p P with the found one.
bool getUniquePublicPointerField(const CXXRecordDecl *R,
                                 std::optional<EstablishedBorrowField> &P) {
  for (const FieldDecl *F : R->fields()) {
    QualType T = F->getType();

    if (F->getAccess() != AS_public || !isDirectPointerType(T) ||
        T.isConstQualified())
      continue;
    if (P)
      return false;
    P.emplace(EstablishedBorrowField{F, F});
  }
  return bool(P);
}

// Records whether \p R is a direct view or, if known, why it is not. Unions
// are skipped.
// A direct view gets an implicit [[gsl::Pointer]].
// \return true if \p R is a direct view.
bool checkRecord(const CXXRecordDecl *R, const LifetimeInference &LI,
                 Reports &Out) {
  if (!R->hasDefinition() || !R->isCompleteDefinition() || R->isUnion())
    return false;

  std::optional<EstablishedBorrowField> P; // The sole borrow field.

  // Rule 5: a direct view base is the borrow field.
  auto *Base = llvm::find_if(R->bases(), [](const CXXBaseSpecifier &B) {
    return isGslPointerType(B.getType());
  });

  if (Base != R->bases().end())
    P.emplace(EstablishedBorrowField{Base, R});
  for (const CXXMethodDecl *M : memberFunctions(R))
    if (!checkMethod(R, M, LI, P, Out))
      return false;
  // If no member function captures in a field, the unique public non-const
  // direct pointer field is P:
  if (!P && !getUniquePublicPointerField(R, P)) {
    Out.whyNotDirectView(R, R, reason::NoUniqueBorrowField{});
    return false;
  }
  if (!checkCapturesInPointerLikeField(R, P->F, Out))
    return false;
  // A reference cannot be invalidated.
  if (!isReferenceTypedField(P->F) &&
      !checkNoBorrowInvalidation(R, LI, P->F, Out))
    return false;
  Out.isDirectView(R);
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
  // A record with an undefined member function, including a specialization of a
  // member function template, cannot be judged. Implicit, defaulted, deleted
  // and pure virtual ones have no body to wait for.
  llvm::SmallVector<const CXXRecordDecl *> Analyzable;
  llvm::copy_if(
      Records, std::back_inserter(Analyzable), [](const CXXRecordDecl *R) {
        return llvm::all_of(memberFunctions(R), [](const CXXMethodDecl *M) {
          return M->isImplicit() || M->isDefaulted() || M->isDeleted() ||
                 M->isPureVirtual() || M->isDefined();
        });
      });
  auto Graph = buildRecordDependencyGraph(Analyzable);
  for (const CXXRecordDecl *R : Graph.getOrder())
    checkRecord(R, LI, Out);
  Out.flush(Diags);
}

} // namespace clang::lifetimes
