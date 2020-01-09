//===--- ByteCodeExprGen.cpp - Code generator for expressions ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ByteCodeExprGen.h"
#include "ByteCodeEmitter.h"
#include "ByteCodeGenError.h"
#include "Context.h"
#include "Function.h"
#include "PrimType.h"
#include "Program.h"
#include "State.h"
#include "TypeHelper.h"
#include "clang/AST/RecordLayout.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/SourceManager.h"

using namespace clang;
using namespace clang::interp;

using APSInt = llvm::APSInt;
template <typename T> using Expected = llvm::Expected<T>;
template <typename T> using Optional = llvm::Optional<T>;

namespace clang {
namespace interp {

/// Scope used to handle temporaries in toplevel variable declarations.
template <class Emitter> class DeclScope final : public LocalScope<Emitter> {
public:
  DeclScope(ByteCodeExprGen<Emitter> *Ctx, const VarDecl *VD)
      : LocalScope<Emitter>(Ctx), Scope(Ctx->P, VD) {}

  void addExtended(const Scope::Local &Local) override {
    return this->addLocal(Local);
  }

private:
  Program::DeclScope Scope;
};

/// Scope used to handle initialization methods.
template <class Emitter> class OptionScope {
public:
  using InitFnRef = typename ByteCodeExprGen<Emitter>::InitFnRef;
  using InitKind = typename ByteCodeExprGen<Emitter>::InitKind;
  using ChainedInitFnRef = std::function<bool(InitFnRef)>;

  /// Root constructor, compiling or discarding primitives.
  OptionScope(ByteCodeExprGen<Emitter> *Ctx, bool NewDiscardResult)
      : Ctx(Ctx), OldDiscardResult(Ctx->DiscardResult),
        OldInitFn(std::move(Ctx->InitFn)),
        OldInitialiser(Ctx->Initialiser) {
    Ctx->DiscardResult = NewDiscardResult;
    Ctx->InitFn = llvm::Optional<InitFnRef>{};
    Ctx->Initialiser = InitKind::ROOT;
  }

  /// Root constructor, setting up compilation state.
  OptionScope(ByteCodeExprGen<Emitter> *Ctx, InitFnRef NewInitFn,
              InitKind NewInitialiser = InitKind::ROOT)
      : Ctx(Ctx), OldDiscardResult(Ctx->DiscardResult),
        OldInitFn(std::move(Ctx->InitFn)),
        OldInitialiser(Ctx->Initialiser) {
    Ctx->DiscardResult = true;
    Ctx->InitFn = NewInitFn;
    Ctx->Initialiser = NewInitialiser;
  }

  ~OptionScope() {
    Ctx->DiscardResult = OldDiscardResult;
    Ctx->InitFn = std::move(OldInitFn);
    Ctx->Initialiser = OldInitialiser;
  }

protected:
  /// Extends the chain of initialisation pointers.
  OptionScope(ByteCodeExprGen<Emitter> *Ctx, ChainedInitFnRef NewInitFn,
              InitKind NewInitialiser)
      : Ctx(Ctx), OldDiscardResult(Ctx->DiscardResult),
        OldInitFn(std::move(Ctx->InitFn)), OldInitialiser(Ctx->Initialiser) {
    assert(OldInitFn && "missing initializer");
    Ctx->InitFn = [this, NewInitFn] { return NewInitFn(*OldInitFn); };
    Ctx->Initialiser = NewInitialiser;
  }

private:
  /// Parent context.
  ByteCodeExprGen<Emitter> *Ctx;
  /// Old discard flag to restore.
  bool OldDiscardResult;
  /// Old pointer emitter to restore.
  llvm::Optional<InitFnRef> OldInitFn;
  /// Base flag to restore.
  InitKind OldInitialiser;
};

// Scope which initialises a base class.
template <class Emitter> class BaseScope : public OptionScope<Emitter> {
public:
  using ChainedInitFnRef = typename OptionScope<Emitter>::ChainedInitFnRef;
  using InitKind = typename OptionScope<Emitter>::InitKind;
  BaseScope(ByteCodeExprGen<Emitter> *Ctx, ChainedInitFnRef FieldFn)
    : OptionScope<Emitter>(Ctx, FieldFn, InitKind::BASE) {}
};

// Scope which initialises a union field.
template <class Emitter> class UnionScope : public OptionScope<Emitter> {
public:
  using ChainedInitFnRef = typename OptionScope<Emitter>::ChainedInitFnRef;
  using InitKind = typename OptionScope<Emitter>::InitKind;
  UnionScope(ByteCodeExprGen<Emitter> *Ctx, ChainedInitFnRef FieldFn)
    : OptionScope<Emitter>(Ctx, FieldFn, InitKind::UNION) {}
};

// Scope which initialises a record field or array element.
template <class Emitter> class FieldScope : public OptionScope<Emitter> {
public:
  using ChainedInitFnRef = typename OptionScope<Emitter>::ChainedInitFnRef;
  using InitKind = typename OptionScope<Emitter>::InitKind;
  FieldScope(ByteCodeExprGen<Emitter> *Ctx, ChainedInitFnRef FieldFn)
    : OptionScope<Emitter>(Ctx, FieldFn, InitKind::ROOT) {}
};

} // namespace interp
} // namespace clang

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitDeclRefExpr(const DeclRefExpr *DE) {
  if (auto *VD = dyn_cast<VarDecl>(DE->getDecl())) {
    auto It = CaptureFields.find(VD);
    if (It != CaptureFields.end()) {
      const FieldDecl *FD = It->second;
      return withField(FD, [this, FD, DE](const Record::Field *F) {
        if (FD->getType()->isReferenceType()) {
          if (!this->emitGetThisFieldPtr(F->getOffset(), DE))
            return false;
        } else {
          if (!this->emitGetPtrThisField(F->getOffset(), DE))
            return false;
        }
        return DiscardResult ? this->emitPopPtr(DE) : true;
      });
    }
  }

  if (DiscardResult)
    return true;

  if (isa<ObjCIvarDecl>(DE->getDecl()))
    return this->emitTrap(DE);

  if (auto *PD = dyn_cast<ParmVarDecl>(DE->getDecl())) {
    QualType Ty = PD->getType();
    auto It = this->Params.find(PD);
    if (It == this->Params.end()) {
      if (auto *PtrTy = Ty->getAs<PointerType>()) {
        if (PtrTy->getPointeeType()->isFunctionType())
          return this->emitGetDummyFnPtr(PD, DE);
        return emitDummyPtr(PtrTy->getPointeeType(), PD, DE);
      }
      if (auto *RefTy = Ty->getAs<ReferenceType>()) {
        if (RefTy->getPointeeType()->isFunctionType())
          return this->emitGetDummyFnPtr(PD, DE);
        return this->emitTrap(DE);
      }
      // Try to emit a dummy pointer, trap if not possible.
      return emitDummyPtr(Ty, PD, DE);
    } else {
      // Generate a pointer to a parameter.
      if (Ty->isFunctionReferenceType())
        return this->emitGetParamFnPtr(It->second, DE);
      else if (Ty->isReferenceType() || !classify(Ty))
        return this->emitGetParamPtr(It->second, DE);
      else
        return this->emitGetPtrParam(It->second, DE);
    }
  }
  if (auto *VD = dyn_cast<VarDecl>(DE->getDecl())) {
    auto It = Locals.find(VD);
    if (It == Locals.end()) {
      return getPtrVarDecl(VD, DE);
    } else {
      // Generate a pointer to a local.
      if (VD->getType()->isReferenceType())
        return this->emitGetLocal(PT_Ptr, It->second.Offset, DE);
      else
        return this->emitGetPtrLocal(It->second.Offset, DE);
    }
  }
  if (auto *ED = dyn_cast<EnumConstantDecl>(DE->getDecl())) {
    QualType Ty = ED->getType();
    if (Optional<PrimType> T = classify(Ty))
      return this->emitConst(*T, getIntWidth(Ty), ED->getInitVal(), DE);
    return false;
  }
  if (auto *FD = dyn_cast<FunctionDecl>(DE->getDecl())) {
    if (auto *CD = dyn_cast<CXXMethodDecl>(DE->getDecl()))
      if (!CD->isStatic())
        return this->emitConstMem(CD, DE);
    return getPtrConstFn(FD, DE);
  }
  if (auto *BD = dyn_cast<BindingDecl>(DE->getDecl()))
    return this->Visit(BD->getBinding());
  if (auto *FD = dyn_cast<FieldDecl>(DE->getDecl()))
    return this->emitConstMem(FD, DE);

  // TODO: compile other decls.
  return this->bail(DE);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCastExpr(const CastExpr *CE) {
  auto *SubExpr = CE->getSubExpr();
  switch (CE->getCastKind()) {

  case CK_LValueToRValue:
    return lvalueToRvalue(CE->getSubExpr(), CE);

  case CK_MemberPointerToBoolean:
  case CK_PointerToBoolean:
  case CK_IntegralToBoolean: {
    if (DiscardResult)
      return discard(SubExpr);
    if (!visit(SubExpr))
      return false;
    if (auto T = classify(SubExpr->getType()))
      return this->emitTest(*T, PT_Bool, CE);
    return this->emitTrap(CE);
  }

  case CK_IntegralCast: {
    if (DiscardResult)
      return discard(SubExpr);

    if (!visit(SubExpr))
      return false;

    QualType ArgTy = SubExpr->getType();
    QualType RetTy = CE->getType();
    auto ArgT = *classify(ArgTy);
    auto RetT = *classify(RetTy);
    if (isFixedIntegral(RetT))
      return this->emitCastFP(ArgT, RetT, getIntWidth(RetTy), CE);
    else
      return this->emitCast(ArgT, RetT, CE);
  }

  case CK_FloatingCast:
  case CK_IntegralToFloating: {
    if (DiscardResult)
      return discard(SubExpr);

    if (!visit(SubExpr))
      return false;

    QualType ArgTy = SubExpr->getType();
    QualType RetTy = CE->getType();
    auto ArgT = *classify(ArgTy);
    return this->emitCastRealFP(ArgT, getFltSemantics(RetTy), CE);
  }

  case CK_FloatingToIntegral: {
    if (!visit(SubExpr))
      return false;

    QualType RetTy = CE->getType();
    PrimType ArgT = *classify(SubExpr->getType());
    PrimType RetT = *classify(RetTy);
    if (isFixedIntegral(RetT)) {
      if (!this->emitCastRealFPToAluFP(ArgT, RetT, getIntWidth(RetTy), CE))
        return false;
    } else {
      if (!this->emitCastRealFPToAlu(ArgT, RetT, CE))
        return false;
    }

    return DiscardResult ? this->emitPop(RetT, CE) : true;
  }

  case CK_NullToPointer:
  case CK_NullToMemberPointer:
    // Emit a null pointer, avoiding redundancy when casting from nullptr.
    if (!isa<CXXNullPtrLiteralExpr>(SubExpr)) {
      if (!discard(SubExpr))
        return false;
    }
    if (DiscardResult)
      return true;
    return visitZeroInitializer(classifyPrim(CE->getType()), CE);

  case CK_ArrayToPointerDecay:
   if (!this->Visit(SubExpr))
      return false;
    if (DiscardResult)
      return true;
    return this->emitDecayPtr(CE);

  case CK_AtomicToNonAtomic:
  case CK_ConstructorConversion:
  case CK_FunctionToPointerDecay:
  case CK_NonAtomicToAtomic:
  case CK_NoOp:
  case CK_UserDefinedConversion:
    return this->Visit(SubExpr);

  case CK_ToVoid:
    return discard(SubExpr);

  case CK_IntegralRealToComplex:
  case CK_FloatingRealToComplex:
    return visitCastToComplex(CE);

  case CK_IntegralComplexToReal:
  case CK_FloatingComplexToReal:
    return visitCastToReal(CE);

  case CK_DerivedToBase:
  case CK_UncheckedDerivedToBase: {
    if (!visit(SubExpr))
      return false;

    const Record *R = getRecord(SubExpr->getType());
    if (!R)
      return false;

    for (auto *Step : CE->path()) {
      auto *Decl = Step->getType()->getAs<RecordType>()->getDecl();

      const Record::Base *Base;
      if (Step->isVirtual()) {
        Base = R->getVirtualBase(Decl);
        if (!this->emitGetPtrVirtBase(Decl, CE))
          return false;
      } else {
        Base = R->getBase(Decl);
        if (!this->emitGetPtrBase(Base->getOffset(), CE))
          return false;
      }

      R = Base->getRecord();
    }

    return DiscardResult ? this->emitPopPtr(CE) : true;
  }

  case CK_BaseToDerived: {
    if (!visit(SubExpr))
      return false;
    if (!this->emitCastToDerived(CE, CE))
      return false;
    return DiscardResult ? this->emitPopPtr(CE) : true;
  }

  case CK_DerivedToBaseMemberPointer: {
    if (!visit(SubExpr))
      return false;
    for (auto *Step : CE->path()) {
      auto *R = Step->getType()->getAs<RecordType>()->getDecl();
      if (!this->emitCastMemberToBase(cast<CXXRecordDecl>(R), CE))
        return false;
    }
    return DiscardResult ? this->emitPopMemPtr(CE) : true;
  }

  case CK_BaseToDerivedMemberPointer: {
    if (CE->path_empty())
      return discard(SubExpr);

    if (!visit(SubExpr))
      return false;

    // Single cast step from base to derived.
    auto Step = [this, CE](const Type *T) {
      auto *R = cast<CXXRecordDecl>(T->getAs<RecordType>()->getDecl());
      return this->emitCastMemberToDerived(R, CE);
    };

    // Base-to-derived member pointer casts store the path in derived-to-base
    // order, so iterate backwards. The CXXBaseSpecifier also provides us with
    // the wrong end of the derived->base arc, so stagger the path by one class.
    typedef std::reverse_iterator<CastExpr::path_const_iterator> Rev;
    for (auto It = Rev(CE->path_end() - 1); It != Rev(CE->path_begin()); ++It) {
      if (!Step((*It)->getType().getTypePtr()))
        return false;
    }
    // The final type is encoded in the type of the cast.
    if (!Step(CE->getType()->getAs<MemberPointerType>()->getClass()))
      return false;
    return DiscardResult ? this->emitPopMemPtr(CE) : true;
  }

  case CK_AddressSpaceConversion: {
    // If the pointees are different, emit a bit cast.
    const auto &ASTCtx = Ctx.getASTContext();
    if (auto *SrcPtr = SubExpr->getType()->getAs<PointerType>()) {
      if (auto *DstPtr = CE->getType()->getAs<PointerType>()) {
        // Skip the conversion if types are identical.
        auto SrcTy = ASTCtx.removeAddrSpaceQualType(SrcPtr->getPointeeType());
        auto DstTy = ASTCtx.removeAddrSpaceQualType(DstPtr->getPointeeType());
        if (ASTCtx.hasSameType(SrcTy, DstTy))
          return this->Visit(SubExpr);

        // Lower the source pointer.
        if (!this->visit(SubExpr))
          return false;

        // Emit the appropriate bitcast.
        if (llvm::Optional<PrimType> SrcT = classify(SubExpr->getType())) {
          if (llvm::Optional<PrimType> DstT = classify(CE->getType())) {
            return this->emitBitCast(*SrcT, *DstT, CE);
          }
        }
      }
    }
    return this->emitTrap(SubExpr);
  }

  case CK_BitCast:
  case CK_IntegralToPointer:
  case CK_PointerToIntegral: {
    // Check whether the types are identical.
    const auto &ASTCtx = Ctx.getASTContext();
    QualType SrcTy = SubExpr->getType().getCanonicalType();
    QualType DstTy = CE->getType().getCanonicalType();
    if (ASTCtx.hasSameType(SrcTy, DstTy))
      return this->Visit(SubExpr);

    // Lower the source pointer.
    if (!this->visit(SubExpr))
      return false;

    // Emit the relevant bit cast.
    if (llvm::Optional<PrimType> SrcT = classify(SrcTy)) {
      if (llvm::Optional<PrimType> DstT = classify(DstTy)) {
        if (!this->emitBitCast(*SrcT, *DstT, CE))
          return false;
        return DiscardResult ? this->emitPop(*DstT, CE) : true;
      }
    }
    return this->emitTrap(SubExpr);
  }

  case CK_FloatingComplexCast: {
    // Lower the source pointer.
    if (!this->Visit(SubExpr))
      return false;
    if (DiscardResult)
      return true;

    // Emit the conversion.
    auto SrcElemTy = SubExpr->getType()->getAs<ComplexType>()->getElementType();
    auto DstElemTy = CE->getType()->getAs<ComplexType>()->getElementType();
    if (llvm::Optional<PrimType> SrcT = classify(SrcElemTy)) {
      if (llvm::Optional<PrimType> DstT = classify(DstElemTy)) {
        llvm_unreachable("not implemented");
      }
    }
    return this->bail(CE);
  }

  default: {
    // TODO: implement other casts.
    return this->bail(CE);
  }
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitConstantExpr(const ConstantExpr *CE) {
  switch (CE->getResultStorageKind()) {
    case ConstantExpr::RSK_Int64: {
      if (DiscardResult)
        return true;
      if (!CE->isRValue())
        return this->Visit(CE->getSubExpr());
      QualType Ty = CE->getType();
      PrimType T = *classify(Ty);
      return this->emitConst(T, getIntWidth(Ty), CE->getResultAsAPSInt(), CE);
    }

    case ConstantExpr::RSK_APValue:
      return this->bail(CE);

    case ConstantExpr::RSK_None:
      return this->Visit(CE->getSubExpr());
  }
  llvm_unreachable("invalid storage kind");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitIntegerLiteral(const IntegerLiteral *LE) {
  if (DiscardResult)
    return true;

  auto Val = LE->getValue();
  QualType LitTy = LE->getType();
  if (Optional<PrimType> T = classify(LitTy))
    return emitConst(*T, getIntWidth(LitTy), LE->getValue(), LE);
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitFloatingLiteral(const FloatingLiteral *E) {
  if (DiscardResult)
    return true;
  return this->emitConstRealFP(E, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitStringLiteral(const StringLiteral *E) {
  if (InitFn)
    return visitStringInitializer(E);
  if (DiscardResult)
    return true;
  return this->emitGetPtrGlobal(P.createGlobalString(E, E), E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitPredefinedExpr(const PredefinedExpr *E) {
  return VisitStringLiteral(E->getFunctionName());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCharacterLiteral(
    const CharacterLiteral *CE) {
  if (DiscardResult)
    return true;

  QualType CharTy = CE->getType();
  if (Optional<PrimType> T = classify(CharTy)) {
    const unsigned NumBits = sizeof(unsigned) * CHAR_BIT;
    APInt Char(NumBits, static_cast<uint64_t>(CE->getValue()), false);
    return this->emitConst(*T, getIntWidth(CharTy), Char, CE);
  }
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitImaginaryLiteral(
    const ImaginaryLiteral *E) {
  const Expr *SubExpr = E->getSubExpr();
  QualType ElemType = SubExpr->getType();

  PrimType T = classifyPrim(ElemType);
  ImplicitValueInitExpr ZE(ElemType);

  if (!emitInitFn())
    return false;
  if (!visit(&ZE))
    return false;
  if (!this->emitInitElem(T, 0, E))
    return false;
  if (!visit(SubExpr))
    return false;
  return this->emitInitElemPop(T, 1, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitParenExpr(const ParenExpr *PE) {
  return this->Visit(PE->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitBinaryOperator(const BinaryOperator *BO) {
  const Expr *LHS = BO->getLHS();
  const Expr *RHS = BO->getRHS();

  // Deal with operations which have composite or void types.
  switch (BO->getOpcode()) {
  case BO_PtrMemD:
  case BO_PtrMemI:
    return visitIndirectMember(BO);
  case BO_Cmp:
    return visitSpaceship(BO);
  case BO_Comma:
    if (!discard(LHS))
      return false;
    if (!this->Visit(RHS))
      return false;
    return true;
  default:
    break;
  }

  // Typecheck the args.
  Optional<PrimType> LT = classify(LHS->getType());
  Optional<PrimType> RT = classify(RHS->getType());
  Optional<PrimType> T = classify(BO->getType());
  if (!LT || !RT || !T) {
    return this->bail(BO);
  }

  switch (BO->getOpcode()) {
  case BO_Assign:
    return visitAssign(*T, BO);

  case BO_AddAssign:
    if (isPointer(*LT))
      return visitPtrAssign(*RT, &ByteCodeExprGen::emitAddOffset, BO);
    else
      return visitCompoundAssign(*RT, &ByteCodeExprGen::emitAdd, BO);
  case BO_SubAssign:
    if (isPointer(*LT))
      return visitPtrAssign(*RT, &ByteCodeExprGen::emitSubOffset, BO);
    else
      return visitCompoundAssign(*RT, &ByteCodeExprGen::emitSub, BO);

  case BO_MulAssign:
    return visitCompoundAssign(*RT, &ByteCodeExprGen::emitMul, BO);
  case BO_DivAssign:
    return visitCompoundAssign(*RT, &ByteCodeExprGen::emitDiv, BO);
  case BO_RemAssign:
    return visitCompoundAssign(*RT, &ByteCodeExprGen::emitRem, BO);
  case BO_AndAssign:
    return visitCompoundAssign(*RT, &ByteCodeExprGen::emitAnd, BO);
  case BO_XorAssign:
    return visitCompoundAssign(*RT, &ByteCodeExprGen::emitXor, BO);
  case BO_OrAssign:
    return visitCompoundAssign(*RT, &ByteCodeExprGen::emitOr, BO);

  case BO_ShlAssign:
    if (Ctx.isOpenCL())
      return visitShiftAssign(*RT, &ByteCodeExprGen::emitShlTrunc, BO);
    return visitShiftAssign(*RT, &ByteCodeExprGen::emitShl, BO);

  case BO_ShrAssign:
    if (Ctx.isOpenCL())
      return visitShiftAssign(*RT, &ByteCodeExprGen::emitShrTrunc, BO);
    return visitShiftAssign(*RT, &ByteCodeExprGen::emitShr, BO);

  case BO_Add: {
    if (isPointer(*LT) && !isPointer(*RT))
      return visitOffset(LHS, RHS, BO, *LT, &ByteCodeExprGen::emitAddOffset);
    if (isPointer(*RT) && !isPointer(*LT))
      return visitOffset(RHS, LHS, BO, *RT, &ByteCodeExprGen::emitAddOffset);
    break;
  }
  case BO_Sub: {
    if (isPointer(*LT) && isPointer(*RT)) {
      assert(*LT == *RT && "invalid arguments to pointer difference");
      switch (*LT) {
      default:
        llvm_unreachable("invalid type");

      // Pointer difference for regular types.
      case PT_Ptr: {
        if (!visit(LHS))
          return false;
        if (!visit(RHS))
          return false;
        if (isFixedIntegral(*T)) {
          if (!this->emitPtrDiffFP(*T, getIntWidth(BO->getType()), BO))
            return false;
        } else {
          if (!this->emitPtrDiff(*T, BO))
            return false;
        }
        return DiscardResult ? this->emitPop(*T, BO) : true;
      }

      // TODO: fold to 0 when equal, otherwise undefined.
      case PT_FnPtr:
      case PT_MemPtr:
      case PT_VoidPtr:
      case PT_ObjCBlockPtr:
        return this->emitTrap(BO);
      }
    }
    if (isPointer(*LT) && !isPointer(*RT))
      return visitOffset(LHS, RHS, BO, *LT, &ByteCodeExprGen::emitSubOffset);
    break;
  }

  case BO_LOr:
  case BO_LAnd:
    return visitShortCircuit(BO);

  default:
    break;
  }

  if (!visit(LHS))
    return false;
  if (!visit(RHS))
    return false;

  auto Discard = [this, T, BO](bool Result) {
    if (!Result)
      return false;
    return DiscardResult ? this->emitPop(*T, BO) : true;
  };

  switch (BO->getOpcode()) {
  case BO_EQ: return Discard(this->emitEQ(*LT, *T, BO));
  case BO_NE: return Discard(this->emitNE(*LT, *T, BO));
  case BO_LT: return Discard(this->emitLT(*LT, *T, BO));
  case BO_LE: return Discard(this->emitLE(*LT, *T, BO));
  case BO_GT: return Discard(this->emitGT(*LT, *T, BO));
  case BO_GE: return Discard(this->emitGE(*LT, *T, BO));
  case BO_Or: return Discard(this->emitOr(*T, BO));
  case BO_And: return Discard(this->emitAnd(*T, BO));
  case BO_Xor: return Discard(this->emitXor(*T, BO));
  case BO_Rem: return Discard(this->emitRem(*T, BO));
  case BO_Div: return Discard(this->emitDiv(*T, BO));
  case BO_Sub: return Discard(this->emitSub(*T, BO));
  case BO_Add: return Discard(this->emitAdd(*T, BO));
  case BO_Mul: return Discard(this->emitMul(*T, BO));
  case BO_Shr:
    if (Ctx.isOpenCL())
      return Discard(this->emitShrTrunc(*LT, *RT, BO));
    return Discard(this->emitShr(*LT, *RT, BO));
  case BO_Shl:
    if (Ctx.isOpenCL())
      return Discard(this->emitShlTrunc(*LT, *RT, BO));
    return Discard(this->emitShl(*LT, *RT, BO));
  default:
    break;
  }
  return this->bail(BO);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryPlus(const UnaryOperator *UO) {
  return this->Visit(UO->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryMinus(const UnaryOperator *UM) {
  if (!visit(UM->getSubExpr()))
    return false;

  if (Optional<PrimType> T = classify(UM->getType())) {
    if (!this->emitMinus(*T, UM))
      return false;
    return DiscardResult ? this->emitPop(*T, UM) : true;
  }

  return this->bail(UM);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryDeref(const UnaryOperator *E) {
  return this->Visit(E->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryAddrOf(const UnaryOperator *E) {
  return this->Visit(E->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryPostInc(const UnaryOperator *E) {
  if (!visit(E->getSubExpr()))
    return false;
  PrimType T = *classify(E->getType());
  if (!this->emitPostInc(T, E))
    return false;
  return DiscardResult ? this->emitPop(T, E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryPostDec(const UnaryOperator *E) {
  if (!visit(E->getSubExpr()))
    return false;
  PrimType T = *classify(E->getType());
  if (!this->emitPostDec(T, E))
    return false;
  return DiscardResult ? this->emitPop(T, E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryPreInc(const UnaryOperator *E) {
  if (!visit(E->getSubExpr()))
    return false;
  PrimType T = *classify(E->getType());
  if (!this->emitPreInc(T, E))
    return false;
  return DiscardResult ? this->emitPopPtr(E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryPreDec(const UnaryOperator *E) {
  if (!visit(E->getSubExpr()))
    return false;
  PrimType T = *classify(E->getType());
  if (!this->emitPreDec(T, E))
    return false;
  return DiscardResult ? this->emitPopPtr(E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryNot(const UnaryOperator *UO) {
  if (!this->Visit(UO->getSubExpr()))
    return false;
  PrimType T = *classify(UO->getType());
  return DiscardResult ? true : this->emitNot(T, UO);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryLNot(const UnaryOperator *UO) {
  auto *Arg = UO->getSubExpr();
  if (!this->Visit(UO->getSubExpr()))
    return false;
  PrimType ArgT = *classify(Arg->getType());
  PrimType RetT = *classify(UO->getType());
  return DiscardResult ? true : this->emitLogicalNot(ArgT, RetT, UO);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryReal(const UnaryOperator *UO) {
  const Expr *SubExpr = UO->getSubExpr();
  if (!isa<ComplexType>(SubExpr->getType()))
    return this->Visit(SubExpr);
  if (!visit(SubExpr))
    return false;
  if (!this->emitRealElem(UO))
    return false;
  return DiscardResult ? this->emitPop(PT_Ptr, UO) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryImag(const UnaryOperator *UO) {
  const Expr *SubExpr = UO->getSubExpr();
  if (!isa<ComplexType>(SubExpr->getType()))
    return this->Visit(SubExpr);
  if (!visit(SubExpr))
    return false;
  if (!this->emitImagElem(UO))
    return false;
  return DiscardResult ? this->emitPop(PT_Ptr, UO) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryExtension(const UnaryOperator *UO) {
  return this->Visit(UO->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitConditionalOperator(
    const ConditionalOperator *CO) {
  return visitConditionalOperator(CO);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitBinaryConditionalOperator(
    const BinaryConditionalOperator *BO) {
  if (!visitOpaqueExpr(BO->getOpaqueValue()))
    return false;
  return visitConditionalOperator(BO);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitConditionalOperator(
    const AbstractConditionalOperator *CO) {
  if (!visitBool(CO->getCond()))
    return false;
  LabelTy LabelFalse = this->getLabel();
  LabelTy LabelEnd = this->getLabel();
  if (!this->jumpFalse(LabelFalse))
    return false;
  if (!this->Visit(CO->getTrueExpr()))
    return false;
  if (!this->jump(LabelEnd))
    return false;
  this->emitLabel(LabelFalse);
  if (!this->Visit(CO->getFalseExpr()))
    return false;
  if (!this->fallthrough(LabelEnd))
    return false;
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitMemberExpr(const MemberExpr *ME) {
  // Enum constants lower to an integer.
  if (auto *ECD = dyn_cast<EnumConstantDecl>(ME->getMemberDecl())) {
    QualType Ty = ME->getType();
    if (llvm::Optional<PrimType> T = Ctx.classify(Ty)) {
      if (!discard(ME->getBase()))
        return false;
      return this->emitConst(*T, getIntWidth(Ty), ECD->getInitVal(), ME);
    }
    return this->bail(ME);
  }

  // Fetch a pointer to the required field.
  if (auto *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
    return withField(FD, [this, FD, ME](const Record::Field *F) {
      const bool IsReference = FD->getType()->isReferenceType();
      if (isa<CXXThisExpr>(ME->getBase())) {
        if (IsReference) {
          if (!this->emitGetThisFieldPtr(F->getOffset(), ME))
            return false;
        } else {
          if (!this->emitGetPtrThisField(F->getOffset(), ME))
            return false;
        }
      } else {
        if (!visit(ME->getBase()))
          return false;
        if (IsReference) {
          if (!this->emitGetFieldPopPtr(F->getOffset(), ME))
            return false;
        } else {
          if (!this->emitGetPtrField(F->getOffset(), ME))
            return false;
        }
      }
      return DiscardResult ? this->emitPopPtr(ME) : true;
    });
  }

  // Emit the enum constant value of the enum field.
  if (auto *ED = dyn_cast<EnumConstantDecl>(ME->getMemberDecl()))
    return this->Visit(ED->getInitExpr());

  // Pointer to static field.
  if (auto *VD = dyn_cast<VarDecl>(ME->getMemberDecl()))
    return DiscardResult ? true : getPtrVarDecl(VD, ME);

  // Pointer to static method or method.
  if (auto *MD = dyn_cast<CXXMethodDecl>(ME->getMemberDecl())) {
    assert(MD->isStatic() && "Method is not static");
    return DiscardResult ? true : getPtrConstFn(MD, ME);
  }

  llvm_unreachable("Invalid member field");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCallExpr(const CallExpr *CE) {
  // Emit the pointer to build the return value into.
  if (InitFn) {
    if (!CE->getType()->isLiteralType(Ctx.getASTContext()))
      if (!this->emitTrapNonLiteral(CE))
        return false;
    if (!emitInitFn())
      return false;
  }

  auto Args = llvm::makeArrayRef(CE->getArgs(), CE->getNumArgs());
  auto T = classify(CE->getCallReturnType(Ctx.getASTContext()));

  // Emit the call.
  if (unsigned BuiltinOp = CE->getBuiltinCallee()) {
    switch (BuiltinOp) {
    case Builtin::BI__builtin___CFStringMakeConstantString:
    case Builtin::BI__builtin___NSStringMakeConstantString: {
      if (DiscardResult)
        return true;
      assert(Args.size() == 1 && "invalid argument count");
      auto S = cast<StringLiteral>(Args[0]->IgnoreParenCasts());
      return this->emitGetPtrGlobal(P.createGlobalString(CE, S), CE);
    }

    case Builtin::BI__builtin_constant_p: {
      assert(Args.size() == 1 && "invalid argument count");
      const Expr *Arg = Args[0];

      QualType ArgType = Arg->getType();
      if (ArgType->isIntegralOrEnumerationType() || ArgType->isFloatingType() ||
          ArgType->isAnyComplexType() || ArgType->isPointerType() ||
          ArgType->isNullPtrType()) {
        if (T) {
          switch (*T) {
            /// Integer, floating and boolean types are constants.
            case PT_Sint8:
            case PT_Uint8:
            case PT_Sint16:
            case PT_Uint16:
            case PT_Sint32:
            case PT_Uint32:
            case PT_Sint64:
            case PT_Uint64:
            case PT_SintFP:
            case PT_UintFP:
            case PT_RealFP:
            case PT_Bool:
              return discard(Arg) && this->emitConst(CE, true);
            /// A pointer is only "constant" if it is null (or a pointer cast
            /// to integer) or it points to the first character of a string
            /// literal.
            case PT_VoidPtr:
            case PT_Ptr:
              llvm_unreachable("not implemented");
            /// Special pointers are not considered to be constant.
            case PT_FnPtr:
            case PT_MemPtr:
            case PT_ObjCBlockPtr:
              return discard(Arg) && this->emitConst(CE, false);
          }
        } else {
          llvm_unreachable("not implemented");
        }
      }

      // Anything else isn't considered to be sufficiently constant.
      return this->emitConst(CE, false);
    }

    case Builtin::BI__builtin_classify_type: {
      if (DiscardResult)
        return true;
      GCCTypeClass TypeClass;
      if (Args.size() == 0) {
        TypeClass = GCCTypeClass::None;
      } else {
        QualType ArgTy = Args[0]->getType();
        TypeClass = evaluateBuiltinClassifyType(ArgTy, Ctx.getLangOpts());
      }
      return this->emitConst(CE, (int)TypeClass);
    }

    default:
      // If the callee is a builtin, lower args and invoke.
      if (!visitArguments(CE->getCallee()->getType(), Args))
        return false;
      if (!this->emitBuiltin(BuiltinOp, CE))
        return false;
      return DiscardResult && T ? this->emitPop(*T, CE) : true;
    }
  } else if (const FunctionDecl *Callee = CE->getDirectCallee()) {
    // Emit a direct call if the callee is known.
    if (isa<CXXMethodDecl>(Callee) && !Callee->isStatic()) {
      if (!visitArguments(CE->getCallee()->getType(), Args.slice(1)))
        return false;
      if (Args.size() < 1 || !visit(Args[0]))
        return false;
      return emitMethodCall(dyn_cast<CXXMethodDecl>(Callee), T, CE);
    } else {
      // Check function and then lower arguments.
      if (!checkFunctionCall(Callee, CE))
        return false;
      if (!visitArguments(CE->getCallee()->getType(), Args))
        return false;
      return emitFunctionCall(Callee, T, CE);
    }
  } else if (CE->getCallee()->getType()->isFunctionPointerType()) {
    // Function pointer call.
    if (!visitArguments(CE->getCallee()->getType(), Args))
      return false;
    if (!visit(CE->getCallee()))
      return false;
    if (!this->emitIndirectCall(CE))
      return false;
    return DiscardResult && T ? this->emitPop(*T, CE) : true;
  } else {
    // Invalid type to invoke indirectly.
    return false;
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXConstructExpr(
    const CXXConstructExpr *E) {
  CXXConstructorDecl *CD = E->getConstructor();

  // Must be invoked as an initialiser.
  assert(InitFn && "missing object to initialise");

  // Helper to invoke the constructor to initialize the pointer in InitFn.
  auto InvokeConstructor = [this, E, CD] {
    auto Args = llvm::makeArrayRef(E->getArgs(), E->getNumArgs());
    if (E->requiresZeroInitialization() && CD->isTrivial()) {
      // Do not invoke constructor, fill in with 0.
      auto *RT = E->getType()->getAs<RecordType>();
      return visitZeroInitializer(E, RT->getDecl());
    } else {
      // Avoid materialization if elidable.
      if (E->isElidable()) {
        if (auto *ME = dyn_cast<MaterializeTemporaryExpr>(E->getArg(0)))
          return this->Visit(ME->getSubExpr());
      }

      // Check callee and invoke constructor with 'this' as the pointer.
      if (!checkMethodCall(CD, E))
        return false;
      if (!visitArguments(CD->getType(), Args))
        return false;
      if (!emitInitFn())
        return false;
      return emitMethodCall(CD, {}, E);
    }
  };

  // Invoke the constructor on each array element or on the single record.
  if (auto *CAT = dyn_cast<ConstantArrayType>(E->getType())) {
    uint64_t NumElems = CAT->getSize().getZExtValue();
    for (unsigned I = 0; I < NumElems; ++I) {
      FieldScope<Emitter> Scope(this, [this, I, E](InitFnRef Base) {
        if (!Base())
          return false;
        if (!this->emitDecayPtr(E))
          return false;
        if (!this->emitConstUint32(I, E))
          return false;
        return this->emitAddOffsetUint32(E);
      });
      if (!InvokeConstructor())
        return false;
    }
    return true;
  }

  return InvokeConstructor();
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXMemberCallExpr(
    const CXXMemberCallExpr *CE) {
  // Emit the pointer to build the return value into.
  if (InitFn) {
    if (!CE->getType()->isLiteralType(Ctx.getASTContext()))
      if (!this->emitTrapNonLiteral(CE))
        return false;
    if (!emitInitFn())
      return false;
  }

  // Emit the arguments, this pointer and the call.
  auto T = classify(CE->getCallReturnType(Ctx.getASTContext()));
  auto Args = llvm::makeArrayRef(CE->getArgs(), CE->getNumArgs());

  // Identify member calls with direct targets, invoking them.
  if (auto *MD = dyn_cast_or_null<CXXMethodDecl>(CE->getDirectCallee())) {
    if (!visitArguments(MD->getType(), Args))
      return false;

    // Lower the 'this' pointer.
    if (!visit(CE->getImplicitObjectArgument()))
      return false;

    // Direct member call.
    if (!MD->isVirtual()) {
      return emitMethodCall(MD, T, CE);
    } else {
      // Indirect virtual call.
      if (!this->emitVirtualInvoke(MD, CE))
        return false;
      return DiscardResult && T ? this->emitPop(*T, CE) : true;
    }
  }

  // Pattern match the callee for indirect calls to avoid materializing
  // the bound member function pointer on the stack.
  const Expr *Callee = CE->getCallee();
  if (Callee->getType()->isSpecificBuiltinType(BuiltinType::BoundMember)) {
    auto *BO = dyn_cast<BinaryOperator>(Callee->IgnoreParens());
    switch (BO->getOpcode()) {
    case BO_PtrMemD:
    case BO_PtrMemI:
      if (!visitArguments(BO->getRHS()->getType(), Args))
        return false;
      // Emit 'this' pointer.
      if (!visit(BO->getLHS()))
        return false;
      // Emit member function.
      if (!visit(BO->getRHS()))
        return false;
      // Emit invocation.
      if (!this->emitIndirectInvoke(CE))
        return false;
      return DiscardResult && T ? this->emitPop(*T, CE) : true;
    default:
      llvm_unreachable("invalid indirect callee");
    }
  }

  llvm_unreachable("invalid member call expression callee");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::checkFunctionCall(const FunctionDecl *FD,
                                                 const Expr *E) {
  if (FD->isConstexpr() && FD->getDefinition())
    return true;
  return this->emitCheckFunction(FD, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitFunctionCall(const FunctionDecl *Callee,
                                                Optional<PrimType> T,
                                                const Expr *E) {
  if (Expected<Function *> Func = P.getOrCreateFunction(S, Callee)) {
    if (*Func) {
      if (!this->emitCall(*Func, E))
        return false;
    } else {
      if (!this->emitNoCall(Callee, E))
        return false;
    }
  } else {
    consumeError(Func.takeError());
    return this->bail(E);
  }
  return DiscardResult && T ? this->emitPop(*T, E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::checkMethodCall(const CXXMethodDecl *MD,
                                               const Expr *E) {
  bool CanInvoke = !MD->isVirtual() || Ctx.isCPlusPlus2a();
  if (MD->isConstexpr() && MD->getDefinition() && CanInvoke)
    return true;
  return this->emitCheckMethod(MD, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitMethodCall(const CXXMethodDecl *Callee,
                                              Optional<PrimType> T,
                                              const Expr *E) {
  if (Expected<Function *> Func = P.getOrCreateFunction(S, Callee)) {
    if (*Func) {
      if (!this->emitInvoke(*Func, E))
        return false;
    } else {
      if (!this->emitNoInvoke(Callee, E))
        return false;
    }
  } else {
    consumeError(Func.takeError());
    return this->bail(E);
  }
  return DiscardResult && T ? this->emitPop(*T, E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitArraySubscriptExpr(
    const ArraySubscriptExpr *E) {
  const Expr *Base = E->getBase();
  const Expr *Idx = E->getIdx();

  if (Base->getType()->isVoidPointerType())
    return this->bail(E);

  if (!visit(Base))
    return false;
  if (!visit(Idx))
    return false;
  if (!this->emitAddOffset(*classify(Idx->getType()), E))
    return false;
  return DiscardResult ? this->emitPopPtr(E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitExprWithCleanups(
    const ExprWithCleanups *CE) {
  return this->Visit(CE->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCompoundLiteralExpr(
    const CompoundLiteralExpr *E) {
  return materialize(E, E->getInitializer(),
                     /*isConst=*/E->getType().isConstQualified(),
                     /*isGlobal=*/E->isFileScope(),
                     /*isExtended=*/false);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitMaterializeTemporaryExpr(
    const MaterializeTemporaryExpr *ME) {
  const Expr *SubExpr = ME->getSubExpr();

  // Walk through the expression to find the materialized temporary itself.
  SmallVector<const Expr *, 2> Ignored;
  while (true) {
    if (auto *BO = dyn_cast<BinaryOperator>(SubExpr)) {
      if (BO->getOpcode() == BO_Comma) {
        Ignored.push_back(BO->getLHS());
        SubExpr = BO->getRHS();
        continue;
      }
    }
    break;
  }

  // Evaluate, but ignore all values on the LHS of commas.
  for (const Expr *E : Ignored) {
    if (!discard(E))
      return false;
  }

  const bool IsConst = ME->getType().isConstQualified();
  const bool IsGlobal = ME->getStorageDuration() == SD_Static;
  const bool IsExtended = ME->getStorageDuration() == SD_Automatic;
  return materialize(ME, SubExpr, IsConst, IsGlobal, IsExtended);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::materialize(const Expr *Alloc, const Expr *Init,
                                           bool IsConst, bool IsGlobal,
                                           bool IsExtended) {
  QualType Ty = Init->getType();
  if (IsGlobal) {
    if (auto I = P.createGlobal(Alloc, Ty)) {
      auto *MTE = dyn_cast<MaterializeTemporaryExpr>(Alloc);
      if (Optional<PrimType> T = classify(Init->getType())) {
        // Primitive global - compute and set.
        if (!visit(Init))
          return false;

        // The APValue storage must be initialised.
        if (MTE && IsGlobal) {
          if (!this->emitInitTemp(*T, MTE, *I, Alloc))
            return false;
        }

        // Set the global.
        if (!this->emitInitGlobal(*T, *I, Alloc))
          return false;

        // Return a pointer to the block.
        return DiscardResult ? true : this->emitGetPtrGlobal(*I, Alloc);
      } else {
        // Composite global - initialize in place.
        if (!visitGlobalInitializer(Init, *I))
          return false;

        // The APValue storage must be initialised.
        if (MTE && IsGlobal) {
          if (!this->emitGetPtrGlobal(*I, Alloc))
            return false;
          if (!this->emitInitTempValue(MTE, *I, Init))
            return false;
          return DiscardResult ? this->emitPopPtr(Alloc) : true;
        }

        return DiscardResult ? true : this->emitGetPtrGlobal(*I, Alloc);
      }
    }
  } else {
    if (Optional<PrimType> T = classify(Ty)) {
      auto I = allocateLocalPrimitive(Alloc, Ty, *T, IsConst, IsExtended);
      if (!visit(Init))
        return false;
      if (!this->emitSetLocal(*T, I, Init))
        return false;
      return DiscardResult ? true : this->emitGetPtrLocal(I, Init);
    } else {
      // Composite types - allocate storage and initialize it.
      // This operation leaves a pointer to the temporary on the stack.
      if (auto I = allocateLocal(Alloc, Ty, IsExtended)) {
        if (!visitLocalInitializer(Init, *I))
          return false;
        return DiscardResult ? true : this->emitGetPtrLocal(*I, Alloc);
      }
    }
  }
  return this->bail(Alloc);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitImplicitValueInitExpr(
    const ImplicitValueInitExpr *E) {

  QualType Ty = E->getType();
  if (auto *AT = Ty->getAs<AtomicType>())
    Ty = AT->getValueType();

  if (auto T = Ctx.classify(Ty))
    return DiscardResult ? true : visitZeroInitializer(*T, E);

  if (auto *RT = Ty->getAs<RecordType>())
    return visitZeroInitializer(E, RT->getDecl());

  if (auto *CAT = dyn_cast<ConstantArrayType>(Ty)) {
    ImplicitValueInitExpr E(CAT->getElementType());
    return visitArrayInitializer(CAT, &E, [&E](unsigned) { return &E; });
  }

  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitSubstNonTypeTemplateParmExpr(
    const SubstNonTypeTemplateParmExpr *E) {
  return this->Visit(E->getReplacement());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitUnaryExprOrTypeTraitExpr(
    const UnaryExprOrTypeTraitExpr *TE) {
  UnaryExprOrTypeTrait Kind = TE->getKind();
  switch (Kind) {
  case UETT_PreferredAlignOf:
  case UETT_AlignOf: {
    CharUnits Align;
    if (TE->isArgumentType())
      Align = getAlignOfType(Ctx.getASTContext(), TE->getArgumentType(), Kind);
    else
      Align = getAlignOfExpr(Ctx.getASTContext(), TE->getArgumentExpr(), Kind);
    return DiscardResult ? true : emitConst(TE, Align.getQuantity());
  }

  case UETT_VecStep: {
    QualType Ty = TE->getTypeOfArgument();

    if (Ty->isVectorType()) {
      unsigned n = Ty->castAs<VectorType>()->getNumElements();
      // The vec_step built-in functions that take a 3-component
      // vector return 4. (OpenCL 1.1 spec 6.11.12)
      return this->emitConst(TE, n == 3 ? 4 : n);
    } else {
      return this->emitConst(TE, 1);
    }
  }

  case UETT_SizeOf: {
    QualType ArgTy = TE->getTypeOfArgument();

    // C++ [expr.sizeof]p2: "When applied to a reference or a reference type,
    //   the result is the size of the referenced type."
    if (auto *Ref = ArgTy->getAs<ReferenceType>())
      ArgTy = Ref->getPointeeType();

    CharUnits Size;
    if (ArgTy->isVoidType() || ArgTy->isFunctionType()) {
      Size = CharUnits::One();
    } else if (ArgTy->isDependentType()) {
      return this->emitTrap(TE);
    } else if (!ArgTy->isConstantSizeType()) {
      // sizeof(vla) is not a constantexpr: C99 6.5.3.4p2.
      // FIXME: Better diagnostic.
      return this->emitTrap(TE);
    } else {
      Size = Ctx.getASTContext().getTypeSizeInChars(ArgTy);
    }

    // Emit the size as an integer.
    return DiscardResult ? true : emitConst(TE, Size.getQuantity());
  }
  case UETT_OpenMPRequiredSimdAlign: {
    return this->bail(TE);
  }
  }

  llvm_unreachable("unknown expr/type trait");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitChooseExpr(const ChooseExpr *E) {
  return this->Visit(E->getChosenSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitGenericSelectionExpr(
    const GenericSelectionExpr *E) {
  return this->Visit(E->getResultExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitOffsetOfExpr(const OffsetOfExpr *OOE) {
  unsigned N = OOE->getNumComponents();
  if (N == 0)
    return this->emitTrap(OOE);

  const ASTContext &ASTCtx = Ctx.getASTContext();

  CharUnits Result;
  QualType CurrentType = OOE->getTypeSourceInfo()->getType();
  for (unsigned I = 0; I != N; ++I) {
    OffsetOfNode ON = OOE->getComponent(I);
    switch (ON.getKind()) {
    case OffsetOfNode::Array: {
      const Expr *Idx = OOE->getIndexExpr(ON.getArrayExprIndex());
      APValue IdxResult;
      if (!Ctx.evaluateAsRValue(S, Idx, IdxResult))
        return false;
      const ArrayType *AT = ASTCtx.getAsArrayType(CurrentType);
      if (!AT)
        return this->emitTrap(OOE);
      CurrentType = AT->getElementType();
      CharUnits ElementSize = ASTCtx.getTypeSizeInChars(CurrentType);
      Result += IdxResult.getInt().getSExtValue() * ElementSize;
      break;
    }

    case OffsetOfNode::Field: {
      FieldDecl *MemberDecl = ON.getField();
      const RecordType *RT = CurrentType->getAs<RecordType>();
      if (!RT)
        return this->emitTrap(OOE);
      RecordDecl *RD = RT->getDecl();
      if (RD->isInvalidDecl())
        return false;
      const ASTRecordLayout &RL = ASTCtx.getASTRecordLayout(RD);
      unsigned FI = MemberDecl->getFieldIndex();
      assert(FI < RL.getFieldCount() && "offsetof field in wrong type");
      Result += ASTCtx.toCharUnitsFromBits(RL.getFieldOffset(FI));
      CurrentType = MemberDecl->getType().getNonReferenceType();
      break;
    }

    case OffsetOfNode::Identifier:
      llvm_unreachable("dependent __builtin_offsetof");

    case OffsetOfNode::Base: {
      CXXBaseSpecifier *BaseSpec = ON.getBase();
      if (BaseSpec->isVirtual())
        return this->emitTrap(OOE);

      // Find the layout of the class whose base we are looking into.
      const RecordType *RT = CurrentType->getAs<RecordType>();
      if (!RT)
        return this->emitTrap(OOE);
      RecordDecl *RD = RT->getDecl();
      if (RD->isInvalidDecl())
        return false;
      const ASTRecordLayout &RL = ASTCtx.getASTRecordLayout(RD);

      // Find the base class itself.
      CurrentType = BaseSpec->getType();
      const RecordType *BaseRT = CurrentType->getAs<RecordType>();
      if (!BaseRT)
        return this->emitTrap(OOE);

      // Add the offset to the base.
      Result += RL.getBaseClassOffset(cast<CXXRecordDecl>(BaseRT->getDecl()));
      break;
    }
    }
  }

  return this->emitConst(OOE, Result.getQuantity());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitSizeOfPackExpr(const SizeOfPackExpr *E) {
  if (DiscardResult)
    return true;
  return emitConst(E, E->getPackLength());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitOpaqueValueExpr(const OpaqueValueExpr *E) {
  auto It = OpaqueExprs.find(E);
  if (It != OpaqueExprs.end()) {
    // The opaque expression was evaluated earlier and the pointer was cached in
    // a local variable. Load the variable from the pointer at this point.
    PrimType Ty;
    if (Optional<PrimType> T = classify(E->getType())) {
      Ty = *T;
    } else {
      Ty = PT_Ptr;
    }
    return DiscardResult ? true : this->emitGetLocal(Ty, It->second, E);
  } else {
    // Compile the source expression.
    const Expr *Source = E->getSourceExpr();
    if (!Source || Source == E)
      return this->emitTrap(E);
    return this->Visit(Source);
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitArrayInitLoopExpr(
    const ArrayInitLoopExpr *E) {
  // Evaluate the common expression, a pointer to the array copied from.
  if (auto *C = E->getCommonExpr()) {
    if (!visitOpaqueExpr(C))
      return false;
  }

  // Initialise each element of the array. In this scope, ArrayIndex is set to
  // refer to the index of the element being initialised in the callback which
  // returns the initialiser of that element.
  auto OldArrayIndex = ArrayIndex;

  auto ElemInit = [this, E](uint64_t I) {
    ArrayIndex = I;
    return E->getSubExpr();
  };

  auto *AT = E->getType()->getAsArrayTypeUnsafe();
  if (!visitArrayInitializer(cast<ConstantArrayType>(AT), E, ElemInit))
    return false;

  ArrayIndex = OldArrayIndex;
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitArrayInitIndexExpr(
    const ArrayInitIndexExpr *E) {
  assert(!DiscardResult && "ArrayInitIndexExpr should not be discarded");
  return ArrayIndex ? this->emitConst(E, *ArrayIndex) : this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitInitListExpr(const InitListExpr *E) {
  // Initialise a scalar with a given value or zero.
  if (auto T = Ctx.classify(E->getType())) {
    if (E->getNumInits() == 0)
      return DiscardResult ? true : visitZeroInitializer(*T, E);
    if (E->getNumInits() == 1)
      return this->Visit(E->getInit(0));
    return false;
  }

  // Desugar the type and decide which initializer to call based on it.
  QualType Ty = E->getType();
  if (auto *AT = dyn_cast<AtomicType>(Ty))
    Ty = AT->getValueType();

  if (auto *RT = Ty->getAs<RecordType>()) {
    if (E->isTransparent())
      return this->Visit(E->getInit(0));
    return visitRecordInitializer(RT, E);
  }

  if (auto *CT = Ty->getAs<ComplexType>())
    return visitComplexInitializer(CT, E);

  if (auto *VT = Ty->getAs<VectorType>())
    return visitVectorInitializer(VT, E);

  if (E->isStringLiteralInit()) {
    auto *S = cast<StringLiteral>(E->getInit(0)->IgnoreParens());
    return visitStringInitializer(S);
  }

  if (auto CT = Ty->getAsArrayTypeUnsafe()) {
    if (auto CAT = dyn_cast<ConstantArrayType>(CT)) {
      return visitArrayInitializer(CAT, E, [E](uint64_t I) {
        if (I < E->getNumInits())
          return E->getInit(I);
        else
          return E->getArrayFiller();
      });
    }
  }

  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitTypeTraitExpr(const TypeTraitExpr *E) {
  return DiscardResult ? true : this->emitConstBool(E->getValue(), E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitBlockExpr(const BlockExpr *E) {
  if (E->getBlockDecl()->hasCaptures())
    return this->emitTrap(E);
  return this->emitConstObjCBlock(E, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitGNUNullExpr(const GNUNullExpr *E) {
  if (llvm::Optional<PrimType> T = classify(E->getType()))
    return DiscardResult ? true : visitZeroInitializer(*T, E);
  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitLambdaExpr(const LambdaExpr *E) {
  const CXXRecordDecl *ClosureClass = E->getLambdaClass();
  if (ClosureClass->isInvalidDecl())
    return false;
  Record *R = P.getOrCreateRecord(ClosureClass);
  if (!R)
    return false;

  // Initialize all the captured field in the record.
  auto *CaptureInitIt = E->capture_init_begin();
  for (const auto *Field : ClosureClass->fields()) {
    assert(CaptureInitIt != E->capture_init_end());
    // Get the initializer for this field. If there is no initializer, either
    // this is a VLA or an error has occurred.
    Expr *const CurFieldInit = *CaptureInitIt++;
    if (!CurFieldInit)
      return false;
    if (!visitRecordFieldInitializer(R, R->getField(Field), CurFieldInit))
      return false;
  }
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitSourceLocExpr(const SourceLocExpr *E) {
  SourceLocation Loc;
  const DeclContext *Context;
  const Expr *DefaultExpr = SourceLocScope.getDefaultExpr();
  if (auto *DIE = dyn_cast_or_null<CXXDefaultInitExpr>(DefaultExpr)) {
    Loc = DIE->getUsedLocation();
    Context = DIE->getUsedContext();
  } else if (auto *DAE = dyn_cast_or_null<CXXDefaultArgExpr>(DefaultExpr)) {
    Loc = DAE->getUsedLocation();
    Context = DAE->getUsedContext();
  } else {
    Loc = E->getLocation();
    Context = E->getParentContext();
  }

  const auto &SrcMngr = Ctx.getASTContext().getSourceManager();
  auto L = SrcMngr.getPresumedLoc(SrcMngr.getExpansionRange(Loc).getEnd());

  switch (E->getIdentKind()) {
  case SourceLocExpr::File:
    if (!this->emitGetPtrGlobal(P.createGlobalString(L.getFilename()), E))
      return false;
    return this->emitDecayPtr(E);
  case SourceLocExpr::Function: {
    std::string Name;
    if (const Decl *CurDecl = dyn_cast_or_null<Decl>(Context))
      Name = PredefinedExpr::ComputeName(PredefinedExpr::Function, CurDecl);
    if (!this->emitGetPtrGlobal(P.createGlobalString(Name), E))
      return false;
    return this->emitDecayPtr(E);
  }
  case SourceLocExpr::Line:
    return this->emitConst(E, L.getLine());
  case SourceLocExpr::Column:
    return this->emitConst(E, L.getColumn());
  }
  llvm_unreachable("invalid expression kind");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitAddrLabelExpr(const AddrLabelExpr *E) {
  return this->emitConstAddrLabel(E, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitConceptSpecializationExpr(
    const ConceptSpecializationExpr *E) {
  return this->emitConst(E, E->isSatisfied());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXNullPtrLiteralExpr(
    const CXXNullPtrLiteralExpr *E) {
  return DiscardResult ? true : this->emitNullPtr(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXBoolLiteralExpr(
    const CXXBoolLiteralExpr *B) {
  return DiscardResult ? true : this->emitConstBool(B->getValue(), B);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXThisExpr(const CXXThisExpr *TE) {
  if (CaptureThis) {
    return withField(*CaptureThis, [this, TE](const Record::Field *F) {
      if (!this->emitGetThisFieldPtr(F->getOffset(), TE))
        return false;
      return DiscardResult ? this->emitPopPtr(TE) : true;
    });
  } else {
    if (!this->emitThis(TE))
      return false;
    return DiscardResult ? this->emitPopPtr(TE) : true;
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXDefaultArgExpr(
    const CXXDefaultArgExpr *DE) {
  CurrentSourceLocExprScope::SourceLocExprScopeGuard Guard(DE, SourceLocScope);
  return this->Visit(DE->getExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXDefaultInitExpr(
    const CXXDefaultInitExpr *DE) {
  if (!DE->getExpr())
    return this->emitTrap(DE);
  CurrentSourceLocExprScope::SourceLocExprScopeGuard Guard(DE, SourceLocScope);
  return this->Visit(DE->getExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXScalarValueInitExpr(
    const CXXScalarValueInitExpr *E) {
  QualType Ty = E->getType();
  if (auto T = Ctx.classify(Ty))
    return DiscardResult ? true : visitZeroInitializer(*T, E);

  // Void types are not lowered to any value.
  if (Ty->isVoidType())
    return true;
  // Complex numbers are not really scalar.
  if (auto *CT = Ty->getAs<ComplexType>()) {
    QualType ElemType = CT->getElementType();
    PrimType T = classifyPrim(ElemType);
    ImplicitValueInitExpr ZE(ElemType);
    if (!emitInitFn())
      return false;
    if (!visit(&ZE))
      return false;
    if (!this->emitInitElem(T, 0, E))
      return false;
    if (!visit(&ZE))
      return false;
    return this->emitInitElemPop(T, 1, E);
  }
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXThrowExpr(const CXXThrowExpr *E) {
  return this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXTemporaryObjectExpr(
    const CXXTemporaryObjectExpr *E) {
  // In-place construction, simply invoke constructor.
  if (InitFn)
    return VisitCXXConstructExpr(cast<CXXConstructExpr>(E));

  // Allocate a temporary and build into it.
  if (auto I = allocateLocal(E, E->getType(), /*isExtended=*/false)) {
    OptionScope<Emitter> Scope(this, InitFnRef{[this, &I, E] {
      return this->emitGetPtrLocal(*I, E);
    }});
    if (!VisitCXXConstructExpr(cast<CXXConstructExpr>(E)))
      return false;
    return DiscardResult ? true : this->emitGetPtrLocal(*I, E);
  }
  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXTypeidExpr(const CXXTypeidExpr *E) {
  assert(!InitFn && "typeid cannot be constructed in place");
  const Type *InfoTy = E->getType().getTypePtr();
  if (!E->isPotentiallyEvaluated()) {
    const Type *Ty;
    if (E->isTypeOperand())
      Ty = E->getTypeOperand(Ctx.getASTContext()).getTypePtr();
    else
      Ty = E->getExprOperand()->getType().getTypePtr();
    return DiscardResult ? true : this->emitTypeInfo(Ty, InfoTy, E);
  } else {
    if (!Ctx.isCPlusPlus2a() && !this->emitWarnPolyTypeid(E, E))
      return false;
    if (!visit(E->getExprOperand()))
      return false;
    if (!this->emitTypeInfoPoly(InfoTy, E))
      return false;
    return DiscardResult ? this->emitPopPtr(E) : true;
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXReinterpretCastExpr(
    const CXXReinterpretCastExpr *E) {
  if (!this->emitWarnReinterpretCast(E))
    return false;
  return VisitCastExpr(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXDynamicCastExpr(
    const CXXDynamicCastExpr *E) {
  if (!Ctx.isCPlusPlus2a()) {
    if (!this->emitWarnDynamicCast(E))
      return false;
  }
  return VisitCastExpr(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXInheritedCtorInitExpr(
    const CXXInheritedCtorInitExpr *E) {
  auto *Ctor = E->getConstructor();

  // Forward all parameters.
  unsigned Offset = 0;
  for (auto *PD : Ctor->parameters()) {
    PrimType Ty;
    if (Optional<PrimType> T = classify(PD->getType())) {
      Ty = *T;
    } else {
      Ty = PT_Ptr;
    }
    if (!this->emitGetParam(Ty, Offset, E))
      return false;
    Offset += align(primSize(Ty));
  }

  // Invoke the constructor on the same 'this'.
  return emitInitFn() && emitMethodCall(Ctor, {}, E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXStdInitializerListExpr(
    const CXXStdInitializerListExpr *E) {
  auto *AT = getAsConstantArrayType(E->getSubExpr()->getType());

  /// Push the pointer to the std::initializer_list.
  if (!this->emitInitFn())
    return false;


  Record *R = getRecord(E->getType());

  // std::initializer_list should not have base classes.
  if (!R || R->getNumBases() != 0 || R->getNumVirtualBases() != 0)
    return this->emitTrap(E);

  // pointer + pointer or pointer + length
  if (R->getNumFields() != 2)
    return this->emitTrap(E);

  // The first field must be a pointer.
  Record::Field *Fst = R->getField(0);
  if (Fst->getDesc()->ElemTy != PT_Ptr)
    return this->emitTrap(E);

  // Set the first field - either a pointer or an integer.
  Record::Field *Snd = R->getField(1);
  if (Optional<PrimType> T = Snd->getDesc()->ElemTy) {
    QualType Ty = Snd->getDecl()->getType();
    switch (*T) {
      case PT_Sint8:
      case PT_Uint8:
      case PT_Sint16:
      case PT_Uint16:
      case PT_Sint32:
      case PT_Uint32:
      case PT_Sint64:
      case PT_Uint64:
      case PT_SintFP:
      case PT_UintFP:
      case PT_Bool:
        /// Lower the array.
        if (!visit(E->getSubExpr()))
          return false;
        if (!this->emitDecayPtr(E))
          return false;
        // Set the first field.
        if (!this->emitInitFieldPtr(Fst->getOffset(), E))
          return false;
        // Set the length.
        if (!this->emitConst(*T, getIntWidth(Ty), AT->getSize(), E))
          return false;
        if (DiscardResult)
          return this->emitInitFieldPop(*T, Snd->getOffset(), E);
        else
          return this->emitInitField(*T, Snd->getOffset(), E);
      case PT_Ptr:
        /// Lower the array.
        if (!visit(E->getSubExpr()))
          return false;
        if (!this->emitDecayPtr(E))
          return false;
        if (!this->emitInitFieldPeekPtr(Fst->getOffset(), E))
          return false;
        // Set the end pointer.
        if (!this->emitConstUint64(AT->getSize().getZExtValue(), E))
          return false;
        if (!this->emitAddOffsetUint64(E))
          return false;
        if (DiscardResult)
          return this->emitInitFieldPopPtr(Snd->getOffset(), E);
        else
          return this->emitInitFieldPtr(Snd->getOffset(), E);
      default:
        return this->emitTrap(E);
    }
  }
  return this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXNewExpr(const CXXNewExpr *E) {
  return this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXNoexceptExpr(const CXXNoexceptExpr *E) {
  return this->emitConst(E, E->getValue());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitCXXBindTemporaryExpr(
    const CXXBindTemporaryExpr *E) {
  return this->Visit(E->getSubExpr());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitObjCBoolLiteralExpr(
    const ObjCBoolLiteralExpr *E) {
  return this->emitConst(E, E->getValue());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitObjCStringLiteral(
    const ObjCStringLiteral *E) {
  assert(!InitFn && "cannot construct ObjCStringLiteral in place");
  if (DiscardResult)
    return true;
  return this->emitGetPtrGlobal(P.createGlobalString(E, E->getString()), E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitObjCEncodeExpr(const ObjCEncodeExpr *E) {
  const ASTContext &ASTCtx = Ctx.getASTContext();

  // Get the string representing the type.
  std::string Str;
  ASTCtx.getObjCEncodingForType(E->getEncodedType(), Str);

  if (InitFn) {
    // Initialise the string in place.
    if (!emitInitFn())
      return false;

    // Initialise elements one by one, advancing the pointer.
    for (unsigned I = 0, N = Str.size(); I < N; ++I) {
      // Lower the code point.
      if (!emitConst(PT_Sint8, 8, APInt(8, Str[I]), E))
        return false;
      // Set the character.
      if (!this->emitInitElem(PT_Sint8, I, E))
        return false;
    }

    // Set the null terminator.
    if (!emitConst(PT_Sint8, 8, APInt(8, 0), E))
      return false;
    if (!this->emitInitElemPop(PT_Sint8, Str.length(), E))
      return false;

    // String was initialised.
    return true;
  }

  // Create a global string and return a pointer to it.
  if (DiscardResult)
    return true;
  return this->emitGetPtrGlobal(P.createGlobalString(Str), E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitObjCBoxedExpr(const ObjCBoxedExpr *E) {
  if (E->isExpressibleAsConstantInitializer()) {
    assert(!InitFn && "cannot construct ObjCBoxedExpr in place");
    if (DiscardResult)
      return true;
    auto *SL = cast<StringLiteral>(E->getSubExpr()->IgnoreParenCasts());
    return this->emitGetPtrGlobal(P.createGlobalString(E, SL), E);
  }
  return this->emitNote(E) && discard(E->getSubExpr()) && this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::VisitObjCIvarRefExpr(const ObjCIvarRefExpr *E) {
  return this->emitTrap(E);
}

template <class Emitter>
llvm::Optional<PrimType>
ByteCodeExprGen<Emitter>::classify(const Expr *E) const {
  QualType T = E->getType();

  // Function pointers.
  if (T->isFunctionReferenceType())
    return PT_FnPtr;
  if (T->isFunctionPointerType())
    return PT_FnPtr;
  if (T->isFunctionProtoType())
    return PT_FnPtr;
  if (T->isFunctionType())
    return PT_FnPtr;
  // Block pointers.
  if (T->isBlockPointerType())
    return PT_ObjCBlockPtr;
  // Member pointer type check.
  if (T->isMemberPointerType())
    return PT_MemPtr;
  // Void pointers.
  if (T->isVoidPointerType())
    return PT_VoidPtr;

  return E->isGLValue() ? PT_Ptr : classify(E->getType());
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::discard(const Expr *E) {
  QualType Ty = E->getType();
  if (Ty->isVoidType() || !E->isRValue() || classify(Ty)) {
    OptionScope<Emitter> Scope(this, /*discardResult=*/true);
    return this->Visit(E);
  }
  if (auto I = allocateLocal(E, E->getType(), /*isExtended=*/false)) {
    return visitLocalInitializer(E, *I);
  }
  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visit(const Expr *E) {
  if (!E->isRValue() || classify(E->getType())) {
    OptionScope<Emitter> Scope(this, /*discardResult=*/false);
    return this->Visit(E);
  }
  if (auto I = allocateLocal(E, E->getType(), /*isExtended=*/false)) {
    if (!visitLocalInitializer(E, *I))
      return false;
    return this->emitGetPtrLocal(*I, E);
  }
  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitBool(const Expr *E) {
  if (Optional<PrimType> T = classify(E->getType())) {
    if (!visit(E))
      return false;
    return (*T != PT_Bool) ? this->emitTest(*T, PT_Bool, E) : true;
  } else {
    return this->bail(E);
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitZeroInitializer(PrimType T, const Expr *E) {
  switch (T) {
  case PT_Bool:
    return this->emitZeroBool(E);
  case PT_Sint8:
    return this->emitZeroSint8(E);
  case PT_Uint8:
    return this->emitZeroUint8(E);
  case PT_Sint16:
    return this->emitZeroSint16(E);
  case PT_Uint16:
    return this->emitZeroUint16(E);
  case PT_Sint32:
    return this->emitZeroSint32(E);
  case PT_Uint32:
    return this->emitZeroUint32(E);
  case PT_Sint64:
    return this->emitZeroSint64(E);
  case PT_Uint64:
    return this->emitZeroUint64(E);
  case PT_UintFP:
    return this->emitZeroFPUintFP(getIntWidth(E->getType()), E);
  case PT_SintFP:
    return this->emitZeroFPSintFP(getIntWidth(E->getType()), E);
  case PT_RealFP:
    return this->emitZeroRealFP(getFltSemantics(E->getType()), E);
  case PT_FnPtr:
    return this->emitNullFnPtr(E);
  case PT_MemPtr:
    return this->emitNullMemPtr(E);
  case PT_VoidPtr:
    return this->emitNullVoidPtr(E);
  case PT_ObjCBlockPtr:
    return this->emitNullObjCBlockPtr(E);
  case PT_Ptr: {
    if (auto *PtrTy = E->getType()->getAs<PointerType>()) {
      QualType Pointee = PtrTy->getPointeeType();
      return this->emitNullPtrT(P.createDescriptor(nullptr, Pointee), E);
    }
    return this->emitNullPtr(E);
  }
  }
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitOffset(const Expr *Ptr, const Expr *Offset,
                                           const Expr *E, PrimType PT,
                                           UnaryFn OffsetFn) {
  if (Ptr->getType()->isVoidPointerType())
    return this->bail(E);

  if (!visit(Ptr))
    return false;
  if (!visit(Offset))
    return false;
  if (!(this->*OffsetFn)(*classify(Offset->getType()), E))
    return false;
  return DiscardResult ? this->emitPopPtr(E) : true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitIndirectMember(const BinaryOperator *E) {
  if (!visit(E->getLHS()))
    return false;
  if (!visit(E->getRHS()))
    return false;

  if (!this->emitGetPtrFieldIndirect(E))
    return false;
  if (DiscardResult && !this->emitPopPtr(E))
    return false;
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitSpaceship(const BinaryOperator *E) {
  if (!emitInitFn())
    return false;
  if (!DiscardResult)
    return this->emitDupPtr(E);
  if (!visit(E->getLHS()))
    return false;
  if (!visit(E->getRHS()))
    return false;

  auto ArgTy = E->getLHS()->getType();
  if (Optional<PrimType> T = classify(ArgTy))
    return this->emitSpaceship(*T, E);
  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitAssign(PrimType T,
                                           const BinaryOperator *BO) {
  return dereference(
      BO->getLHS(), DerefKind::Write,
      [this, BO](PrimType) {
        // Generate a value to store - will be set.
        return visit(BO->getRHS());
      },
      [this, BO](PrimType T) {
        // Pointer on stack - compile RHS and assign to pointer.
        if (!visit(BO->getRHS()))
          return false;

        if (BO->getLHS()->refersToBitField()) {
          if (DiscardResult)
            return this->emitStoreBitFieldPop(T, BO);
          else
            return this->emitStoreBitField(T, BO);
        } else {
          if (DiscardResult)
            return this->emitStorePop(T, BO);
          else
            return this->emitStore(T, BO);
        }
      });
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitShiftAssign(PrimType RHS, BinaryFn F,
                                                const BinaryOperator *BO) {
  return dereference(
      BO->getLHS(), DerefKind::ReadWrite,
      [this, F, RHS, BO](PrimType LHS) {
        if (!visit(BO->getRHS()))
          return false;
        (this->*F)(LHS, RHS, BO);
        return true;
      },
      [this, F, RHS, BO](PrimType LHS) {
        if (!this->emitLoad(LHS, BO))
          return false;
        if (!visit(BO->getRHS()))
          return false;
        if (!(this->*F)(LHS, RHS, BO))
          return false;

        if (BO->getLHS()->refersToBitField()) {
          if (DiscardResult)
            return this->emitStoreBitFieldPop(LHS, BO);
          else
            return this->emitStoreBitField(LHS, BO);
        } else {
          if (DiscardResult)
            return this->emitStorePop(LHS, BO);
          else
            return this->emitStore(LHS, BO);
        }
      });
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitCompoundAssign(PrimType RHS, UnaryFn F,
                                                   const BinaryOperator *BO) {
  auto ApplyOperation = [this, F, BO, RHS](PrimType LHS) {
    auto *ExprRHS = BO->getRHS();
    auto *ExprLHS = BO->getLHS();
    if (ExprLHS->getType() == ExprRHS->getType()) {
      if (!visit(BO->getRHS()))
        return false;
      if (!(this->*F)(LHS, BO))
        return false;
    } else {
      if (!emitConv(LHS, ExprLHS->getType(), RHS, ExprRHS->getType(), ExprRHS))
        return false;
      if (!visit(ExprRHS))
        return false;
      if (!(this->*F)(RHS, BO))
        return false;
      if (!emitConv(RHS, ExprRHS->getType(), LHS, ExprLHS->getType(), ExprLHS))
        return false;
    }
    return true;
  };

  return dereference(
      BO->getLHS(), DerefKind::ReadWrite, ApplyOperation,
      [this, BO, &ApplyOperation](PrimType LHS) -> bool {
        if (!this->emitLoad(LHS, BO))
          return false;
        if (!ApplyOperation(LHS))
          return false;

        if (BO->getLHS()->refersToBitField()) {
          if (DiscardResult)
            return this->emitStoreBitFieldPop(LHS, BO);
          else
            return this->emitStoreBitField(LHS, BO);
        } else {
          if (DiscardResult)
            return this->emitStorePop(LHS, BO);
          else
            return this->emitStore(LHS, BO);
        }
      });
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitPtrAssign(PrimType RHS, UnaryFn F,
                                              const BinaryOperator *BO) {
  return dereference(
      BO->getLHS(), DerefKind::ReadWrite,
      [this, F, BO, RHS](PrimType LHS) {
        if (!visit(BO->getRHS()))
          return false;
        if (!(this->*F)(RHS, BO))
          return false;
        return true;
      },
      [this, BO, F, RHS](PrimType LHS) -> bool {
        if (!this->emitLoad(LHS, BO))
          return false;
        if (!visit(BO->getRHS()))
          return false;
        if (!(this->*F)(RHS, BO))
          return false;

        if (BO->getLHS()->refersToBitField())
          return this->bail(BO);

        if (DiscardResult)
          return this->emitStorePop(LHS, BO);
        else
          return this->emitStore(LHS, BO);
      });
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitConv(PrimType LHS, QualType TyLHS,
                                        PrimType RHS, QualType TyRHS,
                                        const Expr *Cast) {
  if (isPrimitiveIntegral(RHS)) {
    if (isFixedReal(LHS)) {
      return this->emitCastRealFPToAlu(LHS, RHS, Cast);
    } else {
      return this->emitCast(LHS, RHS, Cast);
    }
  } else if (isFixedIntegral(RHS)) {
    if (isFixedReal(LHS)) {
      return this->emitCastRealFPToAluFP(LHS, RHS, getIntWidth(TyRHS), Cast);
    } else {
      return this->emitCastFP(LHS, RHS, getIntWidth(TyRHS), Cast);
    }
  } else if (isFixedReal(RHS)) {
    return this->emitCastRealFP(LHS, getFltSemantics(TyRHS), Cast);
  }
  llvm_unreachable("Invalid conversion");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitShortCircuit(const BinaryOperator *E) {
  if (!visitBool(E->getLHS()))
    return false;

  LabelTy Short = this->getLabel();
  const bool IsTrue = E->getOpcode() == BO_LAnd;
  if (DiscardResult) {
    if (!(IsTrue ? this->jumpFalse(Short) : this->jumpTrue(Short)))
      return false;
    if (!this->discard(E->getRHS()))
      return false;
  } else {
    if (!this->emitDupBool(E))
      return false;
    if (!(IsTrue ? this->jumpFalse(Short) : this->jumpTrue(Short)))
      return false;
    if (!this->emitPopBool(E))
      return false;
    if (!visitBool(E->getRHS()))
      return false;
  }
  this->fallthrough(Short);
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::dereference(
    const Expr *LV, DerefKind AK, llvm::function_ref<bool(PrimType)> Direct,
    llvm::function_ref<bool(PrimType)> Indirect) {
  // Special handling for the binary operator - dereference return.
  if (auto *BO = dyn_cast<BinaryOperator>(LV->IgnoreParens())) {
    if (BO->getOpcode() == BO_Comma) {
      if (!this->discard(BO->getLHS()))
        return false;
      return dereference(BO->getRHS(), AK, Direct, Indirect);
    }
  }

  // Constant expressions - value can be produced.
  if (auto *CE = dyn_cast<ConstantExpr>(LV)) {
    switch (CE->getResultStorageKind()) {
    case ConstantExpr::RSK_Int64: {
      if (DiscardResult)
        return true;
      QualType Ty = CE->getType();
      PrimType T = *classify(Ty);
      return this->emitConst(T, getIntWidth(Ty), CE->getResultAsAPSInt(), CE);
    }
    case ConstantExpr::RSK_APValue:
      llvm_unreachable("not implemented: RSK_APValue");
    case ConstantExpr::RSK_None:
      llvm_unreachable("not implemented: RSK_None");
    }
  }

  // If the type is known, try to read it directly.
  if (Optional<PrimType> T = classify(LV->getType())) {
    if (!LV->refersToBitField()) {
      // Only primitive, non bit-field types can be dereferenced directly.
      if (auto *DE = dyn_cast<DeclRefExpr>(LV)) {
        const ValueDecl *Decl = DE->getDecl();
        if (auto *VD = dyn_cast<VarDecl>(Decl)) {
          // Redirect the access to the lambda field.
          auto It = CaptureFields.find(VD);
          if (It != CaptureFields.end())
            return dereferenceLambda(DE, It->second, *T, AK, Direct, Indirect);
        }
        if (!Decl->getType()->isReferenceType()) {
          if (auto *PD = dyn_cast<ParmVarDecl>(Decl))
            return dereferenceParam(DE, *T, PD, AK, Direct, Indirect);
          if (auto *VD = dyn_cast<VarDecl>(Decl))
            return dereferenceVar(DE, *T, VD, AK, Direct, Indirect);
          if (auto *FD = dyn_cast<FunctionDecl>(Decl)) {
            if (auto *CD = dyn_cast<CXXMethodDecl>(Decl))
              if (!CD->isStatic())
                return this->emitConstMem(CD, DE);
            return getPtrConstFn(FD, DE);
          }
        }
      }
      if (auto *ME = dyn_cast<MemberExpr>(LV)) {
        if (!ME->getMemberDecl()->getType()->isReferenceType())
          return dereferenceMember(ME, *T, AK, Direct, Indirect);
      }
    }

    if (!visit(LV))
      return false;
    return Indirect(*T);
  }

  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::dereferenceParam(
    const DeclRefExpr *LV, PrimType T, const ParmVarDecl *PD, DerefKind AK,
    llvm::function_ref<bool(PrimType)> Direct,
    llvm::function_ref<bool(PrimType)> Indirect) {
  auto It = this->Params.find(PD);
  if (It != this->Params.end()) {
    unsigned Idx = It->second;
    switch (AK) {
    case DerefKind::Read:
      return DiscardResult ? true : this->emitGetParam(T, Idx, LV);

    case DerefKind::Write:
      if (!Direct(T))
        return false;
      if (!this->emitSetParam(T, Idx, LV))
        return false;
      return DiscardResult ? true : this->emitGetPtrParam(Idx, LV);

    case DerefKind::ReadWrite:
      if (!this->emitGetParam(T, Idx, LV))
        return false;
      if (!Direct(T))
        return false;
      if (!this->emitSetParam(T, Idx, LV))
        return false;
      return DiscardResult ? true : this->emitGetPtrParam(Idx, LV);
    }
    return true;
  }

  switch (AK) {
  case DerefKind::Read: {
    // Nothing to read.
    if (DiscardResult)
      return true;
    // If the param is a pointer or reference, we can dereference a dummy value.
    QualType Ty = PD->getType();
    if (Ty->isVoidPointerType())
      return emitDummyVoidPtr(Ty, PD, LV);
    if (auto *BlockTy = Ty->getAs<BlockPointerType>())
      return this->emitGetDummyObjCBlockPtr(PD, LV);
    if (auto *MemTy = Ty->getAs<MemberPointerType>())
      return this->emitGetDummyMemberPtr(PD, LV);
    if (auto *PtrTy = Ty->getAs<PointerType>()) {
      if (PtrTy->getPointeeType()->isFunctionType())
        return this->emitGetDummyFnPtr(PD, LV);
      return emitDummyPtr(PtrTy->getPointeeType(), PD, LV);
    }
    if (auto *RefTy = Ty->getAs<ReferenceType>()) {
      if (RefTy->getPointeeType()->isFunctionType())
        return this->emitGetDummyFnPtr(PD, LV);
      return emitDummyPtr(RefTy->getPointeeType(), PD, LV);
    }
    // A read of a non-const value is attempted.
    if (PD->getType()->isIntegralOrEnumerationType())
      return this->emitTrapReadNonConstInt(PD, LV);
    return this->emitTrapReadNonConstexpr(PD, LV);
  }

  case DerefKind::Write:
    // Parameters can never be written.
    return this->emitTrap(LV);

  case DerefKind::ReadWrite:
    // TODO: attempt to perform the read.
    return this->emitTrap(LV);
  }
  llvm_unreachable("invalid access kind");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::dereferenceVar(
    const DeclRefExpr *LV, PrimType T, const VarDecl *VD, DerefKind AK,
    llvm::function_ref<bool(PrimType)> Direct,
    llvm::function_ref<bool(PrimType)> Indirect) {
  // If the declaration is of a local, access it directly.
  auto It = Locals.find(VD);
  if (It != Locals.end()) {
    const auto &L = It->second;
    switch (AK) {
    case DerefKind::Read:
      if (!this->emitGetLocal(T, L.Offset, LV))
        return false;
      return DiscardResult ? this->emitPop(T, LV) : true;

    case DerefKind::Write:
      if (!Direct(T))
        return false;
      if (!this->emitSetLocal(T, L.Offset, LV))
        return false;
      return DiscardResult ? true : this->emitGetPtrLocal(L.Offset, LV);

    case DerefKind::ReadWrite:
      if (!this->emitGetLocal(T, L.Offset, LV))
        return false;
      if (!Direct(T))
        return false;
      if (!this->emitSetLocal(T, L.Offset, LV))
        return false;
      return DiscardResult ? true : this->emitGetPtrLocal(L.Offset, LV);
    }
    llvm_unreachable("invalid access kind");
  }

  // Declaration is not local - its value might be readable though.
  QualType Ty = VD->getType();
  if (Ctx.isCPlusPlus14() && VD == EvaluatingDecl) {
    // TODO: check if lifetime started in evaluation.
  } else if (AK == DerefKind::Write || AK == DerefKind::ReadWrite) {
    // All the remaining cases do not permit modification of the object.
    return this->emitTrapModifyGlobal(LV);
  } else if (VD->isConstexpr()) {
    // OK, we can read this variable.
  } else if (Ty->isIntegralOrEnumerationType()) {
    // In OpenCL if a variable is in constant address space it is a const value.
    const bool InConstSpace = Ty.getAddressSpace() == LangAS::opencl_constant;
    if (!(Ty.isConstQualified() || (Ctx.isOpenCL() && InConstSpace))) {
      if (Ctx.isCPlusPlus()) {
        return this->emitTrapReadNonConstInt(VD, LV);
      } else {
        return this->emitTrap(LV);
      }
    }
  } else if (Ty->isFloatingType() && Ty.isConstQualified()) {
    // We support folding of const floating-point types, in order to make
    // static const data members of such types (supported as an extension)
    // more useful.
    if (Ctx.isCPlusPlus11()) {
      if (!this->emitWarnReadNonConstexpr(VD, LV))
        return false;
    } else {
      if (!this->emitWarn(LV))
        return false;
    }
  } else if (Ty.isConstQualified() && VD->hasDefinition(Ctx.getASTContext())) {
    if (!this->emitWarnNonConstexpr(VD, LV))
      return false;
    // Keep evaluating to see what we can do.
  } else {
    if (Ty.isConstQualified() && !VD->hasDefinition(Ctx.getASTContext())) {
      // TODO: different behaviour when checking for potential constexpr.
      return this->bail(LV);
    } else if (Ctx.isCPlusPlus11()) {
      return this->emitTrapReadNonConstexpr(VD, LV);
    } else {
      return this->emitTrap(LV);
    }
  }

  // If the declaration was evaluated already, reproduce the value.
  for (const VarDecl *Decl : VD->redecls()) {
    if (APValue *Val = Decl->getEvaluatedValue()) {
      if (emitValue(T, *Val, LV))
        return true;
    }
  }

  // The expression should be a global with a known value - create that global.
  GlobalLocation Index;
  {
    // Global was computed and cached, read it.
    if (auto Idx = P.getGlobal(VD)) {
      Index = *Idx;
    } else {
      const Expr *Init = VD->getInit();
      if (!Init || Init->isValueDependent()) {
        // No initializer - not constexpr.
        return this->emitTrapReadNonConstexpr(VD, LV);
      } else {
        // Try to evaluate the initializer and create a global.
        if (!this->emitCreateGlobal(VD, LV))
          return false;

        // Emit an instruction to read the APValue or fetch the global index.
        if (isPrimitiveIntegral(T)) {
          return this->emitGetGlobalValue(T, VD, LV);
        } else {
          if (auto Idx = P.getGlobal(VD)) {
            Index = *Idx;
          } else {
            return false;
          }
        }
      }
    }
  }

  // A global can be produced - access it.
  switch (AK) {
  case DerefKind::Read:
    if (!this->emitGetGlobal(T, Index, LV))
      return false;
    return DiscardResult ? this->emitPop(T, LV) : true;

  case DerefKind::Write:
    if (!Direct(T))
      return false;
    if (!this->emitSetGlobal(T, Index, LV))
      return false;
    return DiscardResult ? true : this->emitGetPtrGlobal(Index, LV);

  case DerefKind::ReadWrite:
    if (!this->emitGetGlobal(T, Index, LV))
      return false;
    if (!Direct(T))
      return false;
    if (!this->emitSetGlobal(T, Index, LV))
      return false;
    return DiscardResult ? true : this->emitGetPtrGlobal(Index, LV);
  }
  llvm_unreachable("invalid access kind");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::dereferenceMember(
    const MemberExpr *ME, PrimType T, DerefKind AK,
    llvm::function_ref<bool(PrimType)> Direct,
    llvm::function_ref<bool(PrimType)> Indirect) {
  // Fetch or read the required field.
  if (auto *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
    return withField(FD, [this, ME, AK, T, Direct](const Record::Field *F) {
      const unsigned Off = F->getOffset();
      if (isa<CXXThisExpr>(ME->getBase()) && !CaptureThis) {
        switch (AK) {
        case DerefKind::Read:
          if (!this->emitGetThisField(T, Off, ME))
            return false;
          return DiscardResult ? this->emitPop(T, ME) : true;

        case DerefKind::Write:
          if (!Direct(T))
            return false;
          if (!this->emitSetThisField(T, Off, ME))
            return false;
          return DiscardResult ? true : this->emitGetPtrThisField(Off, ME);

        case DerefKind::ReadWrite:
          if (!this->emitGetThisField(T, Off, ME))
            return false;
          if (!Direct(T))
            return false;
          if (!this->emitSetThisField(T, Off, ME))
            return false;
          return DiscardResult ? true : this->emitGetPtrThisField(Off, ME);
        }
      } else {
        if (!visit(ME->getBase()))
          return false;

        switch (AK) {
        case DerefKind::Read:
          if (!this->emitGetFieldPop(T, Off, ME))
            return false;
          return DiscardResult ? this->emitPop(T, ME) : true;

        case DerefKind::Write:
          if (!Direct(T))
            return false;
          if (DiscardResult)
            return this->emitSetFieldPop(T, Off, ME);
          else
            return this->emitSetField(T, Off, ME);

        case DerefKind::ReadWrite:
          if (!this->emitGetField(T, Off, ME))
            return false;
          if (!Direct(T))
            return false;
          if (DiscardResult)
            return this->emitSetFieldPop(T, Off, ME);
          else
            return this->emitSetField(T, Off, ME);
        }
      }
      return false;
    });
  }

  // Value cannot be produced - try to emit pointer.
  return visit(ME) && Indirect(T);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::dereferenceLambda(
    const DeclRefExpr *DE, const FieldDecl *FD, PrimType T, DerefKind AK,
    llvm::function_ref<bool(PrimType)> Direct,
    llvm::function_ref<bool(PrimType)> Indirect) {
  if (!FD->getType()->isReferenceType()) {
    return withField(FD, [this, DE, T, AK, Direct](const Record::Field *F) {
      const unsigned Off = F->getOffset();
      switch (AK) {
      case DerefKind::Read:
        if (!this->emitGetThisField(T, Off, DE))
          return false;
        return DiscardResult ? this->emitPop(T, DE) : true;

      case DerefKind::Write:
        if (!Direct(T))
          return false;
        if (!this->emitSetThisField(T, Off, DE))
          return false;
        return DiscardResult ? true : this->emitGetPtrThisField(Off, DE);

      case DerefKind::ReadWrite:
        if (!this->emitGetThisField(T, Off, DE))
          return false;
        if (!Direct(T))
          return false;
        if (!this->emitSetThisField(T, Off, DE))
          return false;
        return DiscardResult ? true : this->emitGetPtrThisField(Off, DE);
      }
      llvm_unreachable("invalid access kind");
    });
  } else {
    return visit(DE) && Indirect(T);
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::lvalueToRvalue(const Expr *LV, const Expr *E) {
  return dereference(
      LV, DerefKind::Read,
      [](PrimType) {
        // Value loaded - nothing to do here.
        return true;
      },
      [this, E](PrimType T) {
        // Pointer on stack - dereference it.
        if (!this->emitLoadPop(T, E))
          return false;
        return DiscardResult ? this->emitPop(T, E) : true;
      });
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitConst(PrimType T, unsigned NumBits,
                                         const APInt &Value, const Expr *E) {
  switch (T) {
  case PT_Sint8:
    return this->emitConstSint8(Value.getSExtValue(), E);
  case PT_Uint8:
    return this->emitConstUint8(Value.getZExtValue(), E);
  case PT_Sint16:
    return this->emitConstSint16(Value.getSExtValue(), E);
  case PT_Uint16:
    return this->emitConstUint16(Value.getZExtValue(), E);
  case PT_Sint32:
    return this->emitConstSint32(Value.getSExtValue(), E);
  case PT_Uint32:
    return this->emitConstUint32(Value.getZExtValue(), E);
  case PT_Sint64:
    return this->emitConstSint64(Value.getSExtValue(), E);
  case PT_Uint64:
    return this->emitConstUint64(Value.getZExtValue(), E);
  case PT_SintFP:
    return this->emitConstFPSintFP(NumBits, Value.getSExtValue(), E);
  case PT_UintFP:
    return this->emitConstFPUintFP(NumBits, Value.getZExtValue(), E);
  case PT_Bool:
    return this->emitConstBool(Value.getBoolValue(), E);
  case PT_Ptr:
  case PT_FnPtr:
  case PT_RealFP:
  case PT_MemPtr:
  case PT_VoidPtr:
  case PT_ObjCBlockPtr:
    llvm_unreachable("Invalid integral type");
    break;
  }
  llvm_unreachable("unknown primitive type");
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitValue(PrimType T, APValue &V,
                                         const Expr *E) {
  switch (T) {
  case PT_Sint8:
    return V.isInt() && this->emitConstSint8(V.getInt().getSExtValue(), E);
  case PT_Uint8:
    return V.isInt() && this->emitConstUint8(V.getInt().getZExtValue(), E);
  case PT_Sint16:
    return V.isInt() && this->emitConstSint16(V.getInt().getSExtValue(), E);
  case PT_Uint16:
    return V.isInt() && this->emitConstUint16(V.getInt().getZExtValue(), E);
  case PT_Sint32:
    return V.isInt() && this->emitConstSint32(V.getInt().getSExtValue(), E);
  case PT_Uint32:
    return V.isInt() && this->emitConstUint32(V.getInt().getZExtValue(), E);
  case PT_Sint64:
    return V.isInt() && this->emitConstSint64(V.getInt().getSExtValue(), E);
  case PT_Uint64:
    return V.isInt() && this->emitConstUint64(V.getInt().getZExtValue(), E);
  case PT_Bool:
    return V.isInt() && this->emitConstBool(V.getInt().getBoolValue(), E);
  default:
    return false;
  }
}

template <class Emitter>
unsigned ByteCodeExprGen<Emitter>::allocateLocalPrimitive(DeclTy &&Src,
                                                          QualType Ty,
                                                          PrimType Type,
                                                          bool IsConst,
                                                          bool IsExtended) {
  Descriptor *D = P.createDescriptor(Src, Ty.getTypePtr(), Type, IsConst,
                                     Src.is<const Expr *>());
  Scope::Local Local = this->createLocal(D);
  if (auto *VD = dyn_cast_or_null<ValueDecl>(Src.dyn_cast<const Decl *>()))
    Locals.insert({VD, Local});
  VarScope->add(Local, IsExtended);
  return Local.Offset;
}

template <class Emitter>
llvm::Optional<unsigned>
ByteCodeExprGen<Emitter>::allocateLocal(DeclTy &&Src, QualType Ty,
                                        bool IsExtended) {
  const ValueDecl *Key = nullptr;
  bool IsTemporary = false;
  if (auto *VD = dyn_cast_or_null<ValueDecl>(Src.dyn_cast<const Decl *>())) {
    Key = VD;
  }
  if (auto *E = Src.dyn_cast<const Expr *>()) {
    IsTemporary = true;
  }

  Descriptor *D = P.createDescriptor(Src, Ty.getTypePtr(),
                                     Ty.isConstQualified(), IsTemporary);
  if (!D)
    return {};

  Scope::Local Local = this->createLocal(D);
  if (Key)
    Locals.insert({Key, Local});
  VarScope->add(Local, IsExtended);
  return Local.Offset;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitDummyPtr(QualType Ty, const VarDecl *VD,
                                            const Expr *E) {
  const VarDecl *Canonical = VD->getCanonicalDecl();
  if (Descriptor *Desc = P.createDescriptor(Canonical, Ty))
    return this->emitGetDummyPtr(Canonical, Desc, E);
  return this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::emitDummyVoidPtr(QualType Ty, const VarDecl *VD,
                                                const Expr *E) {
  const VarDecl *Canonical = VD->getCanonicalDecl();
  if (Descriptor *Desc = P.createDescriptor(Canonical, Ty))
    return this->emitGetDummyVoidPtr(Canonical, Desc, E);
  return this->emitTrap(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitArguments(QualType CalleeTy,
                                              ArrayRef<const Expr *> Args) {
  if (auto *MemberTy = CalleeTy->getAs<MemberPointerType>())
    CalleeTy = MemberTy->getPointeeType();
  if (auto *PtrTy = CalleeTy->getAs<PointerType>())
    CalleeTy = PtrTy->getPointeeType();

  if (auto *Ty = CalleeTy->getAs<FunctionProtoType>()) {
    unsigned NumParams = Ty->getNumParams();
    for (unsigned I = 0, N = Args.size(); I < N; ++I)
      if (!visitArgument(Args[I], I >= NumParams))
        return false;
  } else {
    for (unsigned I = 0, N = Args.size(); I < N; ++I)
      if (!visitArgument(Args[I], /*discard=*/true))
        return false;
  }

  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitArgument(const Expr *E, bool Discard) {
  // Primitive or pointer argument - leave it on the stack.
  if (Optional<PrimType> T = classify(E))
    return Discard ? discard(E) : visit(E);

  // Composite argument - copy construct and push pointer.
  if (auto I = allocateLocal(E, E->getType(), /*isExtended=*/false)) {
    if (!visitLocalInitializer(E, *I))
      return false;
    return Discard ? true : this->emitGetPtrLocal(*I, E);
  }

  return this->bail(E);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitInitializer(
    const Expr *Init, InitFnRef InitFn) {
  OptionScope<Emitter> Scope(this, InitFn);
  return this->Visit(Init);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitBaseInitializer(const Expr *Init,
                                                    InitFnRef GenBase) {
  OptionScope<Emitter> Scope(this, GenBase, InitKind::BASE);
  return this->Visit(Init);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::getPtrConstFn(const FunctionDecl *FD,
                                             const Expr *E) {
  const FunctionDecl *Canonical = FD->getCanonicalDecl();
  if (Canonical->isWeak())
    return this->emitConstNoFn(Canonical, E);

  if (Expected<Function *> Func = P.getOrCreateFunction(S, Canonical)) {
    if (*Func)
      return this->emitConstFn(*Func, E);
    else
      return this->emitConstNoFn(Canonical, E);
  } else {
    consumeError(Func.takeError());
    return this->emitConstNoFn(Canonical, E);
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::getPtrVarDecl(const VarDecl *VD, const Expr *E) {
   // Helper to emit a pointer.
  auto EmitPointer = [this, VD, E](const llvm::Optional<GlobalLocation> &Idx) {
    if (Idx) {
      if (VD->getType()->isReferenceType())
        return this->emitGetGlobalPtr(*Idx, E);
      return this->emitGetPtrGlobal(*Idx, E);
    }
    return false;
  };

  QualType Ty = VD->getType();
  // If there is no definition, create a dummy block.
  if (!VD->getAnyInitializer(VD))
    return emitDummyPtr(Ty, VD, E);

  // Check if the definition should be evaluated.
  if (VD->isConstexpr()) {
    // If the global was already evaluated, return its index.
    return EmitPointer(P.getGlobal(VD));
  } else if (Ty->isReferenceType()) {
    // Try to place a pointer into the global.
  } else if (Ty->isIntegralOrEnumerationType()) {
    // Do not evaluate non-const integers.
    const bool InConstSpace = Ty.getAddressSpace() == LangAS::opencl_constant;
    if (!(Ty.isConstQualified() || (Ctx.isOpenCL() && InConstSpace)))
      return emitDummyPtr(Ty, VD, E);
  } else if (Ty->isFloatingType() && Ty.isConstQualified()) {
    // Evaluate floats.
  } else if (Ty.isConstQualified() && VD->hasDefinition(Ctx.getASTContext())) {
    // Evaluate all other const objects.
  } else {
    // Do not allow any other objects - create an object for dummy pointers.
    return emitDummyPtr(Ty, VD, E);
  }

  // Run the initializer and return an ID for the global.
  if (!this->emitCreateGlobal(VD, E))
    return false;
  return EmitPointer(P.getGlobal(VD));
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitStringInitializer(const StringLiteral *S) {
  // Find the type to represent characters.
  const unsigned CharBytes = S->getCharByteWidth();
  PrimType CharType;
  switch (CharBytes) {
  case 1:
    CharType = PT_Sint8;
    break;
  case 2:
    CharType = PT_Uint16;
    break;
  case 4:
    CharType = PT_Uint32;
    break;
  default:
    llvm_unreachable("Unsupported character width!");
  }

  if (!emitInitFn())
    return false;

  // Initialise elements one by one, advancing the pointer.
  const unsigned NumBits = CharBytes * getCharBit();
  for (unsigned I = 0, N = S->getLength(); I < N; ++I) {
    // Set individual characters here.
    APInt CodeUnit(NumBits, S->getCodeUnit(I));
    // Lower the code point.
    if (!emitConst(CharType, NumBits, CodeUnit, S))
      return false;
    // Set the character.
    if (!this->emitInitElem(CharType, I, S))
      return false;
  }

  // Set the null terminator.
  if (!emitConst(CharType, NumBits, APInt(NumBits, 0), S))
    return false;
  if (!this->emitInitElemPop(CharType, S->getLength(), S))
    return false;

  // String was initialised.
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitRecordInitializer(
    const RecordType *RT, const InitListExpr *List) {
  auto *R = P.getOrCreateRecord(RT->getDecl());
  if (!R)
    return this->bail(List);

  if (R->isUnion()) {
    if (auto *InitField = List->getInitializedFieldInUnion()) {
      auto *F = R->getField(InitField);
      switch (List->getNumInits()) {
      case 0: {
        // Clang doesn't provide an initializer - need to default init.
        ImplicitValueInitExpr Init(InitField->getType());
        if (!visitRecordFieldInitializer(R, F, &Init))
          return false;
        break;
      }

      case 1: {
        // Initialise the element.
        if (!visitRecordFieldInitializer(R, F, List->getInit(0)))
          return false;
        break;
      }

      default:
        // Malformed initializer.
        return false;
      }
    }
  } else {
    unsigned Init = 0;
    for (unsigned I = 0, N = R->getNumBases(); I < N; ++I) {
      const Record::Base *B = R->getBase(I);
      BaseScope<Emitter> Scope(this, [this, B, List] (InitFnRef Ptr) {
        return Ptr() && this->emitGetPtrBase(B->getOffset(), List);
      });

      if (Init >= List->getNumInits()) {
        ImplicitValueInitExpr Init(Ctx.getASTContext().getRecordType(B->getDecl()));
        if (!this->Visit(&Init))
          return false;
      } else {
        if (!this->Visit(List->getInit(Init++)))
          return false;
      }
    }
    for (unsigned I = 0, N = R->getNumFields(); I < N; ++I) {
      const Record::Field *F = R->getField(I);

      if (F->getDecl()->isUnnamedBitfield() || Init >= List->getNumInits()) {
        ImplicitValueInitExpr Init(F->getDecl()->getType());
        if (!visitRecordFieldInitializer(R, F, &Init))
          return false;
      } else {
        if (!visitRecordFieldInitializer(R, F, List->getInit(Init++)))
          return false;
      }
    }
  }

  switch (Initialiser) {
  case InitKind::ROOT:
    // Nothing to initialise.
    return true;
  case InitKind::BASE:
    // Initialise the base class.
    return this->emitInitFn() && this->emitInitialise(List);
  case InitKind::UNION:
    // Activate the union field.
    return this->emitInitFn() && this->emitActivate(List);
  }
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitRecordFieldInitializer(
    Record *R, const Record::Field *F, const Expr *E) {
  if (Optional<PrimType> T = classify(E)) {
    if (!emitInitFn())
      return false;
    if (!visit(E))
      return false;
    if (R->isUnion())
      return this->emitInitFieldActive(*T, F->getOffset(), E);
    else if (F->getDecl()->isBitField())
      return this->emitInitBitField(*T, F, E);
    else
      return this->emitInitFieldPop(*T, F->getOffset(), E);
  } else {
    auto FieldFn = [this, F, E](InitFnRef Ptr) {
      return Ptr() && this->emitGetPtrField(F->getOffset(), E);
    };
    if (R->isUnion()) {
      UnionScope<Emitter> Scope(this, FieldFn);
      return this->Visit(E);
    } else {
      FieldScope<Emitter> Scope(this, FieldFn);
      return this->Visit(E);
    }
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitComplexInitializer(
    const ComplexType *CT, const InitListExpr *List) {
  QualType ElemTy = CT->getElementType();

  ImplicitValueInitExpr ZE(ElemTy);
  PrimType T = classifyPrim(ElemTy);

  auto InitElem = [this, T](unsigned Index, const Expr *E) -> bool {
    if (!emitInitFn())
      return false;
    if (!this->emitDecayPtr(E))
      return false;
    if (!visit(E))
      return false;
    return this->emitInitElemPop(T, Index, E);
  };

  switch (List->getNumInits()) {
  case 0:
    return InitElem(0, &ZE) && InitElem(1, &ZE);
  case 1:
    return this->Visit(List->getInit(0));
  case 2:
    return InitElem(0, List->getInit(0)) && InitElem(1, List->getInit(1));
  default:
    llvm_unreachable("invalid complex initializer");
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitVectorInitializer(
    const VectorType *VT, const InitListExpr *List) {
  QualType ElemTy = VT->getElementType();

  unsigned Index = 0;
  if (llvm::Optional<PrimType> T = classify(ElemTy)) {
    for (const Expr *E : List->inits()) {
      if (auto *EVT = E->getType()->getAs<VectorType>()) {
        return this->bail(E);
      } else {
        if (!emitInitFn())
          return false;
        if (!this->emitDecayPtr(E))
          return false;
        if (!this->visit(E))
          return false;
        if (!this->emitInitElem(*T, Index++, List))
          return false;
      }
    }

    for (unsigned I = Index, N = VT->getNumElements(); I < N; ++I) {
      ImplicitValueInitExpr ZE(ElemTy);
      if (!emitInitFn())
        return false;
      if (!this->emitDecayPtr(List))
        return false;
      if (!this->visit(&ZE))
        return false;
      if (!this->emitInitElem(*T, I, List))
        return false;
    }
    return true;
  }
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitCastToComplex(const CastExpr *CE) {
  QualType SubType = CE->getSubExpr()->getType();
  PrimType T = classifyPrim(SubType);

  if (!emitInitFn())
    return false;
  if (!this->emitDecayPtr(CE))
    return false;
  if (!this->visit(CE->getSubExpr()))
    return false;
  if (!this->emitInitElem(T, 0, CE))
    return false;
  ImplicitValueInitExpr ZE(SubType);
  if (!visit(&ZE))
    return false;
  return this->emitInitElemPop(T, 1, CE);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitCastToReal(const CastExpr *CE) {
  if (!this->visit(CE->getSubExpr()))
    return false;
  if (DiscardResult)
    return this->emitPopPtr(CE);
  if (!this->emitDecayPtr(CE))
    return false;
  return this->emitGetElem(classifyPrim(CE->getType()), 0u, CE);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitArrayInitializer(
    const ConstantArrayType *AT,
    const Expr *List,
    llvm::function_ref<const Expr *(uint64_t)> Elem) {
  uint64_t NumElems = AT->getSize().getZExtValue();
  if (Optional<PrimType> T = classify(AT->getElementType())) {
    if (NumElems == 0)
      return true;

    if (!emitInitFn() || !this->emitDecayPtr(List))
      return false;

    for (unsigned I = 0; I < NumElems; ++I) {
      const Expr *ElemInit = Elem(I);
      // Construct the value.
      if (!visit(ElemInit))
        return false;
      // Set the element.
      if (I + 1 != NumElems) {
        if (!this->emitInitElem(*T, I, ElemInit))
          return false;
      } else {
        if (!this->emitInitElemPop(*T, I, ElemInit))
          return false;
      }
    }
  } else {
    for (unsigned I = 0; I < NumElems; ++I) {
      const Expr *ElemInit = Elem(I);

      // Generate a pointer to the field from the array pointer.
      FieldScope<Emitter> Scope(this, [this, I, ElemInit](InitFnRef Ptr) {
        if (!Ptr())
          return false;
        if (!this->emitDecayPtr(ElemInit))
          return false;
        if (!this->emitConstUint32(I, ElemInit))
          return false;
        return this->emitAddOffsetUint32(ElemInit);
      });
      if (!this->Visit(ElemInit))
        return false;
    }
  }

  switch (Initialiser) {
  case InitKind::ROOT:
    return true;
  case InitKind::BASE:
    llvm_unreachable("arrays cannot be base classes");
  case InitKind::UNION:
    return this->emitInitFn() && this->emitActivate(List);
  }
  return false;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitZeroInitializer(const Expr *E,
                                                    const RecordDecl *RD) {
  Record *R = P.getOrCreateRecord(RD);
  if (!R)
    return false;

  for (auto &B : R->bases()) {
    BaseScope<Emitter> Scope(this, [this, &B, E](InitFnRef Ptr) {
      return Ptr() && this->emitGetPtrBase(B.getOffset(), E);
    });
    if (!visitZeroInitializer(E, B.getDecl()))
      return false;
  }

  for (auto &F : R->fields()) {
    const FieldDecl *Decl = F.getDecl();
    InterpUnits Offset = F.getOffset();
    ImplicitValueInitExpr Init(Decl->getType());
    if (Optional<PrimType> T = classify(Decl->getType())) {
      if (!emitInitFn())
        return false;
      if (!visitZeroInitializer(*T, &Init))
        return false;
      if (Decl->isBitField()) {
        if (!this->emitInitBitField(*T, &F, E))
          return false;
      } else {
        if (!this->emitInitFieldPop(*T, Offset, E))
          return false;
      }
    } else {
      FieldScope<Emitter> Scope(this, [this, Offset, E](InitFnRef Ptr) {
        return Ptr() && this->emitGetPtrField(Offset, E);
      });
      if (!this->Visit(&Init))
        return false;
    }
  }
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::withField(
    const FieldDecl *F,
    llvm::function_ref<bool(const Record::Field *)> GenField) {
  if (auto *R = getRecord(F->getParent()))
    if (auto *Field = R->getField(F))
      return GenField(Field);
  return false;
}

template <class Emitter>
const RecordType *ByteCodeExprGen<Emitter>::getRecordTy(QualType Ty) {
  if (auto *PT = dyn_cast<PointerType>(Ty))
    return PT->getPointeeType()->getAs<RecordType>();
  return Ty->getAs<RecordType>();
}

template <class Emitter>
Record *ByteCodeExprGen<Emitter>::getRecord(QualType Ty) {
  if (auto *RecordTy = getRecordTy(Ty))
    return getRecord(RecordTy->getDecl());
  return nullptr;
}

template <class Emitter>
Record *ByteCodeExprGen<Emitter>::getRecord(const RecordDecl *RD) {
  return P.getOrCreateRecord(RD);
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitOpaqueExpr(const OpaqueValueExpr *E) {
  QualType Ty = E->getType();

  PrimType PrimTy;
  if (Optional<PrimType> T = classify(Ty)) {
    PrimTy = *T;
  } else {
    PrimTy = PT_Ptr;
  }

  auto Off = allocateLocalPrimitive(E, Ty, PrimTy, /*isConst=*/true);
  if (!visit(E->getSourceExpr()))
    return false;
  this->emitSetLocal(PrimTy, Off, E);
  OpaqueExprs.insert({E, Off});
  return true;
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitExpr(const Expr *Exp) {
  ExprScope<Emitter> RootScope(this);

  if (Optional<PrimType> ExpTy = classify(Exp)) {
    if (!visit(Exp))
      return false;
    return this->emitRet(*ExpTy, Exp);
  } else {
    // Composite value - allocate a local and initialize it.
    if (auto I = allocateLocal(Exp, Exp->getType(), /*isExtended=*/false)) {
      if (!visitLocalInitializer(Exp, *I))
        return false;
      if (!this->emitGetPtrLocal(*I, Exp))
        return false;
      return this->emitRetValue(Exp);
    }
    return this->bail(Exp);
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitRValue(const Expr *Exp) {
  ExprScope<Emitter> RootScope(this);

  if (Optional<PrimType> ExpTy = classify(Exp)) {
    if (Exp->isGLValue()) {
      // If the expression is an lvalue, dereference it.
      if (!lvalueToRvalue(Exp, Exp))
        return false;

      // Find the type of the expression.
      PrimType Ty;
      if (Optional<PrimType> T = classify(Exp->getType())) {
        Ty = *T;
      } else {
        Ty = PT_Ptr;
      }

      return this->emitRet(Ty, Exp);
    } else {
      // Otherwise, simply evaluate the rvalue.
      if (!visit(Exp))
        return false;
      return this->emitRet(*ExpTy, Exp);
    }
  } else {
    QualType Ty = Exp->getType();

    // Do no evaluate non-literal types.
    if (!Ty->isLiteralType(Ctx.getASTContext()))
      if (!this->emitTrapNonLiteral(Exp))
        return false;

    // Allocate a local and initialize it.
    if (auto I = allocateLocal(Exp, Ty, /*isExtended=*/false)) {
      if (!visitLocalInitializer(Exp, *I))
        return false;
      if (!this->emitGetPtrLocal(*I, Exp))
        return false;
      return this->emitRetValue(Exp);
    }
    return this->bail(Exp);
  }
}

template <class Emitter>
bool ByteCodeExprGen<Emitter>::visitDecl(const VarDecl *VD) {
  // Computes the value of a declaration without initialising or
  // creating global storage. The global block is allocated later on,
  // if the declaration is ever referenced through an lvalue.
  auto InitLazyPrimitive = [this, VD](GlobalLocation I, PrimType T) {
    {
      // Primitive declarations - compute the value and set it.
      DeclScope<Emitter> LocalScope(this, VD);
      if (!visit(VD->getInit()))
        return false;
    }
    return this->emitRet(T, VD);
  };

  // Initialises and creates a block to store a primitive.
  auto InitEagerPrimitive = [this, VD](GlobalLocation I, PrimType T) {
    {
      // Primitive declarations - compute the value and set it.
      DeclScope<Emitter> LocalScope(this, VD);
      if (!visit(VD->getInit()))
        return false;
    }
    // If the declaration is global, save the value for later use.
    if (!this->emitDup(T, VD))
      return false;
    if (!this->emitInitGlobal(T, I, VD))
      return false;
    return this->emitRet(T, VD);
  };

  // Helper to initialise a composite global.
  auto InitComposite = [this, VD](GlobalLocation I) {
    {
      // Composite declarations - allocate storage and initialize it.
      DeclScope<Emitter> LocalScope(this, VD);
      if (!visitGlobalInitializer(VD->getInit(), I))
        return false;
    }
    // Return a pointer to the global.
    if (!this->emitGetPtrGlobal(I, VD))
      return false;
    return this->emitRetValue(VD);
  };

  // Try to create the global the erase it if it was not initialised.
  EvaluatingDecl = VD;
  if (Optional<PrimType> T = classify(VD->getType())) {
    switch (*T) {
    case PT_Sint8:
    case PT_Uint8:
    case PT_Sint16:
    case PT_Uint16:
    case PT_Sint32:
    case PT_Uint32:
    case PT_Sint64:
    case PT_Uint64:
    case PT_Bool:
      if (Optional<GlobalLocation> I = P.prepareGlobal(VD)) {
        if (!InitLazyPrimitive(*I, *T)) {
          P.removeGlobal(VD);
          return false;
        }
        return true;
      }
      break;
    case PT_SintFP:
    case PT_UintFP:
    case PT_RealFP:
    case PT_Ptr:
    case PT_FnPtr:
    case PT_MemPtr:
    case PT_VoidPtr:
    case PT_ObjCBlockPtr:
      if (Optional<GlobalLocation> I = P.createGlobal(VD)) {
        if (!InitEagerPrimitive(*I, *T)) {
          P.removeGlobal(VD);
          return false;
        }
        return true;
      }
      break;
    }
  } else {
    if (Optional<GlobalLocation> I = P.createGlobal(VD)) {
      if (!InitComposite(*I)) {
        P.removeGlobal(VD);
        return false;
      }
      return true;
    }
  }

  return this->bail(VD);
}

template <class Emitter>
void ByteCodeExprGen<Emitter>::emitCleanup() {
  for (VariableScope<Emitter> *C = VarScope; C; C = C->getParent())
    C->emitDestruction();
}

namespace clang {
namespace interp {

template class ByteCodeExprGen<ByteCodeEmitter>;
template class ByteCodeExprGen<EvalEmitter>;

} // namespace interp
} // namespace clang
