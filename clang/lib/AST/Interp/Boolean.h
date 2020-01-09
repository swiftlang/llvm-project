//===--- Boolean.h - Wrapper for boolean types for the VM -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_BOOLEAN_H
#define LLVM_CLANG_AST_INTERP_BOOLEAN_H

#include "CmpResult.h"
#include "clang/AST/APValue.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <cstddef>
#include <cstdint>

namespace clang {
namespace interp {
class State;
class CodePtr;

template <unsigned Bits, bool Signed> class Integral;
template <bool Signed> class FixedIntegral;

/// Wrapper around boolean types.
class Boolean {
private:
  /// Underlying boolean.
  bool V;

  /// Construct a wrapper from a boolean.
  explicit Boolean(bool V) : V(V) {}

public:
  /// Zero-initializes a boolean.
  Boolean() : V(false) {}

  /// Initializes a boolean from an APSInt.
  explicit Boolean(const llvm::APSInt &V) : V(V != 0) {}

  bool operator<(Boolean RHS) const { return V < RHS.V; }
  bool operator>(Boolean RHS) const { return V > RHS.V; }
  bool operator<=(Boolean RHS) const { return V <= RHS.V; }
  bool operator>=(Boolean RHS) const { return V >= RHS.V; }
  bool operator==(Boolean RHS) const { return V == RHS.V; }
  bool operator!=(Boolean RHS) const { return V != RHS.V; }

  bool operator>(unsigned RHS) const { return static_cast<unsigned>(V) > RHS; }

  Boolean operator+(Boolean RHS) const { return Boolean(V | RHS.V); }
  Boolean operator-(Boolean RHS) const { return Boolean(V ^ RHS.V); }
  Boolean operator*(Boolean RHS) const { return Boolean(V & RHS.V); }
  Boolean operator/(Boolean RHS) const { return Boolean(V); }
  Boolean operator%(Boolean RHS) const { return Boolean(V % RHS.V); }
  Boolean operator&(Boolean RHS) const { return Boolean(V & RHS.V); }
  Boolean operator|(Boolean RHS) const { return Boolean(V | RHS.V); }
  Boolean operator^(Boolean RHS) const { return Boolean(V ^ RHS.V); }

  Boolean operator-() const { return *this; }
  Boolean operator~() const { return Boolean(true); }

  Boolean operator>>(unsigned RHS) const { return Boolean(V && RHS == 0); }
  Boolean operator<<(unsigned RHS) const { return *this; }

  explicit operator unsigned() const { return V; }
  explicit operator int64_t() const { return V; }
  explicit operator uint64_t() const { return V; }

  llvm::APSInt toAPSInt() const {
    return llvm::APSInt(llvm::APInt(1, static_cast<uint64_t>(V), false), true);
  }
  llvm::APSInt toAPSInt(unsigned NumBits) const {
    return llvm::APSInt(toAPSInt().zextOrTrunc(NumBits), true);
  }

  bool toAPValue(const ASTContext &Ctx, APValue &Result) const {
    Result = APValue(toAPSInt());
    return true;
  }

  Boolean toUnsigned() const { return *this; }

  constexpr static unsigned bitWidth() { return true; }
  bool isZero() const { return !V; }
  bool isNonZero() const { return V; }
  bool convertsToBool() const { return true; }

  bool isTrue() const { return !isZero(); }
  bool isFalse() const { return isZero(); }
  bool isMin() const { return isZero(); }

  constexpr static bool isMinusOne() { return false; }

  constexpr static bool isSigned() { return false; }

  constexpr static bool isNegative() { return false; }
  constexpr static bool isPositive() { return !isNegative(); }

  CmpResult compare(const Boolean &RHS) const {
    if (!V && RHS.V)
      return CmpResult::Less;
    if (V && !RHS.V)
      return CmpResult::Greater;
    return CmpResult::Equal;
  }

  unsigned countLeadingZeros() const { return V ? 0 : 1; }

  Boolean truncate(unsigned TruncBits) const { return *this; }

  void print(llvm::raw_ostream &OS) const { OS << (V ? "true" : "false"); }

  static Boolean min(unsigned NumBits) { return Boolean(false); }
  static Boolean max(unsigned NumBits) { return Boolean(true); }

  template <typename T>
  static std::enable_if_t<std::is_integral<T>::value, Boolean> from(T Value) {
    return Boolean(Value != 0);
  }

  template <unsigned SrcBits, bool SrcSign>
  static Boolean from(const Integral<SrcBits, SrcSign> &Value);

  template <bool SrcSign> static Boolean from(const FixedIntegral<SrcSign> &I);

  static Boolean from(Boolean Value) { return Value; }

  static Boolean zero() { return Boolean(false); }

  template <typename T> static Boolean from(T Value, unsigned NumBits) {
    return Boolean(Value);
  }

  static bool inRange(int64_t Value, unsigned NumBits) {
    return Value == 0 || Value == 1;
  }

  static bool increment(Boolean A, Boolean *R) {
    *R = Boolean(true);
    return false;
  }

  static bool decrement(Boolean A, Boolean *R) {
    llvm_unreachable("Cannot decrement booleans");
  }

  static bool add(Boolean A, Boolean B, unsigned OpBits, Boolean *R) {
    *R = Boolean(A.V | B.V);
    return false;
  }

  static bool sub(Boolean A, Boolean B, unsigned OpBits, Boolean *R) {
    *R = Boolean(A.V ^ B.V);
    return false;
  }

  static bool mul(Boolean A, Boolean B, unsigned OpBits, Boolean *R) {
    *R = Boolean(A.V & B.V);
    return false;
  }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const Boolean &B) {
  B.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
