//===--- SemaExpr.cpp - Semantic Analysis for Typed Memory Operations -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for typed memory operations.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTStructuralEquivalence.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Attr.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Overload.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/SemaHLSL.h"
#include "clang/Sema/SemaObjC.h"
#include "clang/Sema/SemaRISCV.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/TemplateDeduction.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"

using namespace clang;
using namespace sema;

static void emitTMODescriptorRemarks(
    Sema &S, const Expr *CallExpr, const FunctionDecl *Callee,
    const FunctionDecl *TargetFunction, SourceRange TypeRange,
    ASTContext::TypedMemoryDescriptor TMD,
    const InferredAllocationType &InferredType) {
  if (S.Diags.isIgnored(diag::remark_tmo_passed_type, CallExpr->getBeginLoc()))
    return;
  SourceRange CallRange = CallExpr->getSourceRange();

  TypedMemoryDescriptorBits TypeDescriptor = TMD.asBits();

  bool IsArrayAlloc =
      (TMD.Summary.CallsiteFlags & TypedMemoryCallsiteFlags::Array) ==
      TypedMemoryCallsiteFlags::Array;
  bool IsConstantArray =
      IsArrayAlloc &&
      !!(TMD.Summary.CallsiteFlags & TypedMemoryCallsiteFlags::FixedSize);

  StringRef Note = "";
  if (IsConstantArray)
    Note = "constant sized array of ";
  else if (IsArrayAlloc)
    Note = "array of ";

  std::string InferredTypeName = InferredType.describe(S.getASTContext());
  S.Diag(CallRange.getBegin(), diag::remark_tmo_passed_type)
      << Twine(Note, "type ") << *InferredType.primaryType() << TargetFunction
      << (Callee == TargetFunction) << Callee << TypeRange;
  // Note: we still only encode the primary type, not the full inferred
  // structure
  S.Diag(CallRange.getBegin(), diag::note_tmo_type_encoding)
      << Note << *InferredType.primaryType() << TypeDescriptor.value()
      << TMD.TypeDescription << TypeRange;
}

static void emitTMOInferenceDiagnostics(Sema &S, const Expr *CallExpr,
                                        const FunctionDecl *Callee,
                                        std::optional<QualType> Type,
                                        const InferredTypeInfo &TypeInfo,
                                        const FunctionDecl *RewriteTarget) {
  assert(S.getLangOpts().TypedMemoryOperations);
  assert(RewriteTarget);

  // Don't do any work if logging is not enabled
  bool WarnOnInferenceFailure = !S.Diags.isIgnored(
      diag::warn_tmo_inference_failed, CallExpr->getBeginLoc());
  bool EmitTMORemarks =
      !S.Diags.isIgnored(diag::remark_tmo_passed_type, CallExpr->getBeginLoc());
  if (!WarnOnInferenceFailure && !EmitTMORemarks)
    return;

  const InferredAllocationType *InferredType =
      TypeInfo.Type ? &*TypeInfo.Type : nullptr;

  const Expr *InferenceSourceExpression = TypeInfo.InferenceSourceExpression;
  SourceRange TypeSourceRange;
  if (const ExplicitCastExpr *CastExpr =
          dyn_cast<ExplicitCastExpr>(InferenceSourceExpression))
    TypeSourceRange =
        CastExpr->getTypeInfoAsWritten()->getTypeLoc().getSourceRange();
  else
    TypeSourceRange = InferenceSourceExpression->getSourceRange();

  llvm::scope_exit LogRewriteIfNecessary([&]() {
    if (!EmitTMORemarks)
      return;
    if (!Type) {
      S.Diag(TypeSourceRange.getBegin(), diag::note_tmo_failed_inference_source)
          << InferenceSourceExpression << TypeSourceRange;
      return;
    }
    bool WasInferredFromCast = isa<const CastExpr>(InferenceSourceExpression);
    const Expr *EffectiveExpr = InferenceSourceExpression;
    SourceRange EffectiveRange = TypeSourceRange;
    if (EffectiveRange.isInvalid()) {
      // A number of parts of sema introduce explicit CStyleCasts and similar
      // instead of ImplicitCastExprs, but also don't include the source range
      // of the cast sub expression either so we just substitute in the call
      // expression itself here.
      EffectiveExpr = CallExpr;
      EffectiveRange = CallExpr->getSourceRange();
    }
    assert(InferredType);
    const char *NotePrefix = InferredType->isArray() ? "array of " : "";
    std::string NoteDisplay = InferredType->describe(S.getASTContext());
    S.Diag(EffectiveRange.getBegin(), diag::note_tmo_inference_result)
        << Twine(NotePrefix, NoteDisplay) << WasInferredFromCast
        << EffectiveExpr->IgnoreUnlessSpelledInSource();
  });

  if (!Type) {
    S.Diag(CallExpr->getBeginLoc(), diag::warn_tmo_inference_failed) << Callee;
    return;
  }

  if (!EmitTMORemarks)
    return;

  assert(!(*Type)->isDependentType());
  TypedMemoryCallsiteFlags Flags = TypeInfo.InferredCallsiteFlags;
  ASTContext::TypedMemoryDescriptor TypeDescriptor =
      S.getASTContext().getTypedMemoryDescriptor(
          *Type, Callee->getDeclName().getCXXOverloadedOperator(), Flags);
  assert(InferredType);
  emitTMODescriptorRemarks(S, CallExpr, Callee, RewriteTarget, TypeSourceRange,
                           TypeDescriptor, *InferredType);
}

