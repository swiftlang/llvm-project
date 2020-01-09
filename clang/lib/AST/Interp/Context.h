//===--- Context.h - Context for the constexpr VM ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the constexpr execution context.
//
// The execution context manages cached bytecode and the global context.
// It invokes the compiler and interpreter, propagating errors.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_CONTEXT_H
#define LLVM_CLANG_AST_INTERP_CONTEXT_H

#include "Context.h"
#include "InterpStack.h"
#include "clang/AST/APValue.h"
#include "clang/Basic/LangOptions.h"
#include "llvm/ADT/PointerIntPair.h"

namespace clang {
class ASTContext;
class LangOptions;
class Stmt;
class FunctionDecl;
class VarDecl;

namespace interp {
class Function;
class Program;
class State;
enum PrimType : unsigned;

/// Holds all information required to evaluate constexpr code in a module.
class Context {
public:
  /// Initialises the constexpr VM.
  Context(ASTContext &Ctx);

  /// Cleans up the constexpr VM.
  ~Context();

  /// Checks if a function is a potential constant expression.
  bool isPotentialConstantExpr(State &Parent, const FunctionDecl *FnDecl);

  /// Evaluates a toplevel value.
  bool evaluate(State &Parent, const Expr *E, APValue &Result);
  /// Evaluates a toplevel expression as an rvalue.
  bool evaluateAsRValue(State &Parent, const Expr *E, APValue &Result);
  /// Evaluates a toplevel initializer.
  bool evaluateAsInitializer(State &Parent, const VarDecl *VD, APValue &Result);

  /// Returns the AST context.
  ASTContext &getASTContext() const { return Ctx; }
  /// Returns the language options.
  const LangOptions &getLangOpts() const;
  /// Returns the interpreter stack.
  InterpStack &getStack() { return Stk; }
  /// Returns CHAR_BIT.
  unsigned getCharBit() const;

  /// Checks is language mode is OpenCL.
  bool isOpenCL() { return getLangOpts().OpenCL; }
  /// Checks if the language mode is C++.
  bool isCPlusPlus() { return getLangOpts().CPlusPlus; }
  /// Checks is language mode is C++11.
  bool isCPlusPlus11() { return getLangOpts().CPlusPlus11; }
  /// Checks is language mode is C++14.
  bool isCPlusPlus14() { return getLangOpts().CPlusPlus14; }
  /// Checks is language mode is C++20.
  bool isCPlusPlus2a() { return getLangOpts().CPlusPlus2a; }
  /// Checks if the language mode is OpenMP.
  bool isOpenMP() {
    return getLangOpts().OpenMP && getLangOpts().OpenMPIsDevice;
  }

  /// Classifies an expression.
  llvm::Optional<PrimType> classify(QualType T);

  /// Returns the type implementing int.
  PrimType getShortType();
  /// Returns the type implementing int.
  PrimType getIntType();
  /// Returns the type implementing int.
  PrimType getLongType();
  /// Returns the type implementing unsigned long.
  PrimType getUnsignedLongType();
  /// Returns the type implementing int.
  PrimType getLongLongType();

  /// Returns the bit width of short.
  uint64_t getShortWidth();
  /// Returns the bit width of int.
  uint64_t getIntWidth();
  /// Returns the bit width of long.
  uint64_t getLongWidth();
  /// returns the bit width of unsigned longs.
  uint64_t getUnsignedLongWidth();
  /// returns the bit width of unsigned longs.
  uint64_t getLongLongWidth();

private:
  /// Runs a function.
  bool Run(State &Parent, Function *Func, APValue &Result);

  /// Checks a result fromt the interpreter.
  bool Check(State &Parent, llvm::Expected<bool> &&R);

private:
  /// Current compilation context.
  ASTContext &Ctx;
  /// Interpreter stack, shared across invocations.
  InterpStack Stk;
  /// Constexpr program.
  std::unique_ptr<Program> P;
};

} // namespace interp
} // namespace clang

#endif
