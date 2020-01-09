//===--- Temp.h - Instructions for MTEs -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Opcodes which initialise the storage for MaterializeTemporaryExpr.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_TEMP_H
#define LLVM_CLANG_AST_INTERP_OPCODES_TEMP_H

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitTemp(InterpState &S, CodePtr PC, const MaterializeTemporaryExpr *MTE,
              const GlobalLocation &I) {
  const auto &Value = S.Stk.peek<T>();
  return Value.toAPValue(S.getASTCtx(), *MTE->getOrCreateValue(true));
}

inline bool InitTempValue(InterpState &S, CodePtr PC,
                          const MaterializeTemporaryExpr *MTE,
                          const GlobalLocation &I) {
  const auto &Ptr = S.Stk.peek<Pointer>();
  return PointerToValue(S.getASTCtx(), Ptr, *MTE->getOrCreateValue(true));
}

#endif
