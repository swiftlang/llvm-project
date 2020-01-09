//===--- Getter.h - Memory getter instructions for the VM -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the opcodes which read values from pointers.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_GETTER_H
#define LLVM_CLANG_AST_INTERP_OPCODES_GETTER_H

namespace detail {

template <typename T>
bool LoadFrom(InterpState &S, CodePtr OpPC, const Pointer &Ptr) {
  if (auto *BlockPtr = S.CheckLoad(OpPC, Ptr, AK_Read)) {
    S.Stk.push<T>(BlockPtr->deref<T>());
    return true;
  }
  return false;
}

} // namespace detail

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetLocal(InterpState &S, CodePtr OpPC, uint32_t I) {
  S.Stk.push<T>(S.Current->getLocal<T>(I));
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetParam(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression()) {
    return false;
  }
  S.Stk.push<T>(S.Current->getParam<T>(I));
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetGlobal(InterpState &S, CodePtr OpPC, GlobalLocation G) {
  auto *B = S.P.getGlobal(G);
  S.Stk.push<T>(B->deref<T>());
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetField(InterpState &S, CodePtr OpPC, uint32_t I) {
  const Pointer &Obj = S.Stk.peek<Pointer>();
  if (!S.CheckField(OpPC, Obj))
    return false;
  return detail::LoadFrom<T>(S, OpPC, Obj.atField(I));
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetFieldPop(InterpState &S, CodePtr OpPC, uint32_t I) {
  const Pointer &Obj = S.Stk.pop<Pointer>();
  if (!S.CheckField(OpPC, Obj))
    return false;
  return detail::LoadFrom<T>(S, OpPC, Obj.atField(I));
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetThisField(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  return detail::LoadFrom<T>(S, OpPC, This.atField(I));
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool GetElem(InterpState &S, CodePtr OpPC, uint32_t I) {
  const Pointer &Array = S.Stk.pop<Pointer>();
  return detail::LoadFrom<T>(S, OpPC, Array.atIndex(I));
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Load(InterpState &S, CodePtr OpPC) {
  return detail::LoadFrom<T>(S, OpPC, S.Stk.peek<Pointer>());
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool LoadPop(InterpState &S, CodePtr OpPC) {
  return detail::LoadFrom<T>(S, OpPC, S.Stk.pop<Pointer>());
}

#endif
