//===- SelfContainment.cpp - Self-containment inference ---------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the translation-unit-wide inference of whether C++ types
// are self-contained, for Swift's import of C++ types.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/Ownership/SelfContainment.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/DynamicRecursiveASTVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeBase.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/DiagnosticUtils.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <variant>

namespace clang::ownership {

void DestroyedFieldsCollector::onDestructorDestroysField(
    const CXXRecordDecl *Class, const FieldDecl *Field) {
  Map[Class].insert(Field);
}

const llvm::DenseSet<const FieldDecl *> &
DestroyedFieldsCollector::getDestroyedFields(const CXXRecordDecl *Class) const {
  static const llvm::DenseSet<const FieldDecl *> Empty;
  auto It = Map.find(Class);
  return It == Map.end() ? Empty : It->second;
}

namespace {
enum class Result {
  TriviallySelfContained,
  SelfContained,
  Unknown,
  NotSelfContained,
};
[[nodiscard]]
Result latticeJoinResult(Result L, Result R) {
  return std::max(L, R);
}
[[nodiscard]] StringRef asString(Result R) {
  switch (R) {
  case Result::TriviallySelfContained:
    return "trivially-self-contained";
  case Result::SelfContained:
    return "self-contained";
  case Result::Unknown:
    return "unknown";
  case Result::NotSelfContained:
    return "not-self-contained";
  }
  llvm_unreachable("unknown Result");
}

auto hasSwiftAttrAttr(StringRef Str) {
  return [=](const SwiftAttrAttr *A) { return A->getAttribute() == Str; };
}

bool hasSelfContainedAttr(const RecordDecl *D) {
  return llvm::any_of(D->specific_attrs<SwiftAttrAttr>(),
                      hasSwiftAttrAttr("Escapable"));
}
bool hasViewAttr(const RecordDecl *D) {
  return llvm::any_of(D->specific_attrs<SwiftAttrAttr>(),
                      hasSwiftAttrAttr("~Escapable"));
}

// `SELF_CONTAINED_IF(T1, T2, ...)` / `SWIFT_ESCAPABLE_IF(...)` expand to
// `swift_attr("escapable_if:T1, T2, ...")`. Returns the named parameters as
// written, comma-split and trimmed; empty when \p D makes no conditional claim.
// Matching the names to the template's parameters is the caller's job.
llvm::SmallVector<StringRef> conditionalSelfContainedParams(const RecordDecl *D) {
  llvm::SmallVector<StringRef> Names;
  for (const auto *A : D->specific_attrs<SwiftAttrAttr>()) {
    StringRef Text = A->getAttribute();
    if (!Text.consume_front("escapable_if:"))
      continue;
    llvm::SmallVector<StringRef> Parts;
    Text.split(Parts, ',');
    for (StringRef Part : Parts)
      if (StringRef Name = Part.trim(); !Name.empty())
        Names.push_back(Name);
  }
  return Names;
}

// The type arguments a conditional self-containedness claim resolves to, each
// paired with the parameter that named it. Only meaningful for a specialization,
// whose substituted arguments the claim refers to by the template's parameter
// names. Names matching no type parameter, or one bound to a non-type argument,
// are dropped -- a well-formed claim is assumed here; a malformed one is a hard
// error reported at the written definition by ConditionalClaimValidator. An
// empty result (not a specialization, or nothing resolved) is the signal to
// fall back to the ordinary base/field walk.
static llvm::SmallVector<std::pair<const TemplateTypeParmDecl *, QualType>, 2>
resolveConditionArgs(const RecordDecl *RD, llvm::ArrayRef<StringRef> Names) {
  llvm::SmallVector<std::pair<const TemplateTypeParmDecl *, QualType>, 2> Args;
  const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(RD);
  if (!Spec)
    return Args;
  const TemplateParameterList *Params =
      Spec->getSpecializedTemplate()->getTemplateParameters();
  const TemplateArgumentList &TArgs = Spec->getTemplateArgs();
  for (StringRef Name : Names)
    for (unsigned I = 0, N = Params->size(); I != N; ++I) {
      const auto *P = dyn_cast<TemplateTypeParmDecl>(Params->getParam(I));
      if (!P || !P->getIdentifier() || P->getName() != Name)
        continue;
      if (I < TArgs.size() && TArgs[I].getKind() == TemplateArgument::Type)
        Args.push_back({P, TArgs[I].getAsType()});
      break;
    }
  return Args;
}

// Lambda capture FieldDecls are unnamed; resolve them to the capture's name
// ('this' for a this-capture, the captured variable otherwise) so notes don't
// print an empty ''. Normal fields fall back to their own name.
std::string fieldDisplayName(const FieldDecl *F) {
  const auto *RD = dyn_cast_or_null<CXXRecordDecl>(F->getParent());
  if (RD && RD->isLambda()) {
    // captures() and fields() are in 1:1 lockstep (see getCaptureFields).
    for (const auto &[Field, Cap] :
         llvm::zip_equal(RD->fields(), RD->captures())) {
      if (Field != F)
        continue;
      if (Cap.capturesThis())
        return "this";
      if (Cap.capturesVariable())
        return Cap.getCapturedVar()->getName().str();
      break; // VLA-type capture: no name, fall through
    }
  }
  return F->getName().str();
}

struct ReferenceField {
  const FieldDecl *Field;
  void report(DiagnosticsEngine &Diags) const;
};

struct UnmanagedRawPtrField {
  const FieldDecl *Field;
  void report(DiagnosticsEngine &Diags) const;
};

struct BadBase {
  CXXBaseSpecifier Base;
  Result MemberResult;

