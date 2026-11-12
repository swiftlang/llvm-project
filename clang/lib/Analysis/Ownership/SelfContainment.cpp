//===- SelfContainment.cpp - Self-containment inference -------------------===//
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
#include "clang/AST/DynamicRecursiveASTVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeBase.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/DiagnosticUtils.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include <cassert>
#include <optional>
#include <variant>

using namespace clang;
using namespace ownership;
using llvm::DenseMap;

namespace {
/// The SelfContainment analysis lattice.
enum class Result {
  TriviallySelfContained, ///< Self-contained classes without any fields.
  SelfContained,          ///< No evidence for being not self-contained.
  Unknown,                ///< The analysis was inconclusive.
  View,                   ///< Evidence found for being not self-contained.
};
} // namespace

[[nodiscard]] static Result join(Result L, Result R) { return std::max(L, R); }

[[nodiscard]] static StringRef asString(Result Outcome) {
  switch (Outcome) {
  case Result::TriviallySelfContained:
    return "trivially-self-contained";
  case Result::SelfContained:
    return "self-contained";
  case Result::Unknown:
    return "unknown";
  case Result::View:
    return "view";
  }
  llvm_unreachable("unknown Result");
}

static auto hasSwiftAttrAttrEqualTo(StringRef Str) {
  return [=](const SwiftAttrAttr *A) { return A->getAttribute() == Str; };
}
static bool hasSelfContainedAttr(const RecordDecl *D) {
  return llvm::any_of(D->specific_attrs<SwiftAttrAttr>(),
                      hasSwiftAttrAttrEqualTo("Escapable"));
}
static bool hasViewAttr(const RecordDecl *D) {
  return llvm::any_of(D->specific_attrs<SwiftAttrAttr>(),
                      hasSwiftAttrAttrEqualTo("~Escapable"));
}

static std::optional<Result>
hasEitherViewOrSelfContainedAttr(const RecordDecl *R) {
  if (hasSelfContainedAttr(R))
    return Result::SelfContained;
  if (hasViewAttr(R))
    return Result::View;
  return std::nullopt;
}

// Return the natural spelling of the field unless it's a lambda for which let's
// use the name of the capture.
static StringRef fieldDisplayName(const FieldDecl *F) {
  const auto *RD = dyn_cast_or_null<CXXRecordDecl>(F->getParent());
  if (RD && RD->isLambda()) {
    for (const auto &[Field, Cap] : zip_equal(RD->fields(), RD->captures())) {
      if (Field != F)
        continue;
      if (Cap.capturesThis())
        return "this";
      if (Cap.capturesVariable())
        return Cap.getCapturedVar()->getName();
      // Unreachable: the only capture that is neither 'this' nor a variable is
      // a VLA bound, whose implicit field is an integer.
      // We only name the reference, pointer and record fields.
      assert(false && "unexpected capture kind for a reported field");
      break; // fall through
    }
  }
  return F->getName();
}

namespace {
// List of context-rich diagnostic notes.
struct HoldsReferenceField {
  const FieldDecl *Field;
  void report(DiagnosticsEngine &Diags) const;
};

struct HoldsUnmanagedRawPtrField {
  const FieldDecl *Field;
  void report(DiagnosticsEngine &Diags) const;
};

struct HoldsManagedRawPtrField {
  const FieldDecl *Field;
  void report(DiagnosticsEngine &Diags) const;
};

struct DerivesFrom {
  CXXBaseSpecifier Base;
  Result Outcome;

  LLVM_ATTRIBUTE_RETURNS_NONNULL const CXXRecordDecl *asDecl() const {
    return Base.getType()->getAsCXXRecordDecl();
  }

  bool wasAnnotated() const {
    return hasEitherViewOrSelfContainedAttr(asDecl()).has_value();
  }
  void report(DiagnosticsEngine &Diags) const;
};

struct HoldsField {
  const FieldDecl *Field;
  const RecordDecl *CheckedRecord;
  Result Outcome;

