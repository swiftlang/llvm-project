//===--- Comparison.h - Comparison instructions for the VM ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the comparison opcodes: EQ, NE, GT, GE, LT, LE, InRange
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_COMPARISON_H
#define LLVM_CLANG_AST_INTERP_OPCODES_COMPARISON_H

namespace detail {

inline bool CheckVirtual(InterpState &S, CodePtr PC, const MemberPointer &Ptr) {
  if (auto *MD = dyn_cast<CXXMethodDecl>(Ptr.getDecl())) {
    if (MD->isVirtual()) {
      const SourceInfo &Loc = S.getSource(PC);
      S.CCEDiag(Loc, diag::note_constexpr_compare_virtual_mem_ptr) << MD;
      return false;
    }
  }
  return true;
}

inline llvm::Optional<CmpResult> CmpPointerOrdered(InterpState &S, CodePtr PC,
                                                   const Pointer &LHS,
                                                   const Pointer &RHS) {
  if (!LHS.hasSameBase(RHS)) {
    const SourceInfo &Loc = S.getSource(PC);
    S.FFDiag(Loc, diag::note_constexpr_pointer_comparison_unspecified);
    return {};
  } else {
    return LHS.compare(RHS);
  }
}

inline llvm::Optional<CmpResult> CmpPointerOrdered(InterpState &S, CodePtr PC,
                                                   const MemberPointer &LHS,
                                                   const MemberPointer &RHS) {
  if (!CheckVirtual(S, PC, LHS) || !CheckVirtual(S, PC, RHS))
    return {};
  return LHS == RHS ? CmpResult::Equal : CmpResult::Unequal;
}

template <typename T>
llvm::Optional<CmpResult> CmpPointerOrdered(InterpState &S, CodePtr PC,
                                            const T &LHS, const T &RHS) {
  return LHS == RHS ? CmpResult::Equal : CmpResult::Unequal;
}

template <PrimType Name, typename T = typename PrimConv<Name>::T>
typename std::enable_if<!isPointer(Name), llvm::Optional<CmpResult>>::type
CmpOrdered(InterpState &S, CodePtr PC, const T &LHS, const T &RHS) {
  return LHS.compare(RHS);
}

template <PrimType Name, typename T = typename PrimConv<Name>::T>
typename std::enable_if<isPointer(Name), llvm::Optional<CmpResult>>::type
CmpOrdered(InterpState &S, CodePtr PC, const T &LHS, const T &RHS) {
  // Do not allow comparisons of weak pointers.
  if (LHS.isWeak() || RHS.isWeak())
    return {};

  // Handle comparisons to null pointers.
  const bool LHSZero = LHS.isZero();
  const bool RHSZero = RHS.isZero();
  if (LHSZero && RHSZero)
    return CmpResult::Equal;

  if (LHSZero || RHSZero) {
    const SourceInfo &Loc = S.getSource(PC);
    S.FFDiag(Loc, diag::note_constexpr_pointer_comparison_unspecified);
    return {};
  }

  // Handle general ordering - pointers must have same base.
  return CmpPointerOrdered(S, PC, LHS, RHS);
}

inline llvm::Optional<CmpResult> CmpPointerEquality(InterpState &S, CodePtr PC,
                                                    const Pointer &LHS,
                                                    const Pointer &RHS) {
  if (!LHS.hasSameBase(RHS) && (LHS.isLiteral() || RHS.isLiteral())) {
    S.FFDiag(S.getSource(PC), diag::note_invalid_subexpr_in_const_expr);
    return {};
  }
  return LHS == RHS ? CmpResult::Equal : CmpResult::Unequal;
}

inline llvm::Optional<CmpResult> CmpPointerEquality(InterpState &S, CodePtr PC,
                                                    const MemberPointer &LHS,
                                                    const MemberPointer &RHS) {
  if (!CheckVirtual(S, PC, LHS) || !CheckVirtual(S, PC, RHS))
    return {};
  return LHS == RHS ? CmpResult::Equal : CmpResult::Unequal;
}

template <typename T>
llvm::Optional<CmpResult> CmpPointerEquality(InterpState &S, CodePtr PC,
                                             const T &LHS, const T &RHS) {
  return LHS == RHS ? CmpResult::Equal : CmpResult::Unequal;
}

template <PrimType Name, typename T = typename PrimConv<Name>::T>
typename std::enable_if<!isPointer(Name), llvm::Optional<CmpResult>>::type
CmpEquality(InterpState &S, CodePtr PC, const T &LHS, const T &RHS) {
  return LHS.compare(RHS);
}

template <PrimType Name, typename T = typename PrimConv<Name>::T>
typename std::enable_if<isPointer(Name), llvm::Optional<CmpResult>>::type
CmpEquality(InterpState &S, CodePtr PC, const T &LHS, const T &RHS) {
  // Do not allow comparisons of weak pointers.
  if (LHS.isWeak() || RHS.isWeak())
    return {};

  // Handle comparisons to null pointers.
  const bool LHSZero = LHS.isZero();
  const bool RHSZero = RHS.isZero();
  if (LHSZero && RHSZero)
    return CmpResult::Equal;
  if (LHSZero && RHS.isNonZero())
    return CmpResult::Unequal;
  if (RHSZero && LHS.isNonZero())
    return CmpResult::Unequal;

  // Handle general ordering - pointers can have a different base.
  return CmpPointerEquality(S, PC, LHS, RHS);
}

template <PrimType Name>
llvm::Optional<CmpResult> CmpWrapperOrdered(InterpState &S, CodePtr PC) {
  using T = typename PrimConv<Name>::T;
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  return CmpOrdered<Name>(S, PC, LHS, RHS);
}

template <PrimType Name>
llvm::Optional<CmpResult> CmpWrapperEquality(InterpState &S, CodePtr PC) {
  using T = typename PrimConv<Name>::T;
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  return CmpEquality<Name>(S, PC, LHS, RHS);
}

} // namespace detail

