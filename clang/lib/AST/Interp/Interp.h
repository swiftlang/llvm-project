//===--- Interp.h - Interpreter for the constexpr VM ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the opcodes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_INTERP_H
#define LLVM_CLANG_AST_INTERP_INTERP_H

#include "Builtin.h"
#include "Function.h"
#include "InterpFrame.h"
#include "InterpHelper.h"
#include "InterpStack.h"
#include "InterpState.h"
#include "Opcode.h"
#include "PrimType.h"
#include "Program.h"
#include "State.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/Support/Endian.h"
#include <limits>
#include <vector>

namespace clang {
namespace interp {

using APInt = llvm::APInt;
using APSInt = llvm::APSInt;

//===----------------------------------------------------------------------===//
// Add, Sub, Mul
//===----------------------------------------------------------------------===//

template <typename T, bool (*OpFW)(T, T, unsigned, T *),
          template <typename U> class OpAP>
bool AddSubMulHelper(InterpState &S, CodePtr OpPC, unsigned Bits, const T &LHS,
                     const T &RHS) {
  // Fast path - add the numbers with fixed width.
  T Result;
  if (!OpFW(LHS, RHS, Bits, &Result)) {
    S.Stk.push<T>(Result);
    return true;
  }

  // If for some reason evaluation continues, use the truncated results.
  S.Stk.push<T>(Result);

  // Slow path - compute the result using another bit of precision.
  APSInt Value = OpAP<APSInt>()(LHS.toAPSInt(Bits), RHS.toAPSInt(Bits));

  // Report undefined behaviour, stopping if required.
  const Expr *E = S.getSource(OpPC).asExpr();
  QualType Type = E->getType();
  if (S.checkingForUndefinedBehavior()) {
    auto Trunc = Value.trunc(Result.bitWidth()).toString(10);
    auto Loc = E->getExprLoc();
    S.report(Loc, diag::warn_integer_constant_overflow) << Trunc << Type;
    return true;
  } else {
    S.CCEDiag(E, diag::note_constexpr_overflow) << Value << Type;
    return S.noteUndefinedBehavior();
  }
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Add(InterpState &S, CodePtr OpPC) {
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  const unsigned Bits = RHS.bitWidth() + 1;
  return AddSubMulHelper<T, T::add, std::plus>(S, OpPC, Bits, LHS, RHS);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Sub(InterpState &S, CodePtr OpPC) {
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  const unsigned Bits = RHS.bitWidth() + 1;
  return AddSubMulHelper<T, T::sub, std::minus>(S, OpPC, Bits, LHS, RHS);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Mul(InterpState &S, CodePtr OpPC) {
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  const unsigned Bits = RHS.bitWidth() * 2;
  return AddSubMulHelper<T, T::mul, std::multiplies>(S, OpPC, Bits, LHS, RHS);
}

template <Real (*Op)(const Real &, const Real &, RoundingMode), RoundingMode M>
bool RealArithHelper(InterpState &S, CodePtr OpPC) {
  const Real &RHS = S.Stk.pop<Real>();
  const Real &LHS = S.Stk.pop<Real>();

  Real Result = Op(LHS, RHS, M);
  S.Stk.push<Real>(Result);
  if (!Result.isNaN())
    return true;

  const SourceInfo &Src = S.getSource(OpPC);
  S.CCEDiag(Src, diag::note_constexpr_float_arithmetic) << Result.isNaN();
  return S.noteUndefinedBehavior();
}

template <> inline bool Add<PT_RealFP>(InterpState &S, CodePtr OpPC) {
  return RealArithHelper<Real::add, APFloat::rmNearestTiesToEven>(S, OpPC);
}

template <> inline bool Sub<PT_RealFP>(InterpState &S, CodePtr OpPC) {
  return RealArithHelper<Real::sub, APFloat::rmNearestTiesToEven>(S, OpPC);
}

template <> inline bool Mul<PT_RealFP>(InterpState &S, CodePtr OpPC) {
  return RealArithHelper<Real::mul, APFloat::rmNearestTiesToEven>(S, OpPC);
}

//===----------------------------------------------------------------------===//
// Div, Rem
//===----------------------------------------------------------------------===//

template <typename T, T (*OpFW)(T, T), typename OpAP>
bool DivRemHelper(InterpState &S, CodePtr OpPC) {
  const T RHS = S.Stk.pop<T>();
  const T LHS = S.Stk.pop<T>();

  // Bail on division by zero.
  if (RHS.isZero()) {
    S.FFDiag(S.getSource(OpPC), diag::note_expr_divide_by_zero);
    return false;
  }

  if (RHS.isSigned()) {
    // (-MAX - 1) / -1 = MAX + 1 overflows.
    if (LHS.isMin() && RHS.isMinusOne()) {
      // Push the truncated value in case the interpreter continues.
      S.Stk.push<T>(T::min(RHS.bitWidth()));

      // Compute the actual value for the diagnostic.
      const size_t Bits = RHS.bitWidth() + 1;
      APSInt Value = OpAP()(LHS.toAPSInt(Bits), RHS.toAPSInt(Bits));
      return S.reportOverflow(S.getSource(OpPC).asExpr(), Value);
    }
  }

  // Safe to execute division here.
  S.Stk.push<T>(OpFW(LHS, RHS));
  return true;
}

template <typename T> T DivFn(T a, T b) { return a / b; }

template <typename T> T RemFn(T a, T b) { return a % b; }

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Div(InterpState &S, CodePtr OpPC) {
  return DivRemHelper<T, DivFn<T>, std::divides<APSInt>>(S, OpPC);
}

template <> inline bool Div<PT_RealFP>(InterpState &S, CodePtr OpPC) {
  const Real RHS = S.Stk.pop<Real>();
  const Real LHS = S.Stk.pop<Real>();

  if (RHS.isZero()) {
    S.CCEDiag(S.getSource(OpPC), diag::note_expr_divide_by_zero);
  }

  const Real Result = Real::div(LHS, RHS, APFloat::rmNearestTiesToEven);
  S.Stk.push<Real>(Result);
  if (!Result.isNaN())
    return true;

  const SourceInfo &Src = S.getSource(OpPC);
  S.CCEDiag(Src) << diag::note_constexpr_float_arithmetic << Result.isNaN();
  return S.noteUndefinedBehavior();
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Rem(InterpState &S, CodePtr OpPC) {
  return DivRemHelper<T, RemFn<T>, std::modulus<APSInt>>(S, OpPC);
}

//===----------------------------------------------------------------------===//
// Minus, Not, LogicalNot, Test
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Minus(InterpState &S, CodePtr OpPC) {
  const T &Arg = S.Stk.pop<T>();

  if (Arg.isSigned() && Arg.isMin()) {
    // Push the truncated value in case the interpreter continues.
    S.Stk.push<T>(T::min(Arg.bitWidth()));

    // Compute the actual value for the diagnostic.
    const size_t Bits = Arg.bitWidth() + 1;
    const APSInt Value = std::negate<APSInt>()(Arg.toAPSInt(Bits));
    return S.reportOverflow(S.getSource(OpPC).asExpr(), Value);
  }

  S.Stk.push<T>(std::negate<T>()(Arg));
  return true;
}

template <> inline bool Minus<PT_RealFP>(InterpState &S, CodePtr OpPC) {
  S.Stk.push<Real>(Real::negate(S.Stk.pop<Real>()));
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Not(InterpState &S, CodePtr OpPC) {
  S.Stk.push<T>(~S.Stk.pop<T>());
  return true;
}

template <PrimType ArgName, PrimType RetName>
bool LogicalNot(InterpState &S, CodePtr OpPC) {
  using ArgT = typename PrimConv<ArgName>::T;
  using RetT = typename PrimConv<RetName>::T;
  const ArgT &Value = S.Stk.pop<ArgT>();
  if (!Value.convertsToBool())
    return false;
  S.Stk.push<RetT>(RetT::from(Value.isZero()));
  return true;
}

template <PrimType ArgName, PrimType RetName>
bool Test(InterpState &S, CodePtr OpPC) {
  using ArgT = typename PrimConv<ArgName>::T;
  using RetT = typename PrimConv<RetName>::T;
  const ArgT &Value = S.Stk.pop<ArgT>();
  if (!Value.convertsToBool())
    return false;
  S.Stk.push<RetT>(RetT::from(Value.isNonZero()));
  return true;
}

//===----------------------------------------------------------------------===//
// Dup, Pop
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Dup(InterpState &S, CodePtr OpPC) {
  S.Stk.push<T>(S.Stk.peek<T>());
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Pop(InterpState &S, CodePtr OpPC) {
  S.Stk.pop<T>();
  return true;
}

//===----------------------------------------------------------------------===//
// Const
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Const(InterpState &S, CodePtr OpPC, const T &Arg) {
  S.Stk.push<T>(Arg);
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool ConstFP(InterpState &S, CodePtr OpPC, uint32_t Bits, uint64_t Value) {
  S.Stk.push<T>(T::from(Value, Bits));
  return true;
}

inline bool ConstRealFP(InterpState &S, CodePtr OpPC,
                        const FloatingLiteral *Value) {
  S.Stk.push<Real>(Real::from(Value->getValue(), Value->getSemantics()));
  return true;
}

//===----------------------------------------------------------------------===//
// ConstFn, ConstNoFn, ConstMem
//===----------------------------------------------------------------------===//

inline bool ConstFn(InterpState &S, CodePtr OpPC, Function *Fn) {
  S.Stk.push<FnPointer>(Fn);
  return true;
}

inline bool ConstNoFn(InterpState &S, CodePtr OpPC, const FunctionDecl *FD) {
  // TODO: try to compile and rewrite opcode.
  S.Stk.push<FnPointer>(FD);
  return true;
}

inline bool ConstMem(InterpState &S, CodePtr OpPC, const ValueDecl *VD) {
  S.Stk.push<MemberPointer>(VD);
  return true;
}

inline bool ConstObjCBlock(InterpState &S, CodePtr OpPC, const BlockExpr *E) {
  S.Stk.push<ObjCBlockPointer>(E);
  return true;
}

inline bool ConstAddrLabel(InterpState &S, CodePtr OpPC,
                           const AddrLabelExpr *E) {
  S.Stk.push<VoidPointer>(E);
  return true;
}

//===----------------------------------------------------------------------===//
// GetPtr Local/Param/Global/Field/This
//===----------------------------------------------------------------------===//

inline bool GetPtrLocal(InterpState &S, CodePtr OpPC, uint32_t I) {
  S.Stk.push<Pointer>(S.Current->getLocalPointer(I));
  return true;
}

inline bool GetPtrParam(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression()) {
    return false;
  }
  S.Stk.push<Pointer>(S.Current->getParamPointer(I));
  return true;
}

inline bool GetPtrGlobal(InterpState &S, CodePtr OpPC, GlobalLocation G) {
  S.Stk.push<Pointer>(S.P.getPtrGlobal(G));
  return true;
}

inline bool GetDummyPtr(InterpState &S, CodePtr OpPC, const ValueDecl *VD,
                        Descriptor *D) {
  S.Stk.push<Pointer>(D, VD);
  return true;
}

inline bool GetDummyVoidPtr(InterpState &S, CodePtr OpPC, const ValueDecl *VD,
                            Descriptor *D) {
  S.Stk.push<VoidPointer>(Pointer{D, VD});
  return true;
}

inline bool GetDummyFnPtr(InterpState &S, CodePtr OpPC, const ValueDecl *VD) {
  S.Stk.push<FnPointer>(VD);
  return true;
}

inline bool GetDummyMemberPtr(InterpState &S, CodePtr OpPC,
                              const ValueDecl *VD) {
  S.Stk.push<MemberPointer>(VD, MemberPointer::Dummy{});
  return true;
}

inline bool GetDummyObjCBlockPtr(InterpState &S, CodePtr OpPC,
                                 const ValueDecl *VD) {
  S.Stk.push<ObjCBlockPointer>(VD);
  return true;
}

inline bool GetPtrField(InterpState &S, CodePtr OpPC, uint32_t Off) {
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckField(OpPC, Ptr))
    return false;
  S.Stk.push<Pointer>(Ptr.atField(Off));
  return true;
}

inline bool GetPtrThisField(InterpState &S, CodePtr OpPC, uint32_t Off) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  S.Stk.push<Pointer>(This.atField(Off));
  return true;
}

inline bool GetPtrActiveField(InterpState &S, CodePtr OpPC, uint32_t Off) {
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckField(OpPC, Ptr))
    return false;
  Pointer Field = Ptr.atField(Off);
  Ptr.asBlock()->deactivate();
  Field.asBlock()->activate();
  S.Stk.push<Pointer>(std::move(Field));
  return true;
}

inline bool GetPtrActiveThisField(InterpState &S, CodePtr OpPC, uint32_t Off) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  Pointer Field = This.atField(Off);
  This.asBlock()->deactivate();
  Field.asBlock()->activate();
  S.Stk.push<Pointer>(std::move(Field));
  return true;
}

inline bool GetPtrBase(InterpState &S, CodePtr OpPC, uint32_t Off) {
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Ptr, CSK_Base))
    return false;
  S.Stk.push<Pointer>(Ptr.atBase(Off, /*isVirtual=*/false));
  return true;
}

inline bool GetPtrThisBase(InterpState &S, CodePtr OpPC, uint32_t Off) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  S.Stk.push<Pointer>(This.atBase(Off, /*isVirtual=*/false));
  return true;
}

inline bool VirtBaseHelper(InterpState &S, CodePtr OpPC, const RecordDecl *Decl,
                           const Pointer &Ptr) {
  Pointer Base = Ptr;
  while (Base.isBaseClass())
    Base = Base.getBase();

  auto *Field = Base.getRecord()->getVirtualBase(Decl);
  S.Stk.push<Pointer>(Base.atBase(Field->getOffset(), /*isVirtual=*/true));
  return true;
}

inline bool GetPtrVirtBase(InterpState &S, CodePtr OpPC, const RecordDecl *D) {
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Ptr, CSK_Base))
    return false;
  return VirtBaseHelper(S, OpPC, D, Ptr);
}