static void diagnoseTMOCastConflict(Sema &S, const CallExpr *Call,
                                    const FunctionDecl *Callee,
                                    QualType InferredType, const CastExpr *Cast,
                                    const Expr *InferenceSource) {
  if (!Cast)
    return;

  if (InferenceSource == Cast->IgnoreImplicit())
    return;

  QualType CastType = Cast->getType();
  if (!CastType->isAnyPointerType())
    return;

  QualType CastTarget = CastType->getPointeeType();
  ASTContext &Ctx = S.getASTContext();
  if (CastTarget->isArrayType())
    CastTarget = Ctx.getBaseElementType(CastTarget);
  if (CastTarget->isVoidType() || CastTarget->isDependentType() ||
      InferredType->isDependentType())
    return;

  if (CastTarget->isCharType())
    return;

  if (Ctx.hasSameUnqualifiedType(CastTarget, InferredType))
    return;

  S.Diag(Call->getBeginLoc(), diag::warn_tmo_inference_cast_conflict)
      << Callee << InferredType << CastTarget << Cast->getSourceRange();
}

static bool typedMemoryTypesAreEquivalent(const ASTContext &Context,
                                          QualType SourceType,
                                          QualType DestinationType) {
  SourceType = Context.getCanonicalType(SourceType).getUnqualifiedType();
  DestinationType =
      Context.getCanonicalType(DestinationType).getUnqualifiedType();
  return SourceType == DestinationType;
}