  LLVM_ATTRIBUTE_RETURNS_NONNULL const CXXRecordDecl *asDecl() const {
    return Base.getType()->getAsCXXRecordDecl();
  }

  bool trustedViewAttribute() const { return hasViewAttr(asDecl()); }

  void report(DiagnosticsEngine &Diags) const;
};

struct BadField {
  const FieldDecl *Field;
  // The record actually checked: Field's type after unwrapType() (array /
  // complex / vector / atomic element), which is what the verdict applies to.
  const RecordDecl *CheckedRecord;
  Result MemberResult;

  bool trustedViewAttribute() const { return hasViewAttr(CheckedRecord); }

  void report(DiagnosticsEngine &Diags) const;
};

struct ManagedRawPtrField {
  const FieldDecl *Field;
  void report(DiagnosticsEngine &Diags) const;
};

// A managed raw-pointer field whose pointee constrains the owner: whoever owns
// a view (or something unclassifiable) can only hand out that view. Like
// BadField, recorded only for a constraining pointee (>= Unknown).
struct BadPointee {
  const FieldDecl *Field;
  const RecordDecl *CheckedRecord;
  Result MemberResult;

  bool trustedViewAttribute() const { return hasViewAttr(CheckedRecord); }

  void report(DiagnosticsEngine &Diags) const;
};

struct OpaqueUserDeclaredDestructor {
  const CXXDestructorDecl *Dtor;
  void report(DiagnosticsEngine &Diags) const;
};

struct NotInstantiatedDestructor {
  const CXXDestructorDecl *Dtor;
  void report(DiagnosticsEngine &Diags) const;
};

// A SELF_CONTAINED_IF claim decomposed the record into this template argument
// (named by \p Param, bound to \p Arg), which the analysis classified as \p
// MemberResult. Recorded only for a constraining argument (>= Unknown), like
// BadField/BadBase.
struct TemplateTypeArg {
  const TemplateTypeParmDecl *Param;
  QualType Arg;
  Result MemberResult;

  bool trustedViewAttribute() const {
    const RecordDecl *Rec = Arg->getAsRecordDecl();
    return Rec && hasViewAttr(Rec);
  }

  void report(DiagnosticsEngine &Diags) const;
};

void ReferenceField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field)
      << /*reference=*/0 << fieldDisplayName(Field) << SourceRange(Loc);
}

void UnmanagedRawPtrField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field)
      << /*unmanaged raw-pointer=*/1 << fieldDisplayName(Field)
      << SourceRange(Loc);
}

void BadBase::report(DiagnosticsEngine &Diags) const {
  auto ResultSelect = [](Result R) {
    return R == Result::NotSelfContained ? 0 : 1;
  };
  // Point at the base-specifier (where the inheritance is expressed),
  // not the base class's own definition.
  SourceLocation Loc = Base.getBaseTypeLoc();
  Diags.Report(Loc, diag::note_ownership_inference_derives_from_base_inferred)
      << asDecl() << trustedViewAttribute() << ResultSelect(MemberResult)
      << SourceRange(Loc);
}

void BadField::report(DiagnosticsEngine &Diags) const {
  auto ResultSelect = [](Result R) {
    return R == Result::NotSelfContained ? 0 : 1;
  };
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field_inferred)
      << fieldDisplayName(Field) << CheckedRecord << trustedViewAttribute()
      << ResultSelect(MemberResult) << SourceRange(Loc);
}