template <PrimType Name, PrimType Flag> bool EQ(InterpState &S, CodePtr PC) {
  using FlagTy = typename PrimConv<Flag>::T;
  if (auto R = detail::CmpWrapperEquality<Name>(S, PC)) {
    bool Result = *R == CmpResult::Equal;
    S.Stk.push<FlagTy>(FlagTy::from(Result));
    return true;
  }
  return false;
}

template <PrimType Name, PrimType Flag> bool NE(InterpState &S, CodePtr PC) {
  using FlagTy = typename PrimConv<Flag>::T;
  if (auto R = detail::CmpWrapperEquality<Name>(S, PC)) {
    bool Result = *R != CmpResult::Equal;
    S.Stk.push<FlagTy>(FlagTy::from(Result));
    return true;
  }
  return false;
}

template <PrimType Name, PrimType Flag> bool LT(InterpState &S, CodePtr PC) {
  using FlagTy = typename PrimConv<Flag>::T;
  if (auto R = detail::CmpWrapperOrdered<Name>(S, PC)) {
    bool Result = *R == CmpResult::Less;
    S.Stk.push<FlagTy>(FlagTy::from(Result));
    return true;
  }
  return false;
}

template <PrimType Name, PrimType Flag> bool LE(InterpState &S, CodePtr PC) {
  using FlagTy = typename PrimConv<Flag>::T;
  if (auto R = detail::CmpWrapperOrdered<Name>(S, PC)) {
    bool Result = *R == CmpResult::Less || *R == CmpResult::Equal;
    S.Stk.push<FlagTy>(FlagTy::from(Result));
    return true;
  }
  return false;
}

template <PrimType Name, PrimType Flag> bool GT(InterpState &S, CodePtr PC) {
  using FlagTy = typename PrimConv<Flag>::T;
  if (auto R = detail::CmpWrapperOrdered<Name>(S, PC)) {
    bool Result = *R == CmpResult::Greater;
    S.Stk.push<FlagTy>(FlagTy::from(Result));
    return true;
  }
  return false;
}

template <PrimType Name, PrimType Flag> bool GE(InterpState &S, CodePtr PC) {
  using FlagTy = typename PrimConv<Flag>::T;
  if (auto R = detail::CmpWrapperOrdered<Name>(S, PC)) {
    bool Result = *R == CmpResult::Greater || *R == CmpResult::Equal;
    S.Stk.push<FlagTy>(FlagTy::from(Result));
    return true;
  }
  return false;
}

template <PrimType Name> bool InRange(InterpState &S, CodePtr PC) {
  using T = typename PrimConv<Name>::T;
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  const T &Value = S.Stk.pop<T>();

  S.Stk.push<bool>(LHS <= Value && Value <= RHS);
  return true;
}

template <PrimType Name> bool Spaceship(InterpState &S, CodePtr PC) {
  using T = typename PrimConv<Name>::T;
  const T &RHS = S.Stk.pop<T>();
  const T &LHS = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.pop<Pointer>();

  if (auto Result = detail::CmpOrdered<Name>(S, PC, LHS, RHS)) {
    QualType ResultTy = S.getSource(PC).asExpr()->getType();
    const auto &CmpInfo = S.getASTCtx().CompCategories.getInfoForType(ResultTy);
    ComparisonCategoryResult CCResult = ToCCR(*Result);
    const auto *VD = CmpInfo.getValueInfo(CmpInfo.makeWeakResult(CCResult))->VD;

    // Get a pointer to the global containing the result.
    if (auto G = S.P.getGlobal(VD)) {
      return Copy(S, PC, *S.P.getPtrGlobal(*G).asBlock(), *Ptr.asBlock());
    }

    // Otherwise, create the global and try again.
    APValue R;
    if (S.Ctx.evaluateAsInitializer(S, VD, R)) {
      return Copy(S, PC, *S.P.getPtrGlobal(VD).asBlock(), *Ptr.asBlock());
    }
  }
  return false;
}

#endif