bool Sema::checkTypedMemorySignature(const AttributeCommonInfo &CI,
                                     const FunctionDecl *Source,
                                     const FunctionDecl *Target,
                                     ParamIdx InferredParameterIdx,
                                     bool SkipDependent) {
  SourceLocation Loc = CI.getLoc();
  auto RejectTarget = [&]() {
    Diag(Target->getLocation(), diag::note_tmo_rewrite_target_type_mismatch);
    return true;
  };
  auto TypesMatch = [&](QualType SourceType, QualType TargetType) {
    if (SkipDependent &&
        (SourceType->isDependentType() || TargetType->isDependentType()))
      return true;
    return typedMemoryTypesAreEquivalent(Context, SourceType, TargetType);
  };

  if (!TypesMatch(Source->getReturnType(), Target->getReturnType())) {
    Diag(Loc, diag::err_tmo_rewrite_target_return_type_mismatch)
        << Target << Source->getReturnType() << Source
        << Target->getReturnType();
    return RejectTarget();
  }

  unsigned ExpectedParamCount = getFunctionOrMethodNumParams(Source) + 1;
  if (getFunctionOrMethodNumParams(Target) != ExpectedParamCount) {
    Diag(Loc, diag::err_tmo_rewrite_target_arity_mismatch)
        << Target << ExpectedParamCount << Source
        << getFunctionOrMethodNumParams(Target);
    return RejectTarget();
  }

  const ParmVarDecl *DescriptorParam =
      getFunctionOrMethodParam(Target, InferredParameterIdx.getASTIndex() + 1);
  QualType DescriptorType = DescriptorParam->getType();
  if (DescriptorType->isDependentType()) {
    Diag(DescriptorParam->getLocation(),
         diag::err_tmo_dependent_type_descriptor)
        << Target << DescriptorType;
    return true;
  }
  if (!DescriptorType->isIntegerType() ||
      Context.getTypeSize(DescriptorType) != 64) {
    Diag(Loc, diag::err_tmo_rewrite_target_descriptor_type)
        << InferredParameterIdx.getSourceIndex() + 1 << Target
        << DescriptorType;
    return RejectTarget();
  }

  unsigned TargetParameterIdx = 0;
  for (unsigned SourceParameterIdx = 0;
       SourceParameterIdx != getFunctionOrMethodNumParams(Source);
       SourceParameterIdx++, TargetParameterIdx++) {
    const ParmVarDecl *SourceParam =
        getFunctionOrMethodParam(Source, SourceParameterIdx);
    const ParmVarDecl *TargetParam =
        getFunctionOrMethodParam(Target, TargetParameterIdx);
    if (!TypesMatch(SourceParam->getType(), TargetParam->getType())) {
      Diag(Loc, diag::err_tmo_rewrite_target_param_type_mismatch)
          << TargetParameterIdx + 1 << Target << SourceParam->getType()
          << Source << TargetParam->getType();
      return RejectTarget();
    }
    if (SourceParameterIdx == InferredParameterIdx.getASTIndex())
      TargetParameterIdx++;
  }
  return false;
}

void Sema::recordInfoForInferredCall(TMOInferenceCandidate Candidate) {
  if (!getLangOpts().TypedMemoryOperations)
    return;

  const CallExpr *Call = Candidate.Call;
  const auto *TMA = Call->getTypedMemoryAttribute();
  if (!TMA)
    return;
  const FunctionDecl *CalleeDecl = Call->getDirectCallee();
  if (!CalleeDecl)
    return;
  FunctionDecl *Target = TMA->getRewriteTarget();
  const Expr *InferredParameter =
      Call->getArg(TMA->getInferredParameterIdx().getLLVMIndex());

  if (Call->getDependence() != ExprDependence::None)
    return;
  MarkFunctionReferenced(Call->getExprLoc(), Target);

  const CastExpr *CastExpr = Candidate.Cast;
  if (CastExpr && CastExpr->getDependence() != ExprDependence::None) {
    // FIXME: maybe just discard?
    // CastExpr = nullptr;
    return;
  }

  InferredTypeInfo InferredInfo =
      getASTContext().inferTypedMemoryType(Call, *InferredParameter, CastExpr);

  std::optional<QualType> PrimaryType;
  if (InferredInfo.Type)
    PrimaryType = InferredInfo.Type->primaryType();
  if (PrimaryType)
    diagnoseTMOCastConflict(*this, Call, CalleeDecl, *PrimaryType, CastExpr,
                            InferredInfo.InferenceSourceExpression);
  emitTMOInferenceDiagnostics(*this, Call, CalleeDecl, PrimaryType,
                              InferredInfo, Target);
}

void Sema::drainTMOCandidates(unsigned FirstCandidateIndex) {
  assert(FirstCandidateIndex <= TMOCandidates.size());
  if (!getLangOpts().TypedMemoryOperations)
    return;

  for (unsigned I = FirstCandidateIndex; I != TMOCandidates.size(); ++I)
    recordInfoForInferredCall(TMOCandidates[I]);

  TMOCandidates.truncate(FirstCandidateIndex);
}

void Sema::finalizeOutstandingTMOCandidates() {
  if (!getLangOpts().TypedMemoryOperations)
    return;
  drainTMOCandidates(currentEvaluationContext().ContextHeadTMOIndex);
}