void ManagedRawPtrField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field)
      << /*managed raw-pointer=*/2 << fieldDisplayName(Field)
      << SourceRange(Loc);
}

void BadPointee::report(DiagnosticsEngine &Diags) const {
  auto ResultSelect = [](Result R) {
    return R == Result::NotSelfContained ? 0 : 1;
  };
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_owns_pointee_inferred)
      << fieldDisplayName(Field) << CheckedRecord << trustedViewAttribute()
      << ResultSelect(MemberResult) << SourceRange(Loc);
}

void OpaqueUserDeclaredDestructor::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Dtor->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_destructor)
      << /*opaque=*/0 << SourceRange(Loc);
}

void NotInstantiatedDestructor::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Dtor->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_destructor)
      << /*not instantiated=*/1 << SourceRange(Loc);
}

void TemplateTypeArg::report(DiagnosticsEngine &Diags) const {
  auto ResultSelect = [](Result R) {
    return R == Result::NotSelfContained ? 0 : 1;
  };
  SourceLocation Loc = Param->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_decomposed_template_argument)
      << Param << Arg << trustedViewAttribute() << ResultSelect(MemberResult)
      << SourceRange(Loc);
}

class Reason {
public:
  /*implicit*/ Reason(ReferenceField R) : Data{std::move(R)} {}
  /*implicit*/ Reason(UnmanagedRawPtrField R) : Data{std::move(R)} {}
  /*implicit*/ Reason(BadBase R) : Data{std::move(R)} {}
  /*implicit*/ Reason(BadField R) : Data{std::move(R)} {}
  /*implicit*/ Reason(OpaqueUserDeclaredDestructor R) : Data{std::move(R)} {}
  /*implicit*/ Reason(NotInstantiatedDestructor R) : Data{std::move(R)} {}
  /*implicit*/ Reason(ManagedRawPtrField R) : Data{std::move(R)} {}
  /*implicit*/ Reason(BadPointee R) : Data{std::move(R)} {}
  /*implicit*/ Reason(TemplateTypeArg R) : Data{std::move(R)} {}

  void emitNotes(DiagnosticsEngine &Diags) const {
    std::visit([&](const auto &Item) { Item.report(Diags); }, Data);
  }

private:
  std::variant<ReferenceField, UnmanagedRawPtrField, BadBase, BadField,
               OpaqueUserDeclaredDestructor, NotInstantiatedDestructor,
               ManagedRawPtrField, BadPointee, TemplateTypeArg>
      Data;
};

void emitNotes(DiagnosticsEngine &Diags, llvm::ArrayRef<Reason> Reasons) {
  for (const Reason &R : Reasons) {
    R.emitNotes(Diags);
  }
}

struct Explanation {
  Result Outcome;
  llvm::SmallVector<Reason> Reasons;
};

static const Type *unwrapTypeOnce(const Type *T) {
  if (const auto *AT = T->getAsCanonical<ArrayType>())
    return AT->getElementType().getTypePtr();
  if (const auto *CT = T->getAsCanonical<ComplexType>())
    return CT->getElementType().getTypePtr();
  if (const auto *VT = T->getAsCanonical<VectorType>())
    return VT->getElementType().getTypePtr();
  if (const auto *AT = T->getAsCanonical<AtomicType>())
    return AT->getValueType().getTypePtr();
  return T;
}
const Type *unwrapType(QualType Ty) {
  CanQualType CanonicalTy = Ty->getCanonicalTypeUnqualified();
  const Type *T = CanonicalTy->getTypePtr();
  while (true) {
    const Type *Curr = unwrapTypeOnce(T);
    if (Curr == T)
      return T;
    T = Curr;
  }
}

// Reports the hard errors for a malformed SELF_CONTAINED_IF claim. Runs with
// template-instantiation visiting OFF so it sees only written declarations and
// each error fires once at the definition rather than per instantiation.
class ConditionalClaimValidator final : public DynamicRecursiveASTVisitor {
public:
  DiagnosticsEngine &Diags;

