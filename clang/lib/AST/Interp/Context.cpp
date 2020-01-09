//===--- Context.cpp - Context for the constexpr VM -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Context.h"
#include "ByteCodeEmitter.h"
#include "ByteCodeExprGen.h"
#include "ByteCodeStmtGen.h"
#include "EvalEmitter.h"
#include "InterpFrame.h"
#include "InterpLoop.h"
#include "InterpStack.h"
#include "PrimType.h"
#include "Program.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/TargetInfo.h"

using namespace clang;
using namespace clang::interp;

namespace {

static PrimType GetIntType(unsigned Width) {
  switch (Width) {
  case 8:
    return PT_Sint8;
  case 16:
    return PT_Sint16;
  case 32:
    return PT_Sint32;
  case 64:
    return PT_Sint64;
  default:
    return PT_SintFP;
  }
}

/// A fast evaluator for common and simple r-values.
class FastEval : public ConstStmtVisitor<FastEval, bool> {
public:
  FastEval(ASTContext &Ctx, APValue &Result) : Ctx(Ctx), Result(Result) {}

  bool VisitCharacterLiteral(const CharacterLiteral *E) {
    Result = APValue(Ctx.MakeIntValue(E->getValue(), E->getType()));
    return true;
  }

  bool VisitCXXBoolLiteralExpr(const CXXBoolLiteralExpr *E) {
    Result = APValue(Ctx.MakeIntValue(E->getValue(), E->getType()));
    return true;
  }

  bool VisitTypeTraitExpr(const TypeTraitExpr *E) {
    Result = APValue(Ctx.MakeIntValue(E->getValue(), E->getType()));
    return true;
  }

  bool VisitIntegerLiteral(const IntegerLiteral *E) {
    const bool IsUnsigned = E->getType()->isUnsignedIntegerOrEnumerationType();
    Result = APValue(APSInt(E->getValue()));
    Result.getInt().setIsUnsigned(IsUnsigned);
    return true;
  }

  bool VisitCastExpr(const CastExpr *E) {
    QualType DestType = E->getType();
    switch (E->getCastKind()) {
    default:
      return false;

    case CK_UserDefinedConversion:
    case CK_NoOp:
      return Visit(E->getSubExpr());

    case CK_IntegralCast: {
      if (!Visit(E->getSubExpr()) || !Result.isInt())
        return false;
      unsigned DestWidth = Ctx.getIntWidth(E->getType());
      APSInt Value;
      if (DestType->isBooleanType()) {
        Value = Result.getInt().getBoolValue();
      } else {
        Value = Result.getInt().extOrTrunc(DestWidth);
        Value.setIsUnsigned(DestType->isUnsignedIntegerOrEnumerationType());
      }
      Result = APValue(Value);
      return true;
    }
    }
  }

  bool
  VisitSubstNonTypeTemplateParmExpr(const SubstNonTypeTemplateParmExpr *E) {
    return Visit(E->getReplacement());
  }

  bool VisitParenExpr(const ParenExpr *E) {
    return Visit(E->getSubExpr());
  }

  bool VisitStmt(const Stmt *) { return false; }
  bool VisitExpr(const Expr *E) { return false; }

private:
  ASTContext &Ctx;
  APValue &Result;
};

} // namespace

Context::Context(ASTContext &Ctx) : Ctx(Ctx), P(new Program(*this)) {}

Context::~Context() {}

bool Context::isPotentialConstantExpr(State &Parent, const FunctionDecl *FD) {
  Function *Func = P->getFunction(FD);
  if (!Func) {
    ByteCodeStmtGen<ByteCodeEmitter> C(*this, *P, Parent);
    if (auto R = C.compileFunc(FD)) {
      Func = *R;
    } else {
      handleAllErrors(R.takeError(), [&Parent](ByteCodeGenError &Err) {
        Parent.FFDiag(Err.getLoc(), diag::err_experimental_clang_interp_failed);
      });
      return false;
    }
  }

  if (!Func->isConstexpr())
    return false;

  APValue Dummy;
  return Run(Parent, Func, Dummy);
}

bool Context::evaluate(State &Parent, const Expr *E, APValue &Result) {
  if (FastEval(Ctx, Result).Visit(E))
    return true;
  ByteCodeExprGen<EvalEmitter> C(*this, *P, Parent, Stk, Result);
  return Check(Parent, C.interpretExpr(E));
}

