//===--- BlockPointer.cpp - ObjC Block pointer type -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ObjCBlockPointer.h"

using namespace clang;
using namespace clang::interp;

bool ObjCBlockPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  if (Ptr.dyn_cast<const ValueDecl *>())
    return false;
  const BlockExpr *E = Ptr.dyn_cast<const BlockExpr *>();
  SmallVector<APValue::LValuePathEntry, 1> Entries;
  Result = APValue(E, CharUnits::Zero(), Entries, /*isOnePastEnd=*/false, !E);
  return true;
}

bool ObjCBlockPointer::isZero() const {
  return !isWeak() && !Ptr.dyn_cast<const BlockExpr *>();
}

bool ObjCBlockPointer::isNonZero() const {
  return !isWeak() && Ptr.dyn_cast<const BlockExpr *>();
}

bool ObjCBlockPointer::isWeak() const {
  const ValueDecl *Decl = Ptr.dyn_cast<const ValueDecl *>();
  return Decl && (Decl->isWeak() || isa<ParmVarDecl>(Decl));
}

void ObjCBlockPointer::print(llvm::raw_ostream &OS) const {
  OS << "<block pointer>";
}