inline bool GetPtrThisVirtBase(InterpState &S, CodePtr OpPC,
                               const RecordDecl *D) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  return VirtBaseHelper(S, OpPC, D, S.Current->getThis());
}

inline bool GetPtrFieldIndirect(InterpState &S, CodePtr OpPC) {
  // Fetch a pointer to the member.
  const MemberPointer &Field = S.Stk.pop<MemberPointer>();

  // Fetch and validate the object.
  Pointer Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckField(OpPC, Ptr))
    return false;

  // Validate the pointer.
  if (Field.isZero()) {
    S.FFDiag(S.getSource(OpPC));
    return false;
  }

  // Find the correct class in the hierarchy and fetch the pointer.
  if (auto *Decl = dyn_cast<FieldDecl>(Field.getDecl())) {
    if (Record *R = PointerLookup(Ptr, Field)) {
      S.Stk.push<Pointer>(Ptr.atField(R->getField(Decl)->getOffset()));
      return true;
    }
  }

  S.FFDiag(S.getSource(OpPC));
  return false;
}

//===----------------------------------------------------------------------===//
// AddOffset, SubOffset
//===----------------------------------------------------------------------===//

template <class T, bool Add> bool OffsetHelper(InterpState &S, CodePtr OpPC) {
  // Do nothing if the offset is zero.
  const T &Offset = S.Stk.pop<T>();
  if (Offset.isZero())
    return true;

  // Get the pointer to be adjusted.
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (Ptr.isZero()) {
    const auto &Loc = S.getSource(OpPC);
    S.CCEDiag(Loc, diag::note_constexpr_null_subobject) << CSK_ArrayIndex;
  }

  // Get a version of the index comparable to the type.
  T Index = T::from(Ptr.getIndex(), Offset.bitWidth());
  if (Ptr.isUnknownSize()) {
    S.CCEDiag(S.getSource(OpPC), diag::note_constexpr_unsized_array_indexed);
  } else {
    // Compute the largest index into the array.
    unsigned MaxIndex = Ptr.getNumElems();

    // Helper to report an invalid offset, computed as APSInt.
    auto InvalidOffset = [&]() {
      const unsigned Bits = Offset.bitWidth();
      APSInt APOffset(Offset.toAPSInt().extend(Bits + 2), false);
      APSInt APIndex(Index.toAPSInt().extend(Bits + 2), false);
      APSInt NewIndex = Add ? (APIndex + APOffset) : (APIndex - APOffset);
      S.CCEDiag(S.getSource(OpPC), diag::note_constexpr_array_index)
          << NewIndex << /*array*/ static_cast<int>(!Ptr.inArray())
          << static_cast<unsigned>(MaxIndex);
      return false;
    };

    // If the new offset would be negative, bail out.
    if (Add && Offset.isNegative() && (Offset.isMin() || -Offset > Index))
      return InvalidOffset();
    if (!Add && Offset.isPositive() && Index < Offset)
      return InvalidOffset();

    // If the new offset would be out of bounds, bail out.
    unsigned MaxOffset = MaxIndex - Ptr.getIndex();
    if (Add && Offset.isPositive() && Offset > MaxOffset)
      return InvalidOffset();
    if (!Add && Offset.isNegative() && (Offset.isMin() || -Offset > MaxOffset))
      return InvalidOffset();
  }

  // Offset is valid - compute it on unsigned.
  int64_t WideIndex = static_cast<int64_t>(Index);
  int64_t WideOffset = static_cast<int64_t>(Offset);
  int64_t Result = Add ? (WideIndex + WideOffset) : (WideIndex - WideOffset);
  S.Stk.push<Pointer>(Ptr.atIndex(Result));
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool AddOffset(InterpState &S, CodePtr OpPC) {
  return OffsetHelper<T, true>(S, OpPC);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SubOffset(InterpState &S, CodePtr OpPC) {
  return OffsetHelper<T, false>(S, OpPC);
}

//===----------------------------------------------------------------------===//
// Destroy
//===----------------------------------------------------------------------===//

inline bool Destroy(InterpState &S, CodePtr OpPC, uint32_t I) {
  S.Current->destroy(I);
  return true;
}

//===----------------------------------------------------------------------===//
// Cast, CastFP
//===----------------------------------------------------------------------===//

template <PrimType TFrom, PrimType TTo>
bool Cast(InterpState &S, CodePtr OpPC) {
  using T = typename PrimConv<TFrom>::T;
  using U = typename PrimConv<TTo>::T;
  S.Stk.push<U>(U::from(S.Stk.pop<T>()));
  return true;
}

template <PrimType TIn, PrimType TOut>
bool CastFP(InterpState &S, CodePtr OpPC, uint32_t Bits) {
  using T = typename PrimConv<TIn>::T;
  using U = typename PrimConv<TOut>::T;
  S.Stk.push<U>(U::from(S.Stk.pop<T>(), Bits));
  return true;
}

template <PrimType TIn>
bool CastRealFP(InterpState &S, CodePtr OpPC, const fltSemantics *Sema) {
  using T = typename PrimConv<TIn>::T;
  S.Stk.push<Real>(Real::from(S.Stk.pop<T>(), *Sema));
  return true;
}

template <typename U>
bool RealToIntHelper(InterpState &S, CodePtr OpPC, const APFloat &V,
                     uintptr_t Bits) {
  // Convert to an APSInt.
  APSInt Result(Bits, !U::isSigned());
  bool Ignored;
  llvm::APFloat::opStatus Status =
      V.convertToInteger(Result, llvm::APFloat::rmTowardZero, &Ignored);
  S.Stk.push<U>(Result);

  // Report potential overflow.
  if (Status & APFloat::opInvalidOp) {
    const Expr *E = S.getSource(OpPC).asExpr();
    S.CCEDiag(E, diag::note_constexpr_overflow) << V << E->getType();
    return S.noteUndefinedBehavior();
  }

  return true;
}

template <PrimType TIn, PrimType TOut>
bool CastRealFPToAlu(InterpState &S, CodePtr OpPC) {
  using T = typename PrimConv<TIn>::T;
  using U = typename PrimConv<TOut>::T;
  return RealToIntHelper<U>(S, OpPC, S.Stk.pop<T>().toAPFloat(), U::bitWidth());
}

template <PrimType TIn, PrimType TOut>
bool CastRealFPToAluFP(InterpState &S, CodePtr OpPC, uint32_t Bits) {
  using T = typename PrimConv<TIn>::T;
  using U = typename PrimConv<TOut>::T;
  return RealToIntHelper<U>(S, OpPC, S.Stk.pop<T>().toAPFloat(), Bits);
}

//===----------------------------------------------------------------------===//
// CastMemberToBase, CastMemberToDerived
//===----------------------------------------------------------------------===//

inline bool CastMemberToBase(InterpState &S, CodePtr OpPC,
                             const CXXRecordDecl *R) {
  if (S.Stk.peek<MemberPointer>().toBase(R))
    return true;
  S.FFDiag(S.getSource(OpPC));
  return false;
}

inline bool CastMemberToDerived(InterpState &S, CodePtr OpPC,
                                const CXXRecordDecl *R) {
  if (S.Stk.peek<MemberPointer>().toDerived(R))
    return true;
  S.FFDiag(S.getSource(OpPC));
  return false;
}

//===----------------------------------------------------------------------===//
// CastToDerived
//===----------------------------------------------------------------------===//

inline bool CastToDerived(InterpState &S, CodePtr OpPC, const Expr *E) {
  QualType Ty = E->getType();
  if (const PointerType *PT = Ty->getAs<PointerType>())
    Ty = PT->getPointeeType();

  Pointer Base = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Base, CSK_Derived))
    return false;

  if (auto *R = S.P.getOrCreateRecord(Ty->getAs<RecordType>()->getDecl())) {
    // Traverse the chain of bases (actually derived classes).
    while (Base.isBaseClass()) {
      // Step to deriving class.
      Base = Base.getBase();

      // Found the derived class.
      if (Base.getRecord() == R) {
        S.Stk.push<Pointer>(std::move(Base));
        return true;
      }
    }
  }

  // Emit the diagnostic.
  QualType BaseTy = Base.getFieldType();
  S.CCEDiag(E, diag::note_constexpr_invalid_downcast) << BaseTy << Ty;
  return false;
}