  bool wasAnnotated() const {
    return hasEitherViewOrSelfContainedAttr(CheckedRecord).has_value();
  }
  void report(DiagnosticsEngine &Diags) const;
};

struct NotInstantiatedDestructor {
  const CXXDestructorDecl *Dtor;
  void report(DiagnosticsEngine &Diags) const;
};

struct OpaqueUserDeclaredDestructor {
  const CXXDestructorDecl *Dtor;
  void report(DiagnosticsEngine &Diags) const;
};
} // namespace

// holds {reference|unmanaged raw-pointer|managed raw-pointer} field %
void HoldsReferenceField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field)
      << /*reference=*/0 << fieldDisplayName(Field) << SourceRange(Loc);
}

// holds {reference|unmanaged raw-pointer|managed raw-pointer} field %
void HoldsUnmanagedRawPtrField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field)
      << /*unmanaged raw-pointer=*/1 << fieldDisplayName(Field)
      << SourceRange(Loc);
}

// holds {reference|unmanaged raw-pointer|managed raw-pointer} field %
void HoldsManagedRawPtrField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field)
      << /*managed raw-pointer=*/2 << fieldDisplayName(Field)
      << SourceRange(Loc);
}

// derives from class % that was {inferred|annotated} as '%'
void DerivesFrom::report(DiagnosticsEngine &Diags) const {
  // Point at the base-specifier (where the inheritance is expressed),
  // not the base class's own definition.
  SourceLocation Loc = Base.getBaseTypeLoc();
  Diags.Report(Loc, diag::note_ownership_inference_derives_from_base_inferred)
      << asDecl() << wasAnnotated() << asString(Outcome)
      << SourceRange(Loc);
}

// holds field % of type % that was {inferred|annotated} as '%'
void HoldsField::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Field->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_holds_field_inferred)
      << fieldDisplayName(Field) << CheckedRecord << wasAnnotated()
      << asString(Outcome) << SourceRange(Loc);
}

// destructor was not instantiated for this specialization
void NotInstantiatedDestructor::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Dtor->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_not_instantiated_dtor)
      << SourceRange(Loc);
}

// declares opaque destructor
void OpaqueUserDeclaredDestructor::report(DiagnosticsEngine &Diags) const {
  SourceLocation Loc = Dtor->getLocation();
  Diags.Report(Loc, diag::note_ownership_inference_opaque_dtor)
      << SourceRange(Loc);
}

namespace {
class Note {
public:
  /*implicit*/ Note(HoldsReferenceField R) : Data{std::move(R)} {}
  /*implicit*/ Note(HoldsUnmanagedRawPtrField R) : Data{std::move(R)} {}
  /*implicit*/ Note(HoldsManagedRawPtrField R) : Data{std::move(R)} {}
  /*implicit*/ Note(DerivesFrom R) : Data{std::move(R)} {}
  /*implicit*/ Note(HoldsField R) : Data{std::move(R)} {}
  /*implicit*/ Note(NotInstantiatedDestructor R) : Data{std::move(R)} {}
  /*implicit*/ Note(OpaqueUserDeclaredDestructor R) : Data{std::move(R)} {}

  void emitNotes(DiagnosticsEngine &Diags) const {
    std::visit([&](const auto &Item) { Item.report(Diags); }, Data);
  }

private:
  std::variant<HoldsReferenceField, HoldsUnmanagedRawPtrField,
               HoldsManagedRawPtrField, DerivesFrom, HoldsField,
               NotInstantiatedDestructor, OpaqueUserDeclaredDestructor>
      Data;
};
} // namespace

static void emitNotes(DiagnosticsEngine &Diags, ArrayRef<Note> Reasons) {
  for (const Note &R : Reasons) {
    R.emitNotes(Diags);
  }
}

namespace {
struct State {
  Result Outcome = Result::TriviallySelfContained;
  SmallVector<Note> Notes;

  void join(Result Other) & { Outcome = ::join(Outcome, Other); }

