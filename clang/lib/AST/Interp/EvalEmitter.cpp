//===--- EvalEmitter.cpp - Instruction emitter for the VM -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EvalEmitter.h"
#include "Context.h"
#include "Interp.h"
#include "InterpHelper.h"
#include "InterpLoop.h"
#include "Opcode.h"
#include "Program.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/Builtins.h"

using namespace clang;
using namespace clang::interp;

using APSInt = llvm::APSInt;
template <typename T> using Expected = llvm::Expected<T>;

static bool IsGlobalLValue(APValue::LValueBase B) {
  // C++11 [expr.const]p3 An address constant expression is a prvalue core
  // constant expression of pointer type that evaluates to...

  // ... a null pointer value, or a prvalue core constant expression of type
  // std::nullptr_t.
  if (!B) return true;

  if (const ValueDecl *D = B.dyn_cast<const ValueDecl*>()) {
    // ... the address of an object with static storage duration,
    if (const VarDecl *VD = dyn_cast<VarDecl>(D))
      return VD->hasGlobalStorage();
    // ... the address of a function,
    return isa<FunctionDecl>(D);
  }

  if (B.is<TypeInfoLValue>() || B.is<DynamicAllocLValue>())
    return true;

  const Expr *E = B.get<const Expr*>();
  switch (E->getStmtClass()) {
  default:
    return false;
  case Expr::CompoundLiteralExprClass: {
    const CompoundLiteralExpr *CLE = cast<CompoundLiteralExpr>(E);
    return CLE->isFileScope() && CLE->isLValue();
  }
  case Expr::MaterializeTemporaryExprClass:
    // A materialized temporary might have been lifetime-extended to static
    // storage duration.
    return cast<MaterializeTemporaryExpr>(E)->getStorageDuration() == SD_Static;
  // A string literal has static storage duration.
  case Expr::StringLiteralClass:
  case Expr::PredefinedExprClass:
  case Expr::ObjCStringLiteralClass:
  case Expr::ObjCEncodeExprClass:
  case Expr::CXXUuidofExprClass:
    return true;
  case Expr::ObjCBoxedExprClass:
    return cast<ObjCBoxedExpr>(E)->isExpressibleAsConstantInitializer();
  case Expr::CallExprClass: {
    unsigned Builtin = cast<CallExpr>(E)->getBuiltinCallee();
    return (Builtin == Builtin::BI__builtin___CFStringMakeConstantString ||
            Builtin == Builtin::BI__builtin___NSStringMakeConstantString);
  }
  // For GCC compatibility, &&label has static storage duration.
  case Expr::AddrLabelExprClass:
    return true;
  // A Block literal expression may be used as the initialization value for
  // Block variables at global or local static scope.
  case Expr::BlockExprClass:
    return !cast<BlockExpr>(E)->getBlockDecl()->hasCaptures();
  case Expr::ImplicitValueInitExprClass:
    // FIXME:
    // We can never form an lvalue with an implicit value initialization as its
    // base through expression evaluation, so these only appear in one case: the
    // implicit variable declaration we invent when checking whether a constexpr
    // constructor can produce a constant expression. We must assume that such
    // an expression might be a global lvalue.
    return true;
  }
}

EvalEmitter::EvalEmitter(Context &Ctx, Program &P, State &Parent,
                         InterpStack &Stk, APValue &Result)
    : Ctx(Ctx), P(P), S(Parent, P, Stk, Ctx, this), Result(Result) {
  // Create a dummy frame for the interpreter which does not have locals.
  S.Current = new InterpFrame(S, nullptr, nullptr, CodePtr(), Pointer());
}

llvm::Expected<bool> EvalEmitter::interpretExpr(const Expr *E) {
  if (this->visitExpr(E))
    return checkValue(E->getType(), E);
  if (BailLocation)
    return llvm::make_error<ByteCodeGenError>(*BailLocation);
  return false;
}

llvm::Expected<bool> EvalEmitter::interpretRValue(const Expr *E) {
  if (this->visitRValue(E))
    return checkValue(E->getType(), E);
  if (BailLocation)
    return llvm::make_error<ByteCodeGenError>(*BailLocation);
  return false;
}

llvm::Expected<bool> EvalEmitter::interpretDecl(const VarDecl *VD) {
  if (this->visitDecl(VD))
    return checkValue(VD->getType(), VD);
  if (BailLocation)
    return llvm::make_error<ByteCodeGenError>(*BailLocation);
  return false;
}

void EvalEmitter::emitLabel(LabelTy Label) { CurrentLabel = Label; }

EvalEmitter::LabelTy EvalEmitter::getLabel() { return NextLabel++; }

