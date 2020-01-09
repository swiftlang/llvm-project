//===--- IncDec.h - Increment and decrement instructions --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the opcodes for pre- and post-increment.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_CLANG_AST_INTERP_OPCODES_INCDED_H
#define LLVM_CLANG_AST_INTERP_OPCODES_INCDED_H

template <typename T>
using IncDecFn = bool (*)(InterpState &, CodePtr, const T &, T &);

template <typename T>
bool Post(InterpState &S, CodePtr OpPC, AccessKinds AK, IncDecFn<T> Op) {
  // Bail if not C++14.
  if (!S.getLangOpts().CPlusPlus14 && !S.keepEvaluatingAfterFailure()) {
    S.FFDiag(S.getSource(OpPC));
    return false;
  }

  // Get the pointer to the object to mutate.
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (auto *BlockPtr = S.CheckLoadStore(OpPC, Ptr, AK)) {
    // Return the original value.
    const T Arg = BlockPtr->deref<T>();
    S.Stk.push<T>(Arg);
    // Perform the operation.
    return Op(S, OpPC, Arg, BlockPtr->deref<T>());
  }
  return false;
}

template <typename T>
bool Pre(InterpState &S, CodePtr OpPC, AccessKinds AK, IncDecFn<T> Op) {
  // Bail if not C++14.
  if (!S.getLangOpts().CPlusPlus14 && !S.keepEvaluatingAfterFailure()) {
    S.FFDiag(S.getSource(OpPC));
    return false;
  }

  // Read the value to mutate.
  const Pointer &Ptr = S.Stk.peek<Pointer>();
  if (auto *BlockPtr = S.CheckLoadStore(OpPC, Ptr, AK)) {
    // Perform the operation.
    const T Arg = BlockPtr->deref<T>();
    return Op(S, OpPC, Arg, BlockPtr->deref<T>());
  }
  return false;
}

template <typename T, bool (*OpFW)(T A, T *R), template <typename U> class OpAP>
bool IncDecInt(InterpState &S, CodePtr OpPC, const T &Arg, T &R) {
  if (std::is_unsigned<T>::value) {
    // Unsigned increment/decrement is defined to wrap around.
    R = OpAP<T>()(Arg, T::from(1, Arg.bitWidth()));
    return true;
  } else {
    // Signed increment/decrement is undefined, catch that.
    if (!OpFW(Arg, &R))
      return true;
    // Compute the actual value for the diagnostic.
    const size_t Bits = R.bitWidth() + 1;
    APSInt One(APInt(Bits, 1, Arg.isSigned()), !Arg.isSigned());
    APSInt Value = OpAP<APSInt>()(Arg.toAPSInt(Bits), One);
    return S.reportOverflow(S.getSource(OpPC).asExpr(), Value);
  }
}

template <template <typename U> class Op>
bool IncDecPtr(InterpState &S, CodePtr OpPC, const Pointer &Arg, Pointer &R) {
  const auto &Loc = S.getSource(OpPC);

  // Do not allow incrementing/decrementing null pointers.
  if (Arg.isZero())
    S.CCEDiag(Loc, diag::note_constexpr_null_subobject) << CSK_ArrayIndex;

  // Compute the new offset with unsigned wraparound.
  const int64_t NewIndex = Op<int64_t>()(Arg.getIndex(), 1);
  if (Arg.isUnknownSize()) {
    S.CCEDiag(Loc, diag::note_constexpr_unsized_array_indexed);
  } else {
    // Ensure the new index is in bounds.
    const int64_t MaxIndex = Arg.getNumElems();
    if (NewIndex < 0 || NewIndex > MaxIndex) {
      S.FFDiag(Loc, diag::note_constexpr_array_index)
          << Integral<64, true>::from(NewIndex).toAPSInt()
          << /*array*/ static_cast<int>(!Arg.inArray())
          << static_cast<unsigned>(MaxIndex);
      return false;
    }
  }

  // Compute the offset from the index.
  R = Arg.atIndex(NewIndex);
  return true;
}

template <Real (*Op)(const Real &)>
bool IncDecReal(InterpState &S, CodePtr OpPC, const Real &Arg, Real &R) {
  R = Op(Arg);
  return true;
}

template <typename T> struct IncOps {
  static constexpr IncDecFn<T> Fn = IncDecInt<T, T::increment, std::plus>;
};
template <> struct IncOps<Pointer> {
  static constexpr IncDecFn<Pointer> Fn = IncDecPtr<std::plus>;
};
template <> struct IncOps<Real> {
  static constexpr IncDecFn<Real> Fn = IncDecReal<Real::increment>;
};

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool PostInc(InterpState &S, CodePtr OpPC) {
  return Post<T>(S, OpPC, AK_Increment, IncOps<T>::Fn);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool PreInc(InterpState &S, CodePtr OpPC) {
  return Pre<T>(S, OpPC, AK_Increment, IncOps<T>::Fn);
}

template <typename T> struct DecOps {
  static constexpr IncDecFn<T> Fn = IncDecInt<T, T::decrement, std::minus>;
};
template <> struct DecOps<Pointer> {
  static constexpr IncDecFn<Pointer> Fn = IncDecPtr<std::minus>;
};
template <> struct DecOps<Real> {
  static constexpr IncDecFn<Real> Fn = IncDecReal<Real::decrement>;
};

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool PostDec(InterpState &S, CodePtr OpPC) {
  return Post<T>(S, OpPC, AK_Decrement, DecOps<T>::Fn);
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool PreDec(InterpState &S, CodePtr OpPC) {
  return Pre<T>(S, OpPC, AK_Decrement, DecOps<T>::Fn);
}

#endif