  void join(State &&Other) & {
    join(Other.Outcome);
    llvm::append_range(Notes, Other.Notes);
  }

  void addNote(Note &&N) & { Notes.emplace_back(std::move(N)); }

  void cantBeTriviallySelfContained() & { join(Result::SelfContained); }
};
} // namespace

static const Type *getElementOrValueTypeSingleStep(const Type *T) {
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
static QualType getTransitiveElementOrValueType(QualType Ty) {
  CanQualType CanonicalTy = Ty->getCanonicalTypeUnqualified();
  const Type *T = CanonicalTy->getTypePtr();
  while (true) {
    const Type *Curr = getElementOrValueTypeSingleStep(T);
    if (Curr == T)
      return QualType{T, /*Quals=*/0};
    T = Curr;
  }
}

namespace {
class SelfContainmentAnalysis final : public DynamicRecursiveASTVisitor {
public:
  explicit SelfContainmentAnalysis(DiagnosticsEngine &Diags) : Diags(Diags) {
    ShouldVisitTemplateInstantiations = true;
    ShouldWalkTypesOfTypeLocs = false;
    ShouldVisitImplicitCode = false;
    ShouldVisitLambdaBody = true;
  }

  /// Diagnose annotations that mismatch with the inference.
  /// \p R may be just a declaration, but only definitions will be inferred.
  bool VisitRecordDecl(RecordDecl *R) final;

private:
  /// Take the annotation of the class or infer one otherwise.
  /// \p R may be just a declaration.
  Result useOfClass(const RecordDecl *R);

  /// Report the inference outcome as a warning or a warning about failed
  /// inference.
  void reportSuggestionOrFailure(const RecordDecl *Def, const State &Res);

  /// Infer from the fields of record \p R.
  /// \p RawPointerFields gathers the raw-pointers in the process.
  State inferFieldsOfRecord(const RecordDecl *Def,
                            SmallVector<const FieldDecl *> &RawPointerFields);
  State inferBaseClassesOfRecord(const CXXRecordDecl *Def);

  /// Infer from the definition \p R and report the outcome.
  /// The result is cached, so each record is inferred and reported only once.
  State inferDefinitionOfRecord(const RecordDecl *Def);

  /// The check each raw-pointer field and decide if any of them are left
  /// unmanaged by the destructor.
  State handleRawPointerFields(const CXXRecordDecl *Def,
                               ArrayRef<const FieldDecl *> RawPointerFields);

  DiagnosticsEngine &Diags;

  /// The inference result of each record definition we have seen.
  DenseMap<const RecordDecl *, State> Cache;
};
} // namespace

bool SelfContainmentAnalysis::VisitRecordDecl(RecordDecl *R) {
  if (R->isDependentContext())
    return true;

  if (!R->isThisDeclarationADefinition())
    return true;

  // Always infer (even if the record has an attribute) to look for mismatches.
  State InferenceResult = inferDefinitionOfRecord(R);

  std::optional<Result> Annotation = hasEitherViewOrSelfContainedAttr(R);

  // The inference was inconclusive.
  if (InferenceResult.Outcome == Result::Unknown)
    return true;

  // No explicit attribute to check against.
  if (!Annotation.has_value())
    return true;

  // The annotation matches the inference.
  if (Annotation.value() == InferenceResult.Outcome)
    return true;

  unsigned MismatchDiagID = diag::warn_ownership_inference_annotation_mismatch;
  SourceLocation Loc = R->getLocation();
  if (!IsDiagEnabledExcludingWeverything(Diags, MismatchDiagID, Loc))
    return true;

  Diags.Report(Loc, MismatchDiagID)
      << R << SourceRange(Loc) << asString(Annotation.value())
      << asString(InferenceResult.Outcome);
  emitNotes(Diags, InferenceResult.Notes);
  return true;
}

Result SelfContainmentAnalysis::useOfClass(const RecordDecl *R) {
  // If has explicit annotation, trust that for the current use.
  if (auto TrustedResult = hasEitherViewOrSelfContainedAttr(R))
    return TrustedResult.value();

  // Infer if it didn't have one.
  if (const RecordDecl *Def = R->getDefinition())
    return inferDefinitionOfRecord(Def).Outcome;

  // Well, couldn't infer one and it also didn't have an explicit.
  return Result::Unknown;
}

void SelfContainmentAnalysis::reportSuggestionOrFailure(const RecordDecl *Def,
                                                        const State &Res) {
  unsigned DiagID;
  switch (Res.Outcome) {
  case Result::TriviallySelfContained:
    DiagID = diag::warn_ownership_inference_suggest_trivially_self_contained;
    break;
  case Result::SelfContained:
    DiagID = diag::warn_ownership_inference_suggest_self_contained;
    break;
  case Result::Unknown:
    DiagID = diag::warn_ownership_inference_failed_to_infer;
    break;
  case Result::View:
    DiagID = diag::warn_ownership_inference_suggest_view;
    break;
  }

  SourceLocation Loc = Def->getLocation();
  if (!IsDiagEnabledExcludingWeverything(Diags, DiagID, Loc))
    return;

  Diags.Report(Loc, DiagID) << Def << SourceRange(Loc);
  emitNotes(Diags, Res.Notes);
}

State SelfContainmentAnalysis::inferBaseClassesOfRecord(
    const CXXRecordDecl *Def) {
  assert(Def->isThisDeclarationADefinition());
  State BasesRes;

  for (const CXXBaseSpecifier &BaseSpec : Def->bases()) {
    CXXRecordDecl *Base = BaseSpec.getType()->getAsCXXRecordDecl();
    Result BaseRes = useOfClass(Base);
    BasesRes.join(State{BaseRes, {DerivesFrom{BaseSpec, BaseRes}}});
  }

  return BasesRes;
}

State SelfContainmentAnalysis::inferFieldsOfRecord(
    const RecordDecl *Def, SmallVector<const FieldDecl *> &RawPointerFields) {
  assert(Def->isThisDeclarationADefinition());
  State FieldsRes;

  // We only want to mark a class trivially self-contained if it has no fields.
  if (!Def->field_empty())
    FieldsRes.cantBeTriviallySelfContained();

  for (const auto *F : Def->fields()) {
    QualType Ty = getTransitiveElementOrValueType(F->getType());

    // Reference fields are not self-contained.
    if (Ty->isReferenceType()) {
      FieldsRes.join(State{Result::View, {HoldsReferenceField{F}}});
      continue;
    }

    // Pointer fields might or might not be self-contained.
    // It depends if they are properly managed or not so check them later.
    if (Ty->isPointerType()) {
      if (!Ty->isFunctionPointerType() && !Ty->isMemberPointerType()) {
        RawPointerFields.push_back(F);
      }
    }

    // Handle record fields.
    if (const auto *FieldRec = Ty->getAsRecordDecl()) {
      Result FieldRes = useOfClass(FieldRec);
      FieldsRes.join(State{FieldRes, {HoldsField{F, FieldRec, FieldRes}}});
    }
  }
  return FieldsRes;
}

static bool hasDefaultedDestructor(const CXXDestructorDecl *Dtor) {
  const auto *Def = cast_or_null<CXXDestructorDecl>(Dtor->getDefinition());
  return Def && Def->isDefaulted();
}

State SelfContainmentAnalysis::handleRawPointerFields(
    const CXXRecordDecl *Def, ArrayRef<const FieldDecl *> RawPointerFields) {
  assert(Def->isThisDeclarationADefinition());
  State RawPointerFieldResult;

  // If we don't have any fields to manage, then preserve the current outcome.
  if (RawPointerFields.empty())
    return RawPointerFieldResult;

  auto WithUnmanagedFields = [&RawPointerFieldResult,
                              RawPointerFields](Result Outcome) -> State && {
    RawPointerFieldResult.join(Outcome);
    for (const FieldDecl *F : RawPointerFields)
      RawPointerFieldResult.addNote(HoldsUnmanagedRawPtrField{F});
    return std::move(RawPointerFieldResult);
  };

  // Plain old classes can't manage memory, thus the fields.
  if (!Def)
    return WithUnmanagedFields(Result::View);

  // Classes without destructors also can't manage memory.
  const CXXDestructorDecl *Dtor = Def->getDestructor();
  if (!Dtor)
    return WithUnmanagedFields(Result::View);

  // Implicit or defaulted destructor: the raw pointer fields are unmanaged.
  if (!Def->hasUserDeclaredDestructor() || hasDefaultedDestructor(Dtor)) {
    return WithUnmanagedFields(Result::View);
  }

  // If the dtor body is present, check if it destroys the raw-pointer fields.
  // TODO: Implement this using LifetimeSafetyAnalysis and ownership_takes.
  if (Dtor->getBody()) {
    // For now, just return Unknown as this is not implemented.
    return WithUnmanagedFields(Result::Unknown);
  }

  // Limitation: Template classes may not instantiate the dtor of the class when
  // that is not used in the current TU. It may still have a body.
  if (const FunctionDecl *Pattern = Dtor->getTemplateInstantiationPattern();
      Pattern && Pattern->hasBody()) {
    // For now, let's track these instances with NotInstantiatedDestructor.
    RawPointerFieldResult.addNote(NotInstantiatedDestructor{Dtor});
    return WithUnmanagedFields(Result::Unknown);
  }

  // Limitation: We can't check if the user-declared opaque dtor destroys the
  // raw-pointer fields.
  RawPointerFieldResult.addNote(OpaqueUserDeclaredDestructor{Dtor});
  return WithUnmanagedFields(Result::Unknown);
}

State SelfContainmentAnalysis::inferDefinitionOfRecord(const RecordDecl *Def) {
  assert(Def->isThisDeclarationADefinition());
  if (auto It = Cache.find(Def); It != Cache.end())
    return It->second;

  State ClassRes;

  // Decompose into the base classes.
  if (const auto *Class = dyn_cast<CXXRecordDecl>(Def))
    if (Class->isCompleteDefinition())
      ClassRes.join(inferBaseClassesOfRecord(Class));

  // Decompose into the fields.
  SmallVector<const FieldDecl *> RawPointerFields;
  ClassRes.join(inferFieldsOfRecord(Def, RawPointerFields));

  // TODO: Also check the type of the pointee. Even if the pointer is managed in
  // the dtor, the pointee type must be self-contained to conclude that our
  // class is self-contained.

  // Check each raw-pointer field if they are managed by the destructor.
  ClassRes.join(
      handleRawPointerFields(dyn_cast<CXXRecordDecl>(Def), RawPointerFields));

  // Whatever our outcome, surface that as a warning.
  // Caching makes sure we only report these once per record.
  reportSuggestionOrFailure(Def, ClassRes);

  // Don't hold references into the cache across recursive inference, because
  // that can grow the map.
  auto [Place, Inserted] = Cache.try_emplace(Def, std::move(ClassRes));
  assert(Inserted);
  return Place->second;
}

void ownership::inferClassSelfContainment(ASTContext &Ctx) {
  SelfContainmentAnalysis{Ctx.getDiagnostics()}.TraverseAST(Ctx);
}

bool ownership::shouldRunSelfContainmentInference(const Sema &S) {
  const DiagnosticsEngine &Diags = S.getDiagnostics();
  constexpr unsigned DiagIDs[] = {
      diag::warn_ownership_inference_suggest_trivially_self_contained,
      diag::warn_ownership_inference_suggest_self_contained,
      diag::warn_ownership_inference_suggest_view,
      diag::warn_ownership_inference_failed_to_infer,
      diag::warn_ownership_inference_annotation_mismatch};
  // Don't enable this via -Weverything - see rdar://190623069.
  for (unsigned DiagID : DiagIDs)
    if (IsDiagEnabledExcludingWeverything(Diags, DiagID, SourceLocation()))
      return true;
  return false;
}