Scope::Local EvalEmitter::createLocal(Descriptor *D) {
  // Allocate memory for a local.
  auto Memory = std::make_unique<char[]>(sizeof(Block) + D->getAllocSize());
  auto *B = new (Memory.get()) Block(D, /*isStatic=*/false);
  B->invokeCtor();

  // Register the local.
  unsigned Off = Locals.size();
  Locals.insert({Off, std::move(Memory)});
  return {Off, D};
}

bool EvalEmitter::bail(const SourceLocation &Loc) {
  if (!BailLocation)
    BailLocation = Loc;
  return false;
}

bool EvalEmitter::jumpTrue(const LabelTy &Label) {
  if (isActive()) {
    if (S.Stk.pop<bool>())
      ActiveLabel = Label;
  }
  return true;
}

bool EvalEmitter::jumpFalse(const LabelTy &Label) {
  if (isActive()) {
    if (!S.Stk.pop<bool>())
      ActiveLabel = Label;
  }
  return true;
}

bool EvalEmitter::jump(const LabelTy &Label) {
  if (isActive())
    CurrentLabel = ActiveLabel = Label;
  return true;
}

bool EvalEmitter::fallthrough(const LabelTy &Label) {
  if (isActive())
    ActiveLabel = Label;
  CurrentLabel = Label;
  return true;
}

bool EvalEmitter::emitInvoke(Function *Fn, const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;

  Pointer This = S.Stk.pop<Pointer>();
  if (!S.CheckInvoke(OpPC, This))
    return false;

  // Interpret the method in its own frame.
  return executeCall(Fn, std::move(This), Info);
}

bool EvalEmitter::emitCall(Function *Fn, const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;

  // Interpret the function in its own frame.
  return executeCall(Fn, Pointer(), Info);
}

bool EvalEmitter::emitVirtualInvoke(const CXXMethodDecl *MD,
                                    const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;
  // Get the this pointer, ensure it's live.
  Pointer This = S.Stk.pop<Pointer>();
  if (const BlockPointer *ThisBlock = S.CheckObject(OpPC, This)) {
    // Dispatch to the virtual method and find the base.
    BlockPointer Base(*ThisBlock);
    const CXXMethodDecl *Override = VirtualLookup(Base, MD);
    if (!S.CheckPure(OpPC, Override))
      return false;

    if (Expected<Function *> Func = P.getOrCreateFunction(S, Override)) {
      if (*Func)
        return executeCall(*Func, std::move(Base), Info);
    } else {
      consumeError(Func.takeError());
    }
    return executeNoCall(Override, Info);
  }
  return false;
}

template <PrimType OpType> bool EvalEmitter::emitRet(const SourceInfo &Info) {
  if (!isActive())
    return true;
  using T = typename PrimConv<OpType>::T;
  return S.Stk.pop<T>().toAPValue(S.getASTCtx(), Result);
}

bool EvalEmitter::emitRetVoid(const SourceInfo &Info) { return true; }

bool EvalEmitter::emitRetCons(const SourceInfo &Info) { return true; }

bool EvalEmitter::emitRetValue(const SourceInfo &Info) {
  // Return the composite type.
  return PointerToValue(S.getASTCtx(), S.Stk.pop<Pointer>(), Result);
}

bool EvalEmitter::emitGetPtrLocal(uint32_t I, const SourceInfo &Info) {
  if (!isActive())
    return true;
  auto It = Locals.find(I);
  assert(It != Locals.end() && "Missing local variable");
  S.Stk.push<Pointer>(reinterpret_cast<Block *>(It->second.get()));
  return true;
}

template <PrimType OpType>
bool EvalEmitter::emitGetLocal(uint32_t I, const SourceInfo &Info) {
  if (!isActive())
    return true;

  using T = typename PrimConv<OpType>::T;

  auto It = Locals.find(I);
  assert(It != Locals.end() && "Missing local variable");
  auto *B = reinterpret_cast<Block *>(It->second.get());
  S.Stk.push<T>(*reinterpret_cast<T *>(B + 1));
  return true;
}

template <PrimType OpType>
bool EvalEmitter::emitSetLocal(uint32_t I, const SourceInfo &Info) {
  if (!isActive())
    return true;

  using T = typename PrimConv<OpType>::T;

  auto It = Locals.find(I);
  assert(It != Locals.end() && "Missing local variable");
  auto *B = reinterpret_cast<Block *>(It->second.get());
  *reinterpret_cast<T *>(B + 1) = S.Stk.pop<T>();
  return true;
}

bool EvalEmitter::emitDestroy(uint32_t I, const SourceInfo &Info) {
  if (!isActive())
    return true;

  for (auto &Local : Descriptors[I]) {
    auto It = Locals.find(Local.Offset);
    assert(It != Locals.end() && "Missing local variable");
    S.deallocate(reinterpret_cast<Block *>(It->second.get()));
  }

  return true;
}