  explicit ConditionalClaimValidator(DiagnosticsEngine &Diags) : Diags(Diags) {
    ShouldVisitTemplateInstantiations = false;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *R) final {
    if (!R->isThisDeclarationADefinition())
      return true;

    // A full/explicit specialization or (extern) template instantiation only
    // inherits the attribute from its primary template; the claim is "declared"
    // on the primary template (or a partial specialization), which is validated
    // on its own. Skip the rest so we don't re-report inherited claims -- e.g.
    // `extern template class mpark::variant<...>` of an annotated template.
    if (isa<ClassTemplateSpecializationDecl>(R) &&
        !isa<ClassTemplatePartialSpecializationDecl>(R))
      return true;

    llvm::SmallVector<StringRef> Names = conditionalSelfContainedParams(R);
    if (Names.empty())
      return true;

    // The names refer to template parameters, so the claim needs a template's
    // parameter list: the described template for a primary template, or the
    // partial specialization itself (getDescribedClassTemplate() is null for a
    // partial specialization, which is still a template with its own params).
    const TemplateParameterList *Params = nullptr;
    if (const ClassTemplateDecl *CTD = R->getDescribedClassTemplate())
      Params = CTD->getTemplateParameters();
    else if (const auto *PS = dyn_cast<ClassTemplatePartialSpecializationDecl>(R))
      Params = PS->getTemplateParameters();

    if (!Params) {
      Diags.Report(R->getLocation(),
                   diag::err_ownership_inference_escapable_if_not_template);
      return true;
    }

    for (StringRef Name : Names) {
      bool Found = llvm::any_of(*Params, [&](const NamedDecl *P) {
        return isa<TemplateTypeParmDecl>(P) && P->getIdentifier() &&
               P->getName() == Name;
      });
      if (!Found)
        Diags.Report(R->getLocation(),
                     diag::err_ownership_inference_escapable_if_unknown_param)
            << Name;
    }
    return true;
  }
};

class MyChecker final : public DynamicRecursiveASTVisitor {
public:
  DiagnosticsEngine &Diags;
  const DestroyedFieldsCollector &Collector;

  MyChecker(DiagnosticsEngine &Diags, const DestroyedFieldsCollector &Collector)
      : Diags(Diags), Collector(Collector) {
    ShouldVisitTemplateInstantiations = true;
    ShouldWalkTypesOfTypeLocs = false;
    ShouldVisitImplicitCode = false;
    ShouldVisitLambdaBody = true;
  }

  bool VisitRecordDecl(RecordDecl *R) final {
    if (R->isDependentContext())
      return true;

    if (!R->isThisDeclarationADefinition())
      return true;

    Explanation InferenceResult = inferDefinitionOfClass(R);

    if (auto TrustedResult = hasSomeAttribute(R)) {
      if (InferenceResult.Outcome != Result::Unknown &&
          TrustedResult.value() != InferenceResult.Outcome) {
        SourceLocation Loc = R->getLocation();
        if (IsDiagEnabledExcludingWeverything(
                Diags, diag::warn_ownership_inference_annotation_mismatch,
                Loc)) {
          Diags.Report(Loc, diag::warn_ownership_inference_annotation_mismatch)
              << R << SourceRange(Loc) << asString(TrustedResult.value())
              << asString(InferenceResult.Outcome);
          emitNotes(Diags, InferenceResult.Reasons);
        }
      }
    }

    return true;
  }

private:
  static std::optional<Result> hasSomeAttribute(const RecordDecl *R) {
    if (hasSelfContainedAttr(R))
      return Result::SelfContained;
    if (hasViewAttr(R))
      return Result::NotSelfContained;
    return std::nullopt;
  }

  Result useOfClass(const RecordDecl *R) {
    if (auto TrustedResult = hasSomeAttribute(R))
      return TrustedResult.value();
    // Only a pointee can be incomplete; without a definition the annotation
    // above is all there is to go on.
    if (!R->getDefinition())
      return Result::Unknown;
    return inferDefinitionOfClass(R).Outcome;
  }

  // Classify a SELF_CONTAINED_IF template argument by the same rules the field
  // walk applies, minus the destructor/managed path (a bare argument has no
  // field destructor to manage it): a reference or raw-pointer argument is a
  // view (a container of borrowed pointers), a record argument recurses, and a
  // scalar is trivially self-contained.
  Result classifyConditionArg(QualType QT) {
    const Type *Ty = unwrapType(QT);
    if (Ty->isReferenceType())
      return Result::NotSelfContained;
    if (Ty->isPointerType() && !Ty->isFunctionPointerType() &&
        !Ty->isMemberPointerType())
      return Result::NotSelfContained;
    if (const auto *Rec = Ty->getAsRecordDecl())
      return useOfClass(Rec);
    return Result::TriviallySelfContained;
  }

