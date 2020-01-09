//===--- Trap.h - Trap instructions for the VM ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the opcodes which fail evaluation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_TRAP_H
#define LLVM_CLANG_AST_INTERP_OPCODES_TRAP_H

inline bool Trap(InterpState &S, CodePtr PC) {
  const SourceLocation &Loc = S.getSource(PC).getLoc();
  S.FFDiag(Loc, diag::note_invalid_subexpr_in_const_expr);
  return false;
}

inline bool TrapReadNonConstInt(InterpState &S, CodePtr PC,
                                const ValueDecl *VD) {
  S.FFDiag(S.getSource(PC), diag::note_constexpr_ltor_non_const_int, 1) << VD;
  S.Note(VD->getLocation(), diag::note_declared_at);
  return false;
}

inline bool TrapReadNonConstexpr(InterpState &S, CodePtr PC,
                                 const ValueDecl *VD) {
  S.FFDiag(S.getSource(PC), diag::note_constexpr_ltor_non_constexpr, 1) << VD;
  S.Note(VD->getLocation(), diag::note_declared_at);
  return false;
}

inline bool TrapModifyGlobal(InterpState &S, CodePtr PC) {
  S.FFDiag(S.getSource(PC), diag::note_constexpr_modify_global);
  return false;
}

inline bool TrapNonLiteral(InterpState &S, CodePtr PC) {
  // C++1y: A constant initializer for an object o [...] may also invoke
  // constexpr constructors for o and its subobjects even if those objects
  // are of non-literal class types.
  //
  // C++11 missed this detail for aggregates, so classes like this:
  //   struct foo_t { union { int i; volatile int j; } u; };
  // are not (obviously) initializable like so:
  //   __attribute__((__require_constant_initialization__))
  //   static const foo_t x = {{0}};
  // because "i" is a subobject with non-literal initialization (due to the
  // volatile member of the union). See:
  //   http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#1677
  // Therefore, we use the C++1y behavior.
  const Pointer &This = S.Current->getThis();
  if (const BlockPointer *BlockThis = This.asBlock()) {
    if (S.P.getCurrentDecl() == BlockThis->getDeclID())
      return true;
  }

  const Expr *E = S.getSource(PC).asExpr();
  if (S.getLangOpts().CPlusPlus11)
    S.FFDiag(E, diag::note_constexpr_nonliteral) << E->getType();
  else
    S.FFDiag(E, diag::note_invalid_subexpr_in_const_expr);
  return false;
}

inline bool Warn(InterpState &S, CodePtr PC) {
  const SourceLocation &Loc = S.getSource(PC).getLoc();
  S.CCEDiag(Loc, diag::note_invalid_subexpr_in_const_expr);
  return true;
}

inline bool WarnReadNonConstInt(InterpState &S, CodePtr PC,
                                const ValueDecl *VD) {
  S.CCEDiag(S.getSource(PC), diag::note_constexpr_ltor_non_const_int, 1) << VD;
  S.Note(VD->getLocation(), diag::note_declared_at);
  return true;
}

inline bool WarnReadNonConstexpr(InterpState &S, CodePtr PC,
                                 const ValueDecl *VD) {
  S.CCEDiag(S.getSource(PC), diag::note_constexpr_ltor_non_constexpr, 1) << VD;
  S.Note(VD->getLocation(), diag::note_declared_at);
  return true;
}

inline bool WarnNonConstexpr(InterpState &S, CodePtr PC, const ValueDecl *VD) {
  S.CCEDiag(S.getSource(PC), diag::note_constexpr_ltor_non_constexpr) << VD;
  return true;
}

inline bool WarnPolyTypeid(InterpState &S, CodePtr PC, const CXXTypeidExpr *E) {
  S.CCEDiag(E, diag::note_constexpr_typeid_polymorphic)
      << E->getExprOperand()->getType()
      << E->getExprOperand()->getSourceRange();
  return true;
}

inline bool WarnReinterpretCast(InterpState &S, CodePtr OpPC) {
  auto *E = S.getSource(OpPC).asExpr();
  S.CCEDiag(E, diag::note_constexpr_invalid_cast) << 0;
  return true;
}

inline bool WarnDynamicCast(InterpState &S, CodePtr OpPC) {
  auto *E = S.getSource(OpPC).asExpr();
  S.CCEDiag(E, diag::note_constexpr_invalid_cast) << 1;
  return true;
}

inline bool Note(InterpState &S, CodePtr PC) {
  return S.noteFailure();
}

#endif
