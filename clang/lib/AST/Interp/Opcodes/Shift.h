//===--- Shift.h - Bitwise shift instructions for the VM --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of shift left and right operations.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_SHIFT_H
#define LLVM_CLANG_AST_INTERP_OPCODES_SHIFT_H

namespace detail {

template <PrimType TR, PrimType TL, class T = typename PrimConv<TR>::T>
unsigned Trunc(InterpState &S, CodePtr OpPC, unsigned Bits, const T &V) {
  // C++11 [expr.shift]p1: Shift width must be less than the bit width of
  // the shifted type.
  if (Bits > 1 && V >= T::from(Bits, V.bitWidth())) {
    const Expr *E = S.getSource(OpPC).asExpr();
    const APSInt Val = V.toAPSInt();
    QualType Ty = E->getType();
    S.CCEDiag(E, diag::note_constexpr_large_shift) << Val << Ty << Bits;
    return Bits - 1;
  } else {
    return static_cast<unsigned>(V);
  }
}

template <PrimType TL, PrimType TR, typename T = typename PrimConv<TL>::T>
inline bool ShiftRight(InterpState &S, CodePtr OpPC, const T &V, unsigned RHS) {
  if (RHS >= V.bitWidth()) {
    S.Stk.push<T>(T::from(0, V.bitWidth()));
  } else {
    S.Stk.push<T>(T::from(V >> RHS, V.bitWidth()));
  }
  return true;
}

template <PrimType TL, PrimType TR, typename T = typename PrimConv<TL>::T>
inline bool ShiftLeft(InterpState &S, CodePtr OpPC, const T &V, unsigned RHS) {
  if (V.isSigned() && !S.getLangOpts().CPlusPlus2a) {
    // C++11 [expr.shift]p2: A signed left shift must have a non-negative
    // operand, and must not overflow the corresponding unsigned type.
    // C++2a [expr.shift]p2: E1 << E2 is the unique value congruent to
    // E1 x 2^E2 module 2^N.
    if (V.isNegative()) {
      const Expr *E = S.getSource(OpPC).asExpr();
      S.CCEDiag(E, diag::note_constexpr_lshift_of_negative) << V.toAPSInt();
    } else if (V.countLeadingZeros() < RHS) {
      const Expr *E = S.getSource(OpPC).asExpr();
      S.CCEDiag(E, diag::note_constexpr_lshift_discards);
    }
  }

  if (V.bitWidth() == 1) {
    S.Stk.push<T>(V);
  } else if (RHS >= V.bitWidth()) {
    S.Stk.push<T>(T::from(0, V.bitWidth()));
  } else {
    S.Stk.push<T>(T::from(V.toUnsigned() << RHS, V.bitWidth()));
  }
  return true;
}

} // namespace detail

template <PrimType TL, PrimType TR>
inline bool Shr(InterpState &S, CodePtr OpPC) {
  using namespace detail;
  const auto &RHS = S.Stk.pop<typename PrimConv<TR>::T>();
  const auto &LHS = S.Stk.pop<typename PrimConv<TL>::T>();
  const unsigned Bits = LHS.bitWidth();

  if (RHS.isSigned() && RHS.isNegative()) {
    const SourceInfo &Loc = S.getSource(OpPC);
    S.CCEDiag(Loc, diag::note_constexpr_negative_shift) << RHS.toAPSInt();
    return ShiftLeft<TL, TR>(S, OpPC, LHS, Trunc<TR, TL>(S, OpPC, Bits, -RHS));
  } else {
    return ShiftRight<TL, TR>(S, OpPC, LHS, Trunc<TR, TL>(S, OpPC, Bits, RHS));
  }
}

template <PrimType TL, PrimType TR>
inline bool Shl(InterpState &S, CodePtr OpPC) {
  using namespace detail;
  const auto &RHS = S.Stk.pop<typename PrimConv<TR>::T>();
  const auto &LHS = S.Stk.pop<typename PrimConv<TL>::T>();
  const unsigned Bits = LHS.bitWidth();

  if (RHS.isSigned() && RHS.isNegative()) {
    const SourceInfo &Loc = S.getSource(OpPC);
    S.CCEDiag(Loc, diag::note_constexpr_negative_shift) << RHS.toAPSInt();
    return ShiftRight<TL, TR>(S, OpPC, LHS, Trunc<TR, TL>(S, OpPC, Bits, -RHS));
  } else {
    return ShiftLeft<TL, TR>(S, OpPC, LHS, Trunc<TR, TL>(S, OpPC, Bits, RHS));
  }
}

template <PrimType TL, PrimType TR>
inline bool ShrTrunc(InterpState &S, CodePtr OpPC) {
  using namespace detail;
  const auto &RHS = S.Stk.pop<typename PrimConv<TR>::T>();
  const auto &LHS = S.Stk.pop<typename PrimConv<TL>::T>();
  const unsigned Bits = LHS.bitWidth();
  // OpenCL 6.3j: shift values are effectively % word size of LHS.
  return ShiftRight<TL, TR>(S, OpPC, LHS, static_cast<unsigned>(RHS) % Bits);
}

template <PrimType TL, PrimType TR>
inline bool ShlTrunc(InterpState &S, CodePtr OpPC) {
  using namespace detail;
  const auto &RHS = S.Stk.pop<typename PrimConv<TR>::T>();
  const auto &LHS = S.Stk.pop<typename PrimConv<TL>::T>();
  const unsigned Bits = LHS.bitWidth();
  // OpenCL 6.3j: shift values are effectively % word size of LHS.
  return ShiftLeft<TL, TR>(S, OpPC, LHS, static_cast<unsigned>(RHS) % Bits);
}

#endif