bool EvalEmitter::emitNoCall(const FunctionDecl *F, const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;
  return executeNoCall(F, Info);
}

bool EvalEmitter::emitNoInvoke(const CXXMethodDecl *F, const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;
  return executeNoCall(F, Info);
}

bool EvalEmitter::emitIndirectCall(const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;

  const FnPointer &FnPtr = S.Stk.pop<FnPointer>();
  if (!FnPtr.isValid())
    return false;
  if (FnPtr.isWeak() || FnPtr.isZero()) {
    S.FFDiag(Info);
    return false;
  }
  if (Function *F = FnPtr.asFunction())
    return executeCall(F, Pointer(), Info);

  const FunctionDecl *Decl = FnPtr.asFunctionDecl();
  if (Expected<Function *> Func = S.P.getOrCreateFunction(S, Decl)) {
    if (*Func)
      return executeCall(*Func, Pointer(), Info);
  } else {
    consumeError(Func.takeError());
  }
  return executeNoCall(Decl, Info);
}

bool EvalEmitter::emitIndirectInvoke(const SourceInfo &Info) {
  CurrentSource = Info;
  if (!isActive())
    return true;

  // Fetch and validate the field pointer.
  const MemberPointer &Field = S.Stk.pop<MemberPointer>();
  if (Field.isZero()) {
    S.FFDiag(Info);
    return false;
  }

  // Validate the 'this' pointer.
  const Pointer &This = S.Stk.pop<Pointer>();
  if (const BlockPointer *ThisBlock = S.CheckObject(OpPC, This)) {
    // Fetch a pointer to the member function.
    BlockPointer Base(*ThisBlock);
    const CXXMethodDecl *Method = IndirectLookup(Base, Field);
    if (!Method)
      return false;
    if (!S.CheckPure(OpPC, Method))
      return false;

    // Execute the function.
    if (Expected<Function *> Func = S.P.getOrCreateFunction(S, Method)) {
      if (*Func)
        return executeCall(*Func, std::move(Base), Info);
    } else {
      consumeError(Func.takeError());
    }
    return executeNoCall(Method, Info);
  }
  return false;
}

bool EvalEmitter::executeCall(Function *F, Pointer &&This,
                              const SourceInfo &Info) {
  if (!S.CheckCallable(OpPC, F->getDecl()))
    return false;
  if (S.checkingPotentialConstantExpression())
    return false;
  S.Current = new InterpFrame(S, F, S.Current, OpPC, std::move(This));
  return Interpret(S, Result);
}

bool EvalEmitter::executeNoCall(const FunctionDecl *F, const SourceInfo &Info) {
  if (!S.CheckCallable(OpPC, F))
    return false;
  if (S.checkingPotentialConstantExpression())
    return false;
  if (S.getLangOpts().CPlusPlus11) {
    S.FFDiag(Info, diag::note_constexpr_invalid_function, 1)
        << F->isConstexpr() << (bool)dyn_cast<CXXConstructorDecl>(F) << F;
    S.Note(F->getLocation(), diag::note_declared_at);
  } else {
    S.FFDiag(Info, diag::note_invalid_subexpr_in_const_expr);
  }
  return false;
}

bool EvalEmitter::checkValue(QualType Ty, const SourceInfo &Info) {
  if (Result.isLValue()) {
    bool IsReferenceType = Ty->isReferenceType();
    APValue::LValueBase Base = Result.getLValueBase();

    // Check that the object is a global.
    if (!IsGlobalLValue(Base)) {
      if (Ctx.isCPlusPlus11()) {
        if (const ValueDecl *VD = Base.dyn_cast<const ValueDecl*>()) {
          S.FFDiag(Info, diag::note_constexpr_non_global, 1)
            << IsReferenceType << !Result.getLValuePath().empty()
            << true << VD;
          S.Note(VD->getLocation(), diag::note_declared_at);
          return false;
        }

        if (const Expr *E = Base.dyn_cast<const Expr*>()) {
          S.FFDiag(Info, diag::note_constexpr_non_global, 1)
            << IsReferenceType << !Result.getLValuePath().empty()
            << false << static_cast<const ValueDecl *>(nullptr);
          S.Note(E->getExprLoc(), diag::note_constexpr_temporary_here);
          return false;
        }

        llvm_unreachable("invalid lvalue type");
      } else {
        S.FFDiag(Info);
      }
      // Don't allow references to temporaries to escape.
      return false;
    }

    // TODO: more checks here
  }

  return true;
}

//===----------------------------------------------------------------------===//
// Opcode evaluators
//===----------------------------------------------------------------------===//

#define GET_EVAL_IMPL
#include "Opcodes.inc"
#undef GET_EVAL_IMPL
