//===--- FixedIntegral.h - Wrapper for fixed precision ints -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_FIXEDINTEGRAL_H
#define LLVM_CLANG_AST_INTERP_FIXEDINTEGRAL_H

#include <cstddef>
#include <cstdint>
#include "Boolean.h"
#include "CmpResult.h"
#include "clang/AST/APValue.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace interp {

using APInt = llvm::APInt;
using APSInt = llvm::APSInt;

/// Specialisation for fixed precision integers.
template <bool Signed>
class FixedIntegral {
 private:
  template <bool OtherSigned> friend class FixedIntegral;

  /// Value is stored in an APSInt.
  APSInt V;

 public:
  /// Integral initialized to an undefined value.
  FixedIntegral() : V() {}

  /// Construct an integral from a value based on signedness.
  explicit FixedIntegral(const APSInt &V) : V(V, !Signed) {}

  bool operator<(const FixedIntegral &RHS) const { return V < RHS.V; }
  bool operator>(const FixedIntegral &RHS) const { return V > RHS.V; }
  bool operator<=(const FixedIntegral &RHS) const { return V <= RHS.V; }
  bool operator>=(const FixedIntegral &RHS) const { return V >= RHS.V; }
  bool operator==(const FixedIntegral &RHS) const { return V == RHS.V; }
  bool operator!=(const FixedIntegral &RHS) const { return V != RHS.V; }

  template <typename T>
  bool operator>(T RHS) const {
    constexpr bool ArgSigned = std::is_signed<T>::value;
    return V > APSInt(APInt(V.getBitWidth(), RHS, ArgSigned), !ArgSigned);
  }

  template <typename T>
  bool operator<(T RHS) const {
    constexpr bool ArgSigned = std::is_signed<T>::value;
    return V < APSInt(APInt(V.getBitWidth(), RHS, ArgSigned), !ArgSigned);
  }

  FixedIntegral operator+(FixedIntegral RHS) const {
    return FixedIntegral(V + RHS.V);
  }
  FixedIntegral operator-(FixedIntegral RHS) const {
    return FixedIntegral(V - RHS.V);
  }
  FixedIntegral operator*(FixedIntegral RHS) const {
    return FixedIntegral(V * RHS.V);
  }
  FixedIntegral operator/(FixedIntegral RHS) const {
    return FixedIntegral(V / RHS.V);
  }
  FixedIntegral operator%(FixedIntegral RHS) const {
    return FixedIntegral(V % RHS.V);
  }
  FixedIntegral operator&(FixedIntegral RHS) const {
    return FixedIntegral(V & RHS.V);
  }
  FixedIntegral operator|(FixedIntegral RHS) const {
    return FixedIntegral(V | RHS.V);
  }
  FixedIntegral operator^(FixedIntegral RHS) const {
    return FixedIntegral(V ^ RHS.V);
  }

  FixedIntegral operator-() const { return FixedIntegral(-V); }
  FixedIntegral operator~() const { return FixedIntegral(~V); }

  FixedIntegral operator>>(unsigned RHS) const {
    return FixedIntegral(V >> RHS);
  }
  FixedIntegral operator<<(unsigned RHS) const {
    return FixedIntegral(V << RHS);
  }

  explicit operator off_t() const {
    return Signed ? V.getSExtValue() : V.getZExtValue();
  }
  explicit operator unsigned() const {
    return Signed ? V.getZExtValue() : V.getZExtValue();
  }
  explicit operator uint64_t() const {
    return Signed ? V.getZExtValue() : V.getZExtValue();
  }

  APSInt toAPSInt() const { return V; }
  APSInt toAPSInt(unsigned NumBits) const {
    if (Signed)
      return APSInt(toAPSInt().sextOrTrunc(NumBits), !Signed);
    else
      return APSInt(toAPSInt().zextOrTrunc(NumBits), !Signed);
  }

  bool toAPValue(const ASTContext &Ctx, APValue &Result) const {
    Result = APValue(toAPSInt());
    return true;
  }

  FixedIntegral<false> toUnsigned() const {
    return FixedIntegral<false>::from(*this, bitWidth());
  }

  unsigned bitWidth() const { return V.getBitWidth(); }

  bool isZero() const { return V.isNullValue(); }
  bool isNonZero() const { return !V.isNullValue(); }
  bool convertsToBool() const { return true; }

  bool isMin() const {
    if (Signed)
      return V.isMinSignedValue();
    else
      return V.isMinValue();
  }

