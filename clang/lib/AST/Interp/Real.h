//===--- Real.h - Wrapper for floating types for the VM ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the VM types and helpers operating on types.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_REAL_H
#define LLVM_CLANG_AST_INTERP_REAL_H

#include <cstddef>
#include <cstdint>
#include "Integral.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace interp {

using APFloat = llvm::APFloat;
using fltSemantics = llvm::fltSemantics;
using RoundingMode = APFloat::roundingMode;

class Real {
  APFloat V;

public:
  Real() : V(llvm::APFloat::Bogus(), 0) {}

  explicit Real(const APFloat &V) : V(V) {}

  APFloat toAPFloat() const { return V; }

  bool toAPValue(const ASTContext &Ctx, APValue &Result) const {
    Result = APValue(toAPFloat());
    return true;
  }

  void print(llvm::raw_ostream &OS) const { V.print(OS); }

  bool isZero() const { return V.isZero(); }
  bool isNonZero() const { return !V.isZero(); }
  bool convertsToBool() const { return true; }
  bool isNegative() const { return V.isNegative(); }

  bool isNaN() const { return V.isNaN(); }

  CmpResult compare(const Real &RHS) const {
    switch (V.compare(RHS.V)) {
    case APFloat::cmpEqual:
      return CmpResult::Equal;
    case APFloat::cmpLessThan:
      return CmpResult::Less;
    case APFloat::cmpGreaterThan:
      return CmpResult::Greater;
    case APFloat::cmpUnordered:
      return CmpResult::Unordered;
    }
    llvm_unreachable("invalid comparison result");
  }

  template <unsigned Bits, bool Sign>
  static Real from(const Integral<Bits, Sign> &I, const fltSemantics &S) {
    APFloat Result(S, 1);
    Result.convertFromAPInt(I.toAPSInt(), I.isSigned(),
                            APFloat::rmNearestTiesToEven);
    return Real(Result);
  }

  template <bool Sign>
  static Real from(const FixedIntegral<Sign> &I, const llvm::fltSemantics &S) {
    APFloat Result(S, 1);
    Result.convertFromAPInt(I.toAPSInt(), I.isSigned(),
                            APFloat::rmNearestTiesToEven);
    return Real(Result);
  }

  static Real from(const APFloat &F, const llvm::fltSemantics &S) {
    return Real(F);
  }

  static Real from(const Real &F, const llvm::fltSemantics &S) {
    APFloat Result = F.V;
    bool Ignored;
    Result.convert(S, APFloat::rmNearestTiesToEven, &Ignored);
    return Real(Result);
  }

  static Real from(const Boolean &B, const llvm::fltSemantics &S) {
    return Real(APFloat(S, APInt(1u, static_cast<uint64_t>(!B.isZero()))));
  }

  static Real zero(const llvm::fltSemantics &S) {
    return Real(APFloat::getZero(S));
  }

  static Real increment(const Real &A) {
    APFloat R = A.V;
    R.add(APFloat(R.getSemantics(), 1), APFloat::rmNearestTiesToEven);
    return Real(R);
  }

  static Real decrement(const Real &A) {
    APFloat R = A.V;
    R.subtract(APFloat(R.getSemantics(), 1), APFloat::rmNearestTiesToEven);
    return Real(R);
  }

  static Real add(const Real &LHS, const Real &RHS, APFloat::roundingMode M) {
    APFloat Result = LHS.V;
    Result.add(RHS.V, M);
    return Real(Result);
  }

  static Real sub(const Real &LHS, const Real &RHS, APFloat::roundingMode M) {
    APFloat Result = LHS.V;
    Result.subtract(RHS.V, M);
    return Real(Result);
  }

  static Real mul(const Real &LHS, const Real &RHS, APFloat::roundingMode M) {
    APFloat Result = LHS.V;
    Result.multiply(RHS.V, M);
    return Real(Result);
  }

  static Real div(const Real &LHS, const Real &RHS, APFloat::roundingMode M) {
    APFloat Result = LHS.V;
    Result.divide(RHS.V, M);
    return Real(Result);
  }

  static Real negate(const Real &Arg) {
    APFloat Result = Arg.V;
    Result.changeSign();
    return Real(Result);
  }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const Real &F) {
  F.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