  // The notes are only meaningful attached to their warning, so skip both.
  // Don't enable these via -Weverything, like the other lifetime-safety checks.
  void reportIfEnabled(const RecordDecl *R, unsigned DiagID,
                       const llvm::SmallVector<Reason> &Reasons) {
    SourceLocation Loc = R->getLocation();
    if (!IsDiagEnabledExcludingWeverything(Diags, DiagID, Loc))
      return;
    Diags.Report(Loc, DiagID) << R << SourceRange(Loc);
    emitNotes(Diags, Reasons);
  }

  // Emit the verdict warning (and its notes), memoize, and return. Shared by
  // the ordinary base/field walk and the SELF_CONTAINED_IF decomposition.
  Explanation reportReason(const RecordDecl *R, Result Res,
                            llvm::SmallVector<Reason> Reasons) {
    unsigned DiagID = 0;
    switch (Res) {
    case Result::TriviallySelfContained:
      DiagID = diag::warn_ownership_inference_suggest_trivially_self_contained;
      break;
    case Result::SelfContained:
      DiagID = diag::warn_ownership_inference_suggest_self_contained;
      break;
    case Result::Unknown:
      DiagID = diag::warn_ownership_inference_failed_to_infer;
      break;
    case Result::NotSelfContained:
      DiagID = diag::warn_ownership_inference_suggest_view;
      break;
    }
    reportIfEnabled(R, DiagID, Reasons);

    auto [Place, Inserted] =
        Cache.try_emplace(R, Explanation{Res, std::move(Reasons)});
    assert(Inserted);
    return Place->second;
  }