  bool isMinusOne() const { return Signed && V.isAllOnesValue(); }

  constexpr static bool isSigned() { return Signed; }

  bool isNegative() const { return V.isNegative(); }
  bool isPositive() const { return !isNegative(); }

  CmpResult compare(const FixedIntegral &RHS) const {
    if (V < RHS.V) return CmpResult::Less;
    if (V > RHS.V) return CmpResult::Greater;
    return CmpResult::Equal;
  }

  unsigned countLeadingZeros() const { return V.countLeadingZeros(); }

  FixedIntegral truncate(unsigned TruncBits) const {
    if (TruncBits >= bitWidth()) return *this;
    return FixedIntegral(V.trunc(TruncBits).extend(bitWidth()));
  }

  void print(llvm::raw_ostream &OS) const { OS << V; }

  static FixedIntegral min(unsigned NumBits) {
    if (Signed)
      return FixedIntegral(APSInt(APInt::getSignedMinValue(NumBits), !Signed));
    else
      return FixedIntegral(APSInt(APInt::getMinValue(NumBits), !Signed));
  }

  static FixedIntegral max(unsigned NumBits) {
    if (Signed)
      return FixedIntegral(APSInt(APInt::getSignedMaxValue(NumBits), !Signed));
    else
      return FixedIntegral(APSInt(APInt::getMaxValue(NumBits), !Signed));
  }

  template <typename T>
  static
      typename std::enable_if<std::is_integral<T>::value, FixedIntegral>::type
      from(T Value, unsigned NumBits) {
    const bool SrcSign = std::is_signed<T>::value;
    APInt IntVal(NumBits, static_cast<uint64_t>(Value), SrcSign);
    return FixedIntegral(APSInt(IntVal, !SrcSign));
  }

  template <unsigned SrcBits, bool SrcSigned>
  static FixedIntegral from(const Integral<SrcBits, SrcSigned> &I,
                            unsigned NumBits);

  template <bool SrcSign>
  static FixedIntegral from(FixedIntegral<SrcSign> Value, unsigned NumBits) {
    if (SrcSign)
      return FixedIntegral(APSInt(Value.V.sextOrTrunc(NumBits), !Signed));
    else
      return FixedIntegral(APSInt(Value.V.zextOrTrunc(NumBits), !Signed));
  }

  static FixedIntegral from(Boolean Value, unsigned NumBits) {
    return FixedIntegral(
        APSInt(NumBits, static_cast<uint64_t>(!Value.isZero())));
  }

  static FixedIntegral zero(unsigned NumBits) { return from(0, NumBits); }

  static bool inRange(int64_t Value, unsigned NumBits) {
    APSInt V(APInt(NumBits, Value, true), false);
    return min(NumBits).V <= V && V <= max(NumBits).V;
  }

  static bool increment(FixedIntegral A, FixedIntegral *R) {
    *R = A + FixedIntegral::from(1, A.bitWidth());
    return A.isSigned() && !A.isNegative() && R->isNegative();
  }

  static bool decrement(FixedIntegral A, FixedIntegral *R) {
    *R = A - FixedIntegral::from(1, A.bitWidth());
    return !A.isSigned() && A.isNegative() && !R->isNegative();
  }

  static bool add(FixedIntegral A, FixedIntegral B, unsigned OpBits,
                  FixedIntegral *R) {
    APSInt Value(A.V.extend(OpBits) + B.V.extend(OpBits));
    *R = FixedIntegral(Value.trunc(A.bitWidth()));
    return R->V.extend(OpBits) != Value;
  }

  static bool sub(FixedIntegral A, FixedIntegral B, unsigned OpBits,
                  FixedIntegral *R) {
    APSInt Value(A.V.extend(OpBits) - B.V.extend(OpBits));
    *R = FixedIntegral(Value.trunc(A.bitWidth()));
    return R->V.extend(OpBits) != Value;
  }

  static bool mul(FixedIntegral A, FixedIntegral B, unsigned OpBits,
                  FixedIntegral *R) {
    APSInt Value(A.V.extend(OpBits) * B.V.extend(OpBits));
    *R = FixedIntegral(Value.trunc(A.bitWidth()));
    return R->V.extend(OpBits) != Value;
  }
};

template <bool Signed>
llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, FixedIntegral<Signed> I) {
  I.print(OS);
  return OS;
}

}  // namespace interp
}  // namespace clang

#endif