void Sema::forwardTMOCandidatesToEnclosingContext() {
  if (!getLangOpts().TypedMemoryOperations)
    return;
  assert(ExprEvalContexts.size() > 1 &&
         "ActOnStartStmtExpr always pushes a context to cede to");
  currentEvaluationContext().ContextHeadTMOIndex = TMOCandidates.size();
}

void Sema::recordCastForTMOInference(const CastExpr *Cast) {
  if (!getLangOpts().TypedMemoryOperations)
    return;

  if (TMOCandidates.empty())
    return;

  if (Cast->getType()->isVoidPointerType())
    return;

  if (isa<ImplicitCastExpr>(Cast))
    return;

  const Expr *PotentialCall = Cast->getSubExpr();
  const Expr *LastPotentialCall = nullptr;
  // We need to walk through all casts and implicit nodes between the cast
  // node and the actual underlying expression.
  do {
    LastPotentialCall = PotentialCall;
    PotentialCall = PotentialCall->IgnoreParens();
    PotentialCall = PotentialCall->IgnoreImplicit();
    PotentialCall = PotentialCall->IgnoreCasts();
    if (auto *OpaqueValue = dyn_cast<OpaqueValueExpr>(PotentialCall)) {
      if (const Expr *Source = OpaqueValue->getSourceExpr())
        PotentialCall = Source;
    }
    if (auto *SE = dyn_cast<StmtExpr>(PotentialCall)) {
      const CompoundStmt *SubStmt = SE->getSubStmt();
      if (const auto *Last = dyn_cast_or_null<ValueStmt>(SubStmt->body_back()))
        if (const Expr *LastExpr = Last->getExprStmt())
          PotentialCall = LastExpr;
    }
  } while (LastPotentialCall != PotentialCall);

  const CallExpr *Call = dyn_cast_or_null<CallExpr>(PotentialCall);
  if (!Call)
    return;

  if (!Call->getTypedMemoryAttribute())
    return;

  // We prioritize the first, i.e. deepest, non-implicit, non-void* cast.
  for (TMOInferenceCandidate &Candidate : llvm::reverse(TMOCandidates)) {
    if (Candidate.Call != Call)
      continue;
    if (!Candidate.Cast)
      Candidate.Cast = Cast;
    return;
  }
}

void Sema::emitTMODiagnosticsForTypeQuery(SourceLocation QueryLocation,
                                          SourceRange ExpressionRange,
                                          QualType QueriedType) {
  // Don't do any work if logging is not enabled
  if (Diags.isIgnored(diag::remark_tmo_passed_type, QueryLocation))
    return;

  if (QueriedType->isDependentType() || QueriedType->isIncompleteType())
    return;

  ASTContext::TypedMemoryDescriptor Descriptor =
      Context.getTypedMemoryDescriptor(QueriedType, OO_None,
                                       TypedMemoryCallsiteFlags::None);
  TypedMemoryDescriptorBits TMDB;
  TMDB.Summary = Descriptor.Summary;
  TMDB.Hash = Descriptor.IdentityHash;
  Diag(QueryLocation, diag::remark_tmo_get_descriptor_info)
      << QueriedType << TMDB.value() << Descriptor.TypeDescription
      << ExpressionRange;
}

bool Sema::checkTMOGetTypeDescriptor(QualType T, SourceLocation Loc,
                                     SourceRange ArgRange) {
  if (RequireCompleteSizedType(
          Loc, Context.getBaseElementType(T),
          diag::err_sizeof_alignof_incomplete_or_sizeless_type,
          getTraitSpelling(UETT_TMOGetTypeDescriptor), ArgRange))
    return true;
  assert(!T->isVoidType());
  emitTMODiagnosticsForTypeQuery(Loc, ArgRange, T);
  return false;
}