  Explanation inferDefinitionOfClass(const RecordDecl *R) {
    if (auto It = Cache.find(R); It != Cache.end())
      return It->second;

    // A SELF_CONTAINED_IF claim can make two templates depend on each other
    // (A's condition argument resolves to B and B's back to A). Since this
    // result is memoized only on completion, such a cycle would otherwise
    // recurse until the stack overflows. Treat a record already being inferred
    // on this stack as 'unknown' to break the cycle; the outermost frame still
    // computes and caches the real verdict.
    if (!InProgress.insert(R).second)
      return Explanation{Result::Unknown, {}};
    llvm::scope_exit PopInProgress([&] { InProgress.erase(R); });

    // SELF_CONTAINED_IF: a template specialization that declares a conditional
    // claim is decomposed into its named template arguments instead of its
    // bases and fields -- the author has taken responsibility for the
    // container's own storage, leaving the element types as the only open
    // question. The verdict is the join of the arguments' verdicts. An
    // unresolvable claim (not a specialization, or names that resolve to no
    // type argument) falls through to the ordinary walk; it is a hard error
    // reported at the written definition by ConditionalClaimValidator.
    if (llvm::SmallVector<StringRef> Names = conditionalSelfContainedParams(R);
        !Names.empty()) {
      if (auto Args = resolveConditionArgs(R, Names); !Args.empty()) {
        Result Res = Result::SelfContained; // a container, not an empty POD
        llvm::SmallVector<Reason> Reasons;
        for (auto [Param, Arg] : Args) {
          Result ArgRes = classifyConditionArg(Arg);
          Res = latticeJoinResult(Res, ArgRes);
          if (ArgRes >= Result::Unknown)
            Reasons.push_back(TemplateTypeArg{Param, Arg, ArgRes});
        }
        return reportReason(R, Res, std::move(Reasons));
      }
    }

    Result Res = Result::TriviallySelfContained;
    llvm::SmallVector<Reason> Reasons;

    // Fold the bases.
    if (const auto *Class = dyn_cast<CXXRecordDecl>(R);
        Class && Class->isCompleteDefinition()) {
      for (const CXXBaseSpecifier &BaseSpec : Class->bases()) {
        CXXRecordDecl *Base = BaseSpec.getType()->getAsCXXRecordDecl();
        Result BaseRes = useOfClass(Base);
        Res = latticeJoinResult(Res, BaseRes);
        if (BaseRes >= Result::Unknown)
          Reasons.push_back(BadBase{BaseSpec, BaseRes});
      }
    }

    // Fold the fields.
    llvm::SmallVector<const FieldDecl *> RawPointerFields;
    for (const auto *F : R->fields()) {
      // Drop from TriviallySelfContained to just SelfContained.
      Res = latticeJoinResult(Res, Result::SelfContained);
      const Type *Ty = unwrapType(F->getType());

      if (Ty->isReferenceType()) {
        Res = Result::NotSelfContained;
        Reasons.push_back(ReferenceField{F});
      }

      if (Ty->isPointerType()) {
        if (!Ty->isFunctionPointerType() && !Ty->isMemberPointerType()) {
          RawPointerFields.push_back(F);
        }
      }

      if (const auto *FieldRec = Ty->getAsRecordDecl()) {
        Result FieldRes = useOfClass(FieldRec);
        Res = latticeJoinResult(Res, FieldRes);
        if (FieldRes >= Result::Unknown)
          Reasons.push_back(BadField{F, FieldRec, FieldRes});
      }
    }

    const auto *Class = dyn_cast<CXXRecordDecl>(R);
    const auto *Dtor = Class ? Class->getDestructor() : nullptr;
    const auto *DtorDef =
        Dtor ? dyn_cast_or_null<CXXDestructorDecl>(Dtor->getDefinition())
             : nullptr;
    const bool IsDefaultedDtor = DtorDef && DtorDef->isDefaulted();

    // Query LifetimeSafetyAnalysis if the given Field was destroyed in the
    // destructor of the class.
    if (!RawPointerFields.empty()) {
      if (!Class || !Class->hasUserDeclaredDestructor() || IsDefaultedDtor) {
        // Implicit or defaulted destructor: the raw pointer fields are
        // unmanaged.
        Res = Result::NotSelfContained;
        for (const FieldDecl *F : RawPointerFields)
          Reasons.push_back(UnmanagedRawPtrField{F});
      } else if (Dtor->getBody()) {
        const auto &Destroyed = Collector.getDestroyedFields(Class);
        auto IsDestroyedInDtor = [&](const FieldDecl *F) {
          return Destroyed.contains(F);
        };
        auto It = llvm::partition(RawPointerFields, IsDestroyedInDtor);
        auto ManagedFields = llvm::make_range(RawPointerFields.begin(), It);
        auto UnmanagedFields = llvm::make_range(It, RawPointerFields.end());

        if (!UnmanagedFields.empty())
          Res = Result::NotSelfContained;

        for (const FieldDecl *F : UnmanagedFields)
          Reasons.push_back(UnmanagedRawPtrField{F});
        for (const FieldDecl *F : ManagedFields) {
          Reasons.push_back(ManagedRawPtrField{F});

          // Owning a view makes the owner a view: what escapes with the owner
          // is only as self-contained as what it owns.
          const auto *Pointee =
              unwrapType(unwrapType(F->getType())->getPointeeType())
                  ->getAsRecordDecl();
          // An owned self-reference (a list node) adds nothing to its own
          // verdict.
          if (!Pointee || Pointee->getCanonicalDecl() == R->getCanonicalDecl())
            continue;
          Result PointeeRes = useOfClass(Pointee);
          Res = latticeJoinResult(Res, PointeeRes);
          if (PointeeRes >= Result::Unknown)
            Reasons.push_back(BadPointee{F, Pointee, PointeeRes});
        }

      } else if (const FunctionDecl *Pattern =
                     Dtor->getTemplateInstantiationPattern();
                 Pattern && Pattern->hasBody()) {
        // Template specialization whose inline destructor was never
        // instantiated (its body was not materialized). Its effect on the
        // fields is unknown, but it is NOT an opaque out-of-line destructor.
        Res = Result::Unknown;
        Reasons.push_back(NotInstantiatedDestructor{Dtor});
      } else {
        // user declared opaque dtor
        Res = Result::Unknown;
        Reasons.push_back(OpaqueUserDeclaredDestructor{Dtor});
        for (const FieldDecl *F : RawPointerFields)
          Reasons.push_back(UnmanagedRawPtrField{F});
      }
    }

    return reportReason(R, Res, std::move(Reasons));
  }

  /// Memoizes self-containment results (and their provenance) for records
  /// visited during this whole-TU traversal. Owned by the checker (reset per
  /// traversal); the destroyed-fields registry it consults is fully populated
  /// before the traversal runs.
  llvm::DenseMap<const RecordDecl *, Explanation> Cache;

  // Records currently being inferred on the call stack, used to break
  // SELF_CONTAINED_IF dependency cycles (see inferDefinitionOfClass).
  llvm::SmallPtrSet<const RecordDecl *, 16> InProgress;
};
} // namespace

void inferClassSelfContainment(ASTContext &Ctx,
                               const DestroyedFieldsCollector &Collector) {
  ConditionalClaimValidator{Ctx.getDiagnostics()}.TraverseAST(Ctx);
  MyChecker C{Ctx.getDiagnostics(), Collector};
  C.TraverseAST(Ctx);
}

} // namespace clang::ownership
