//===--- VoidPointer.h - Void pointer for the VM ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_VOIDPOINTER_H
#define LLVM_CLANG_AST_INTERP_VOIDPOINTER_H

#include "FnPointer.h"
#include "MemberPointer.h"
#include "Pointer.h"
#include "ObjCBlockPointer.h"
#include "clang/AST/APValue.h"

namespace clang {
namespace interp {

/// Void pointer.
class VoidPointer {
public:
  VoidPointer();
  VoidPointer(CharUnits Offset);
  VoidPointer(Pointer &&Ptr);
  VoidPointer(FnPointer &&Ptr);
  VoidPointer(MemberPointer &&Ptr);
  VoidPointer(ObjCBlockPointer &&Ptr);
  VoidPointer(const AddrLabelExpr *Ptr);

  VoidPointer(VoidPointer &&);
  VoidPointer(const VoidPointer &Ptr);

  ~VoidPointer();

  VoidPointer &operator=(VoidPointer &&);
  VoidPointer &operator=(const VoidPointer &Ptr);

  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Compares two pointers for equality.
  bool operator==(const VoidPointer &RHS) const;

  /// Checks if the pointer is a null pointer.
  bool isZero() const;
  /// Checks if the pointer is a non-null pointer.
  bool isNonZero() const;
  /// Checks if the pointer is weak.
  bool isWeak() const;
  /// Checks if the pointer is convertible to bool.
  bool convertsToBool() const;

  /// Prints the pointer.
  void print(llvm::raw_ostream &OS) const;

  /// Converts to a pointer, if originates from one.
  const Pointer *asPointer() const {
    return SourceKind == Kind::Ptr ? &S.Ptr : nullptr;
  }

private:
  /// Enumeration of source pointer types.
  enum class Kind { Ptr, FnPtr, MemberPtr, ObjCBlockPtr, LabelPtr };
  /// Kind of the void pointer.
  Kind SourceKind;
  /// Storage for all options which can be cast to void.
  union Storage {
    Storage() {}
    ~Storage() {}

    Pointer Ptr;
    FnPointer FnPtr;
    MemberPointer MemberPtr;
    ObjCBlockPointer ObjCBlockPtr;
    const AddrLabelExpr *LabelPtr;
  } S;

  void move(VoidPointer &&Ptr);
  void construct(const VoidPointer &Ptr);
  void destroy();
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                     const VoidPointer &P) {
  P.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