void Sema::recordTMOInferenceCandidate(const Expr *Call) {
  if (!getLangOpts().TypedMemoryOperations)
    return;
  // Unevaluated contexts definitionally don't produce a call, so there's no
  // reason to perform analysis.
  if (currentEvaluationContext().isUnevaluated())
    return;

  if (CurContext->isDependentContext())
    return;

  if (const VarDecl *Initialized =
          currentEvaluationContext().DeclForInitializer;
      Initialized && Initialized->isTemplated())
    return;

  if (RebuildingImmediateInvocation)
    return;
  const auto *CE = dyn_cast_or_null<CallExpr>(Call);
  if (!CE)
    return;
  if (!CE->getTypedMemoryAttribute())
    return;
  TMOCandidates.push_back(TMOInferenceCandidate{CE});
}

static bool hasParameterPack(const FunctionDecl *FD) {
  return llvm::any_of(FD->parameters(), [](const ParmVarDecl *Param) {
    return Param->isParameterPack();
  });
}

static QualType descriptorPlaceholderType(ASTContext &Context) {
  if (Context.getTypeSize(Context.UnsignedLongLongTy) == 64)
    return Context.UnsignedLongLongTy;
  if (QualType Exact = Context.getIntTypeForBitwidth(64, /*Signed=*/false);
      !Exact.isNull())
    return Exact;
  return Context.getBitIntType(/*IsUnsigned=*/true, 64);
}

static FunctionDecl *resolveTypedMemoryTarget(Sema &S,
                                              UnresolvedLookupExpr *ULE,
                                              const FunctionDecl *SourceDecl,
                                              ParamIdx InferredParameterIdx,
                                              SourceLocation Loc) {
  SmallVector<Expr *, 8> Args;
  auto addPlaceholder = [&](QualType T) {
    ExprValueKind ValueKind = VK_PRValue;
    if (const auto *Ref = T->getAs<ReferenceType>()) {
      ValueKind = isa<RValueReferenceType>(Ref) ? VK_XValue : VK_LValue;
      T = Ref->getPointeeType();
    }
    Args.push_back(new (S.Context) OpaqueValueExpr(Loc, T, ValueKind));
  };

  QualType DescriptorPlaceholder = descriptorPlaceholderType(S.Context);

  unsigned InferredIdx = InferredParameterIdx.getASTIndex();
  for (unsigned I = 0, E = SourceDecl->getNumParams(); I != E; ++I) {
    addPlaceholder(SourceDecl->getParamDecl(I)->getType());
    if (I == InferredIdx)
      addPlaceholder(DescriptorPlaceholder);
  }

  OverloadCandidateSet Candidates(Loc, OverloadCandidateSet::CSK_Normal);
  S.AddOverloadedCallCandidates(ULE, Args, Candidates);
  OverloadCandidateSet::iterator Best;
  OverloadingResult Result = Candidates.BestViableFunction(S, Loc, Best);
  if (Result == OR_Success || Result == OR_Deleted) {
    // The TMO redirection target must be accessible by the source declaration
    Sema::AccessResult Access =
        S.CheckUnresolvedLookupAccess(ULE, Best->FoundDecl);
    bool Unusable = S.DiagnoseUseOfDecl(Best->Function, Loc);
    if (Access == Sema::AR_inaccessible || Unusable)
      return nullptr;
    if (Result == OR_Success)
      return Best->Function;
  }

  if (ULE->isTypeDependent()) {
    S.Diag(Loc, diag::err_tmo_dependent_template_target)
        << ULE->getNameInfo().getName() << ULE->getSourceRange();
    return nullptr;
  }

  if (Result == OR_Ambiguous)
    S.Diag(Loc, diag::err_tmo_rewrite_target_is_overloaded)
        << ULE->getNameInfo().getName();
  else
    S.Diag(Loc, diag::err_tmo_rewrite_target_no_match)
        << ULE->getNameInfo().getName() << SourceDecl;
  if (ULE->getType() == S.Context.OverloadTy)
    S.NoteAllOverloadCandidates(ULE);
  return nullptr;
}

