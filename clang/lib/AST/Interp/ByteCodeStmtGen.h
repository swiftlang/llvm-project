//===--- ByteCodeStmtGen.h - Code generator for expressions -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the constexpr bytecode compiler.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_BYTECODESTMTGEN_H
#define LLVM_CLANG_AST_INTERP_BYTECODESTMTGEN_H

#include "ByteCodeEmitter.h"
#include "ByteCodeExprGen.h"
#include "EvalEmitter.h"
#include "PrimType.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/StmtVisitor.h"
#include "llvm/ADT/Optional.h"

namespace clang {
class QualType;

namespace interp {
class Function;
class State;
class Record;

template <class Emitter> class LoopScope;
template <class Emitter> class SwitchScope;
template <class Emitter> class LabelScope;

/// Compilation context for statements.
template <class Emitter>
class ByteCodeStmtGen : public ByteCodeExprGen<Emitter> {
  using LabelTy = typename Emitter::LabelTy;
  using AddrTy = typename Emitter::AddrTy;
  using OptLabelTy = llvm::Optional<LabelTy>;
  using CaseMap = llvm::DenseMap<const SwitchCase *, LabelTy>;

public:
  template<typename... Tys>
  ByteCodeStmtGen(Tys&&... Args)
      : ByteCodeExprGen<Emitter>(std::forward<Tys>(Args)...) {}

protected:
  bool visitFunc(const FunctionDecl *F) override;

private:
  friend class LabelScope<Emitter>;
  friend class LoopScope<Emitter>;
  friend class SwitchScope<Emitter>;

  // Statement visitors.
  bool visitStmt(const Stmt *S);
  bool visitCompoundStmt(const CompoundStmt *S);
  bool visitDeclStmt(const DeclStmt *DS);
  bool visitForStmt(const ForStmt *FS);
  bool visitWhileStmt(const WhileStmt *DS);
  bool visitDoStmt(const DoStmt *DS);
  bool visitReturnStmt(const ReturnStmt *RS);
  bool visitIfStmt(const IfStmt *IS);
  bool visitBreakStmt(const BreakStmt *BS);
  bool visitContinueStmt(const ContinueStmt *CS);
  bool visitSwitchStmt(const SwitchStmt *SS);
  bool visitCaseStmt(const SwitchCase *CS);
  bool visitCXXForRangeStmt(const CXXForRangeStmt *FS);
  bool visitAttributedStmt(const AttributedStmt *AS);

  /// Compiles a variable declaration.
  bool visitVarDecl(const VarDecl *VD);

  /// Visits a field initializer.
  bool visitCtorInit(Record *This, const CXXCtorInitializer *Init);

private:
  /// Function to compile.
  const FunctionDecl *FD;
  /// Type of the expression returned by the function.
  QualType ReturnType;

  /// Switch case mapping.
  CaseMap CaseLabels;

  /// Point to break to.
  OptLabelTy BreakLabel;
  /// Point to continue to.
  OptLabelTy ContinueLabel;
  /// Default case label.
  OptLabelTy DefaultLabel;
};

extern template class ByteCodeExprGen<EvalEmitter>;

} // namespace interp
} // namespace clang

#endif