bool Context::evaluateAsRValue(State &Parent, const Expr *E, APValue &Result) {
  if (FastEval(Ctx, Result).Visit(E))
    return true;
  ByteCodeExprGen<EvalEmitter> C(*this, *P, Parent, Stk, Result);
  return Check(Parent, C.interpretRValue(E));
}

bool Context::evaluateAsInitializer(State &Parent, const VarDecl *VD,
                                    APValue &Result) {
  ByteCodeExprGen<EvalEmitter> C(*this, *P, Parent, Stk, Result);
  return Check(Parent, C.interpretDecl(VD));
}

const LangOptions &Context::getLangOpts() const { return Ctx.getLangOpts(); }

llvm::Optional<PrimType> Context::classify(QualType T) {
  // Function type.
  if (T->isFunctionReferenceType())
    return PT_FnPtr;
  if (T->isFunctionPointerType())
    return PT_FnPtr;
  if (T->isFunctionProtoType())
    return PT_FnPtr;
  if (T->isFunctionType())
    return PT_FnPtr;
  // ObjC block type.
  if (T->isBlockPointerType())
    return PT_ObjCBlockPtr;

  // Member pointer type check.
  if (T->isMemberPointerType())
    return PT_MemPtr;

  // Void pointers.
  if (T->isVoidPointerType())
    return PT_VoidPtr;

  // Pointer type check.
  if (T->isReferenceType())
    return PT_Ptr;
  if (T->isPointerType())
    return PT_Ptr;
  if (T->isNullPtrType())
    return PT_Ptr;
  if (T->isObjCObjectPointerType())
    return PT_Ptr;

  // Boolean check.
  if (T->isBooleanType())
    return PT_Bool;

  // Signed integral type check.
  if (T->isSignedIntegerOrEnumerationType()) {
    switch (Ctx.getIntWidth(T)) {
    case 64:
      return PT_Sint64;
    case 32:
      return PT_Sint32;
    case 16:
      return PT_Sint16;
    case 8:
      return PT_Sint8;
    default:
      return PT_SintFP;
    }
  }

  // Unsigned integral type check.
  if (T->isUnsignedIntegerOrEnumerationType()) {
    switch (Ctx.getIntWidth(T)) {
    case 64:
      return PT_Uint64;
    case 32:
      return PT_Uint32;
    case 16:
      return PT_Uint16;
    case 8:
      return PT_Uint8;
    default:
      return PT_UintFP;
    }
  }

  // Float type check.
  if (T->isRealFloatingType())
    return PT_RealFP;

  // Atomic types are simply unwrapped.
  if (auto *AT = dyn_cast<AtomicType>(T))
    return classify(AT->getValueType());

  return {};
}

unsigned Context::getCharBit() const {
  return Ctx.getTargetInfo().getCharWidth();
}

PrimType Context::getShortType() { return GetIntType(Ctx.getShortWidth()); }

PrimType Context::getIntType() { return GetIntType(Ctx.getIntWidth()); }

PrimType Context::getLongType() { return GetIntType(Ctx.getLongWidth()); }

PrimType Context::getUnsignedLongType() {
  return GetIntType(Ctx.getUnsignedLongWidth());
}

PrimType Context::getLongLongType() {
  return GetIntType(Ctx.getLongLongWidth());
}

uint64_t Context::getShortWidth() { return Ctx.getShortWidth(); }

uint64_t Context::getIntWidth() { return Ctx.getIntWidth(); }

uint64_t Context::getLongWidth() { return Ctx.getLongWidth(); }

uint64_t Context::getUnsignedLongWidth() { return Ctx.getUnsignedLongWidth(); }

uint64_t Context::getLongLongWidth() { return Ctx.getLongLongWidth(); }

bool Context::Run(State &Parent, Function *Func, APValue &Result) {
  InterpState State(Parent, *P, Stk, *this);
  State.Current = new InterpFrame(State, Func, nullptr, {}, {});
  if (Interpret(State, Result))
    return true;
  Stk.clear();
  return false;
}

bool Context::Check(State &Parent, llvm::Expected<bool> &&Flag) {
  if (Flag)
    return *Flag;
  handleAllErrors(Flag.takeError(), [&Parent](ByteCodeGenError &Err) {
    Parent.FFDiag(Err.getLoc(), diag::err_experimental_clang_interp_failed);
  });
  return false;
}