void Sema::handleTypedMemoryAttr(Decl *D, const ParsedAttr &AL) {
  if (!getLangOpts().TypedMemoryOperations)
    return;

  auto Loc = AL.getLoc();
  FunctionDecl *SourceDecl = D->getAsFunction();
  if (!SourceDecl) {
    auto *ND = cast<NamedDecl>(D);
    Diag(Loc, diag::err_tmo_function_kind_error)
        << 0 << ND << diag::TMOErrorKind::NotAFunction;
    AL.setInvalid();
    return;
  }
  // There are a number of cases we simply do not permit to be subject to TMO:
  // * K & R declarations have synthesized parameters that we can't reason
  //   about.
  // * Variadic functions and functions with packs: conservative but has not yet
  //   proven to be a problem.
  // * Instance methods: again conservative, as yet has not seemed to be a
  //   problem.
  // * Overloaded operators: intentionally not supported, because it results in
  //   downstream sema weirdness due to different AST nodes being involved, and
  //   has extremely questionable value.
  if (!SourceDecl->hasWrittenPrototype() || isFunctionOrMethodVariadic(D) ||
      hasParameterPack(SourceDecl) || isInstanceMethod(D) ||
      SourceDecl->isOverloadedOperator()) {
    auto MessageSelector =
        !SourceDecl->hasWrittenPrototype() ? diag::TMOErrorKind::NoPrototype
        : isFunctionOrMethodVariadic(D)    ? diag::TMOErrorKind::Variadic
        : hasParameterPack(SourceDecl)     ? diag::TMOErrorKind::ParameterPack
        : SourceDecl->isOverloadedOperator()
            ? diag::TMOErrorKind::OverloadedOperator
            : diag::TMOErrorKind::InstanceMethod;
    Diag(SourceDecl->getBeginLoc(), diag::err_tmo_function_kind_error)
        << 0 << SourceDecl << MessageSelector;
    AL.setInvalid();
    return;
  }

  ParamIdx InferredParameterIdx;
  if (!checkFunctionOrMethodParameterIndex(D, AL, 1, AL.getArgAsExpr(1),
                                           InferredParameterIdx))
    return;

  auto *InferredParam =
      SourceDecl->getParamDecl(InferredParameterIdx.getASTIndex());
  auto SizeType = InferredParam->getType();
  if (!SizeType->getUnqualifiedDesugaredType()->isIntegerType()) {
    Diag(Loc, diag::err_tmo_invalid_inferred_parameter_type)
        << InferredParameterIdx.getSourceIndex() << SizeType
        << InferredParam->getLocation();
    AL.setInvalid();
    return;
  }

  Expr *TargetExpr = AL.getArgAsExpr(0);
  FunctionDecl *TargetDecl = nullptr;
  DeclarationNameInfo TargetName;
  if (auto *DRE = dyn_cast<DeclRefExpr>(TargetExpr)) {
    TargetDecl = dyn_cast<FunctionDecl>(DRE->getDecl());
    TargetName = DRE->getNameInfo();
    if (!TargetDecl) {
      Diag(Loc, diag::err_tmo_function_kind_error)
          << DRE->getSourceRange() << 1 << DRE->getNameInfo().getName()
          << diag::TMOErrorKind::NotAFunction;
      AL.setInvalid();
      return;
    }
  } else if (auto *ULE = dyn_cast<UnresolvedLookupExpr>(TargetExpr)) {
    TargetName = ULE->getNameInfo();
    TargetDecl = resolveTypedMemoryTarget(*this, ULE, SourceDecl,
                                          InferredParameterIdx, Loc);
    if (!TargetDecl) {
      AL.setInvalid();
      return;
    }
  } else {
    Diag(Loc, diag::err_tmo_function_kind_error)
        << TargetExpr->getSourceRange() << 1 << TargetExpr
        << diag::TMOErrorKind::NotAFunction;
    AL.setInvalid();
    return;
  }

  TargetDecl = TargetDecl->getCanonicalDecl();

  std::optional<unsigned> ErrorKind;
  if (!TargetDecl->hasWrittenPrototype())
    ErrorKind = diag::TMOErrorKind::NoPrototype;
  else if (isFunctionOrMethodVariadic(TargetDecl))
    ErrorKind = diag::TMOErrorKind::Variadic;
  else if (hasParameterPack(TargetDecl))
    ErrorKind = diag::TMOErrorKind::ParameterPack;
  else if (isInstanceMethod(TargetDecl))
    ErrorKind = diag::TMOErrorKind::InstanceMethod;

  if (ErrorKind) {
    Diag(Loc, diag::err_tmo_function_kind_error)
        << 1 << TargetDecl << *ErrorKind;
    AL.setInvalid();
    return;
  }

  if (TargetDecl->getPrimaryTemplate() && TargetDecl->isTemplated()) {
    Diag(Loc, diag::err_tmo_dependent_template_target)
        << TargetName.getName() << TargetExpr->getSourceRange();
    AL.setInvalid();
    return;
  }

  if (checkTypedMemorySignature(AL, SourceDecl, TargetDecl,
                                InferredParameterIdx,
                                /*SkipDependent=*/true)) {
    AL.setInvalid();
    return;
  }

  auto *TMA = ::new (Context)
      TypedMemoryAttr(Context, AL, TargetDecl, InferredParameterIdx);
  D->addAttr(TMA);
}

