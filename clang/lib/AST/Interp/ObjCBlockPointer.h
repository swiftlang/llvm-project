//===--- ObjCBlockPointer.h - ObjC Block pointer type -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OBJCBLOCKPOINTER_H
#define LLVM_CLANG_AST_INTERP_OBJCBLOCKPOINTER_H

#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExprObjC.h"

namespace clang {
class BlockExpr;

namespace interp {

/// Function pointer.
class ObjCBlockPointer {
public:
  /// Creates a null block pointer.
  ObjCBlockPointer() : Ptr(static_cast<const BlockExpr *>(nullptr)) {}
  /// Creates a block pointer.
  ObjCBlockPointer(const BlockExpr *Ptr) : Ptr(Ptr) {}
  /// Creates a dummy pointer.
  ObjCBlockPointer(const ValueDecl *Ptr) : Ptr(Ptr) {}

  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Checks if the pointer is a null block pointer.
  bool isZero() const;
  /// Checks if the pointer is a non-null block pointer.
  bool isNonZero() const;
  /// Checks if the pointer is weak.
  bool isWeak() const;
  /// Checks if the pointer is convertible to bool.
  bool convertsToBool() const { return !isWeak(); }

  /// Compares two pointers for equality.
  bool operator==(const ObjCBlockPointer &RHS) const { return Ptr == RHS.Ptr; }

  /// Prints the pointer.
  void print(llvm::raw_ostream &OS) const;

private:
  llvm::PointerUnion<const BlockExpr *, const ValueDecl *> Ptr;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     const ObjCBlockPointer &P) {
  P.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
