//===--- FnPointer.h - Function pointer type for the VM ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_FNPOINTER_H
#define LLVM_CLANG_AST_INTERP_FNPOINTER_H

#include "Function.h"
#include "clang/AST/DeclCXX.h"

namespace clang {
namespace interp {

/// Function pointer.
class FnPointer {
private:
  /// Enumeration of function pointer kinds.
  enum class PointerKind {
    /// Null function pointer.
    Null,
    /// Pointer to a compiled function.
    Compiled,
    /// Pointer to a declared function.
    Declared,
    /// Dummy pointer.
    Dummy,
    /// Invalid pointer.
    Invalid,
  };

  /// Tag indicating the pointer kind.
  PointerKind Kind;
  /// Underlying storage.
  union Storage {
    const FunctionDecl *FnDecl;
    Function *FnPtr;
    const ValueDecl *Decl;

    Storage() {}
    Storage(const FunctionDecl *FnDecl) : FnDecl(FnDecl) {}
    Storage(Function *FnPtr) : FnPtr(FnPtr) {}
    Storage(const ValueDecl *Decl) : Decl(Decl) {}
  } S;

public:
  struct Invalid {};

  /// Constructs a null function pointer.
  FnPointer() : Kind(PointerKind::Null) {}
  /// Constructs a valid function pointer.
  FnPointer(Function *F) : Kind(PointerKind::Compiled), S(F) {}
  FnPointer(const FunctionDecl *FD) : Kind(PointerKind::Declared), S(FD) {}
  /// Constructs a dummy function pointer.
  FnPointer(const ValueDecl *VD) : Kind(PointerKind::Dummy), S(VD) {}
  /// Constructs an invalid function pointer.
  FnPointer(Invalid) : Kind(PointerKind::Invalid) {}
  /// Constructs an invalid pointer pointing to a declaration.
  FnPointer(const ValueDecl *VD, Invalid) : Kind(PointerKind::Invalid), S(VD) {}

  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Checks if the pointer is valid.
  bool isValid() const;
  /// Checks if the pointer is a null function pointer.
  bool isZero() const { return convertsToBool() && !asFunctionDecl(); }
  /// Checks if the pointer is a non-null function pointer.
  bool isNonZero() const { return convertsToBool() && asFunctionDecl(); }
  /// Checks if the pointer is weak.
  bool isWeak() const;
  /// Checks if the pointer is convertible to bool.
  bool convertsToBool() const { return isValid() && !isWeak(); }

  /// Returns the underlying declaration, including dummies.
  const ValueDecl *asValueDecl() const;
  /// Returns the underlying function declaration.
  const FunctionDecl *asFunctionDecl() const;
  /// Returns the function.
  Function *asFunction() const;

  /// Compares two pointers for equality.
  bool operator==(const FnPointer &RHS) const;

  /// Prints the pointer.
  void print(llvm::raw_ostream &OS) const;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     const FnPointer &P) {
  P.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