static bool hasDependentSpecializationArguments(const FunctionDecl *FD) {
  const TemplateArgumentList *Args = FD->getTemplateSpecializationArgs();
  return Args && llvm::any_of(Args->asArray(), [](const TemplateArgument &Arg) {
           return Arg.isDependent();
         });
}

// FindInstantiatedDecl only maps a member of the enclosing template, so a
// specialization of a template declared outside it is rebuilt here instead.
static FunctionDecl *
substituteTypedMemoryTarget(Sema &S,
                            const MultiLevelTemplateArgumentList &TemplateArgs,
                            FunctionDecl *Target, SourceLocation Loc) {
  FunctionTemplateDecl *Template = Target->getPrimaryTemplate();
  const TemplateArgumentList *Args = Target->getTemplateSpecializationArgs();
  if (!Template || !Args)
    return nullptr;

  // Not the arguments as written: a candidate selected by overload resolution
  // carries no written form, and the resolved list is complete where some were
  // deduced.
  TemplateArgumentListInfo Resolved;
  for (const TemplateArgument &Argument : Args->asArray())
    Resolved.addArgument(
        S.getTrivialTemplateArgumentLoc(Argument, QualType(), Loc));

  TemplateArgumentListInfo Substituted;
  if (S.SubstTemplateArguments(Resolved.arguments(), TemplateArgs, Substituted))
    return nullptr;

  FunctionDecl *Specialization = nullptr;
  sema::TemplateDeductionInfo Info(Loc);
  if (S.DeduceTemplateArguments(Template, &Substituted, Specialization, Info,
                                /*IsAddressOfFunction=*/true) !=
      TemplateDeductionResult::Success)
    return nullptr;
  return Specialization;
}

// Instantiate a concrete typed memory attribute for the instantiation of a
// a dependent TMO attributed function.
void Sema::instantiateTypedMemoryAttr(
    const MultiLevelTemplateArgumentList &TemplateArgs,
    const TypedMemoryAttr *Attr, Decl *New) {
  if (New->hasAttr<TypedMemoryAttr>())
    return;
  FunctionDecl *NewSource = New->getAsFunction();
  if (!NewSource)
    return;
  FunctionDecl *Target = Attr->getRewriteTarget();
  FunctionDecl *NewTarget = nullptr;
  if (hasDependentSpecializationArguments(Target)) {
    NewTarget = substituteTypedMemoryTarget(*this, TemplateArgs, Target,
                                            Attr->getLocation());
  } else {
    NamedDecl *Instantiated =
        FindInstantiatedDecl(Attr->getLocation(), Target, TemplateArgs);
    NewTarget = dyn_cast_or_null<FunctionDecl>(Instantiated);
  }
  if (!NewTarget)
    return;
  ParamIdx InferredParameterIdx = Attr->getInferredParameterIdx();
  if (checkTypedMemorySignature(*Attr, NewSource, NewTarget,
                                InferredParameterIdx,
                                /*SkipDependent=*/New->isTemplated()))
    return;
  New->addAttr(::new (getASTContext()) TypedMemoryAttr(
      getASTContext(), *Attr, NewTarget->getCanonicalDecl(),
      InferredParameterIdx));
}