//===----------------------------------------------------------------------===//
// And, Or, Xor
//===----------------------------------------------------------------------===//

template <typename T, template <typename U> class Op>
bool LogicalHelper(InterpState &S, CodePtr OpPC) {
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  S.Stk.push<T>(Op<T>()(LHS, RHS));
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool And(InterpState &S, CodePtr OpPC) {
  return LogicalHelper<T, std::bit_and>(S, OpPC);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Or(InterpState &S, CodePtr OpPC) {
  return LogicalHelper<T, std::bit_or>(S, OpPC);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Xor(InterpState &S, CodePtr OpPC) {
  return LogicalHelper<T, std::bit_xor>(S, OpPC);
}

//===----------------------------------------------------------------------===//
// Builtin
//===----------------------------------------------------------------------===//

inline bool Builtin(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression())
    return false;
  return InterpBuiltin(S, OpPC, I);
}

//===----------------------------------------------------------------------===//
// CheckFunction, CheckMethod
//===----------------------------------------------------------------------===//

inline bool CheckFunction(InterpState &S, CodePtr OpPC,
                          const FunctionDecl *FD) {
  if (!S.CheckCallable(OpPC, FD))
    return false;

  // TODO: rewrite instruction into no-op.
  return true;

}

inline bool CheckMethod(InterpState &S, CodePtr OpPC,
                        const CXXMethodDecl *MD) {
  if (!S.CheckCallable(OpPC, MD))
    return false;

  // TODO: rewrite instruction into no-op.
  return true;
}

//===----------------------------------------------------------------------===//
// Zero, Nullptr
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Zero(InterpState &S, CodePtr OpPC) {
  S.Stk.push<T>(T::zero());
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool ZeroFP(InterpState &S, CodePtr OpPC, uint32_t Bits) {
  S.Stk.push<T>(T::zero(Bits));
  return true;
}

inline bool ZeroRealFP(InterpState &S, CodePtr OpPC, const fltSemantics *Sema) {
  S.Stk.push<Real>(Real::zero(*Sema));
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
inline bool Null(InterpState &S, CodePtr OpPC) {
  S.Stk.push<T>();
  return true;
}

inline bool NullPtrT(InterpState &S, CodePtr OpPC, Descriptor *Desc) {
  S.Stk.push<Pointer>(Desc);
  return true;
}

//===----------------------------------------------------------------------===//
// This, ImplicitThis
//===----------------------------------------------------------------------===//

inline bool This(InterpState &S, CodePtr OpPC) {
  // Cannot read 'this' in this mode.
  if (S.checkingPotentialConstantExpression()) {
    return false;
  }

  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;

  S.Stk.push<Pointer>(This);
  return true;
}

//===----------------------------------------------------------------------===//
// NoRet
//===----------------------------------------------------------------------===//

inline bool NoRet(InterpState &S, CodePtr OpPC) {
  SourceLocation EndLoc = S.Current->getCallee()->getEndLoc();
  S.FFDiag(EndLoc, diag::note_constexpr_no_return);
  return false;
}

//===----------------------------------------------------------------------===//
// DecayPtr
//===----------------------------------------------------------------------===//

inline bool DecayPtr(InterpState &S, CodePtr OpPC) {
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Ptr, CSK_ArrayToPointer))
    return false;
  const Pointer &Decayed = Ptr.decay();
  S.Stk.push<Pointer>(std::move(Decayed));
  return true;
}

//===----------------------------------------------------------------------===//
// PtrDiff
//===----------------------------------------------------------------------===//

template <typename T>
bool PtrDiffHelper(InterpState &S, CodePtr OpPC, unsigned BitWidth) {
  const Pointer &RHS = S.Stk.pop<Pointer>();
  const Pointer &LHS = S.Stk.pop<Pointer>();

  // Null pointers subtract to 0.
  if (auto *PtrLHS = LHS.asTarget()) {
    if (auto *PtrRHS = RHS.asTarget()) {
      auto Diff = PtrLHS->getTargetOffset() - PtrRHS->getTargetOffset();
      S.Stk.push<T>(T::from(Diff.getQuantity(), BitWidth));
      return true;
    }
    S.FFDiag(S.getSource(OpPC).asExpr());
    return false;
  }

  // Pointers must point to the same object.
  if (LHS.isZero() || RHS.isZero()) {
    S.FFDiag(S.getSource(OpPC).asExpr());
    return false;
  }
  if (!LHS.hasSameBase(RHS)) {
    S.FFDiag(S.getSource(OpPC).asExpr());
    return false;
  }

  // Pointers must point to the same array.
  if (!LHS.hasSameArray(RHS)) {
    const Expr *E = S.getSource(OpPC).asExpr();
    S.CCEDiag(E, diag::note_constexpr_pointer_subtraction_not_same_array);
    return false;
  }

  // Compute the difference.
  auto RHSIndex = static_cast<int64_t>(RHS.getIndex());
  auto LHSIndex = static_cast<int64_t>(LHS.getIndex());
  auto Diff = LHSIndex - RHSIndex;
  S.Stk.push<T>(T::from(Diff, BitWidth));

  if (!T::inRange(Diff, BitWidth)) {
    auto APDiff = Integral<64, true>::from(Diff).toAPSInt();
    const Expr *E = S.getSource(OpPC).asExpr();
    S.CCEDiag(E, diag::note_constexpr_overflow) << APDiff << E->getType();
    return S.noteUndefinedBehavior();
  }

  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool PtrDiff(InterpState &S, CodePtr OpPC) {
  return PtrDiffHelper<T>(S, OpPC, T::bitWidth());
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool PtrDiffFP(InterpState &S, CodePtr OpPC, uint32_t Bits) {
  return PtrDiffHelper<T>(S, OpPC, Bits);
}

//===----------------------------------------------------------------------===//
// RealElem, ImagElem
//===----------------------------------------------------------------------===//

inline bool ComplexAccess(InterpState &S, CodePtr OpPC, unsigned Idx,
                          CheckSubobjectKind CSK) {
  const auto &Ptr = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Ptr, CSK))
    return false;
  S.Stk.push<Pointer>(Ptr.atIndex(Idx));
  return true;
}

inline bool RealElem(InterpState &S, CodePtr OpPC) {
  return ComplexAccess(S, OpPC, 0, CSK_Real);
}

inline bool ImagElem(InterpState &S, CodePtr OpPC) {
  return ComplexAccess(S, OpPC, 1, CSK_Imag);
}

//===----------------------------------------------------------------------===//
// TypeInfo
//===----------------------------------------------------------------------===//

inline bool TypeInfo(InterpState &S, CodePtr OpPC, const Type *Ty,
                     const Type *InfoTy) {
  S.Stk.push<Pointer>(Ty, QualType(InfoTy, Qualifiers::Const));
  return true;
}

inline bool TypeInfoPoly(InterpState &S, CodePtr OpPC, const Type *InfoTy) {
  const auto &Ptr = S.Stk.pop<Pointer>();
  if (auto *BlockPtr = S.CheckTypeid(OpPC, Ptr)) {
    // Find the outermost class.
    BlockPointer Base(*BlockPtr);
    while (Base.isBaseClass() && Base.isInitialized())
      Base = Base.getBase();

    // Create a typeid structure to the record.
    const RecordDecl *BaseDecl = Base.getRecord()->getDecl();
    const Type *BaseTy = S.getASTCtx().getRecordType(BaseDecl).getTypePtr();
    S.Stk.push<Pointer>(BaseTy, QualType(InfoTy, Qualifiers::Const));
    return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// InitUnion
//===----------------------------------------------------------------------===//

inline bool InitUnion(InterpState &S, CodePtr OpPC) {
  const Pointer &Dst = S.Current->getThis();
  const Pointer &Src = S.Current->getParam<Pointer>(0);
  if (auto *BlockSrc = S.CheckLoad(OpPC, Src, AK_Read)) {
    if (auto *BlockDst = S.CheckInit(OpPC, Dst)) {
      return CopyUnion(S, OpPC, *BlockSrc, *BlockDst);
    }
  }
  return false;
}

//===----------------------------------------------------------------------===//
// CreateGlobal
//===----------------------------------------------------------------------===//

inline bool CreateGlobal(InterpState &S, CodePtr OpPC, const VarDecl *VD) {
  SmallVector<PartialDiagnosticAt, 8> Notes;
  if (!VD->evaluateValue(Notes)) {
    S.FFDiag(S.getSource(OpPC), diag::note_constexpr_var_init_non_constant,
             Notes.size() + 1) << VD;
    S.Note(VD->getLocation(), diag::note_declared_at);
    S.addNotes(Notes);
    return false;
  } else if (!VD->checkInitIsICE()) {
    S.CCEDiag(S.getSource(OpPC), diag::note_constexpr_var_init_non_constant,
              Notes.size() + 1) << VD;
    S.Note(VD->getLocation(), diag::note_declared_at);
    S.addNotes(Notes);
    return false;
  }
  return true;
}

//===----------------------------------------------------------------------===//
// GetGlobalValue
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetGlobalValue(InterpState &S, CodePtr OpPC, const VarDecl *VD) {
  APValue *Init = VD->getEvaluatedValue();
  if (!Init || !Init->isInt())
    return false;
  S.Stk.push<T>(T::from(Init->getInt().getExtValue()));
  return true;
}

#include "Opcodes/BitCast.h"
#include "Opcodes/Comparison.h"
#include "Opcodes/Getter.h"
#include "Opcodes/IncDec.h"
#include "Opcodes/Setter.h"
#include "Opcodes/Shift.h"
#include "Opcodes/Trap.h"
#include "Opcodes/Temp.h"

} // namespace interp
} // namespace clang

#endif
