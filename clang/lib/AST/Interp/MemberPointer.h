//===--- MemberPointer.h - Member pointer type for the VM -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_MEMBERPOINTER_H
#define LLVM_CLANG_AST_INTERP_MEMBERPOINTER_H

#include "clang/AST/DeclCXX.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace interp {

/// Base class for member and member function pointers.
class MemberPointer {
  /// Structure holding intermediary conversion steps.
  using StepList = SmallVector<const CXXRecordDecl *, 4>;
  using step_iterator = StepList::const_reverse_iterator;

  /// The declaration to which this pointer points.
  llvm::PointerIntPair<const ValueDecl *, 1> F;
  /// Flag indicating that the pointer points to a field
  /// in a class derived from the pointer's class.
  bool IsDerived = false;
  /// Chain of derived or base classes to look in.
  SmallVector<const CXXRecordDecl *, 4> Steps;

public:
  struct Dummy {};
  /// Pointer construction.
  MemberPointer() : F(nullptr, 0) {}
  MemberPointer(const ValueDecl *F)
      : F(F, 0), Steps({cast<CXXRecordDecl>(F->getDeclContext())}) {}
  MemberPointer(const ValueDecl *VD, Dummy) : F(VD, 1) {}

  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Checks if the pointer is a null member pointer.
  bool isZero() const { return !isWeak() && !F.getPointer(); }
  /// Check if the pointer is a non-null member pointer.
  bool isNonZero() const { return !isWeak() && F.getPointer(); }
  /// Checks if the pointer is weak.
  bool isWeak() const { return F.getInt() == 1; }
  /// Checks if the pointer is convertible to bool.
  bool convertsToBool() const { return !isWeak(); }

  /// Checks if the field is of a derived class.
  bool isDerived() const { return IsDerived; }

  /// Compares two member pointers.
  bool operator==(const MemberPointer &RHS) const;

  /// Converts to a base pointer, in place.
  bool toBase(const CXXRecordDecl *B);

  /// Converts to a derived pointer, in place.
  bool toDerived(const CXXRecordDecl *D);

  /// Returns the underlying declaration.
  const ValueDecl *getDecl() const {
    return F.getInt() ? nullptr : F.getPointer();
  }

  /// Iterators over the conversion chain, excluding the last class.
  llvm::iterator_range<step_iterator> steps() const {
    return llvm::make_range(Steps.rbegin() + 1, Steps.rend());
  }

  /// Prints the pointer.
  void print(llvm::raw_ostream &OS) const {
    if (isZero())
      OS << "nullptr";
    else
      OS << *getDecl();
  }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     const MemberPointer &P) {
  P.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
