//===--- FnPointer.cpp - Function pointer for the VM ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "FnPointer.h"

using namespace clang;
using namespace clang::interp;

bool FnPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  APValue::LValueBase Base;
  bool IsNullPtr;

  switch (Kind) {
  case PointerKind::Null: {
    Base = static_cast<const Expr *>(nullptr);
    IsNullPtr = true;
    break;
  }
  case PointerKind::Compiled: {
    IsNullPtr = false;
    Base = S.FnPtr->getDecl();
    break;
  }
  case PointerKind::Declared: {
    IsNullPtr = false;
    Base = S.FnDecl;
    break;
  }
  case PointerKind::Dummy:
  case PointerKind::Invalid:
    IsNullPtr = S.Decl != nullptr;
    Base = S.Decl;
    break;
  }

  SmallVector<APValue::LValuePathEntry, 1> Entries;
  Result = APValue(Base, CharUnits::Zero(), Entries, /*isOnePastEnd=*/false,
                   IsNullPtr);
  return true;
}

bool FnPointer::isValid() const {
  return Kind != PointerKind::Invalid;
}

bool FnPointer::isWeak() const {
  switch (Kind) {
  case PointerKind::Null:
    return false;
  case PointerKind::Compiled:
    return false;
  case PointerKind::Declared:
    return S.FnDecl->isWeak();
  case PointerKind::Dummy:
    return isa<ParmVarDecl>(S.Decl);
  case PointerKind::Invalid:
    return false;
  }
  llvm_unreachable("invalid function pointer kind");
}

const ValueDecl *FnPointer::asValueDecl() const {
  switch (Kind) {
  case PointerKind::Null:
    return nullptr;
  case PointerKind::Compiled:
    return S.FnPtr->getDecl();
  case PointerKind::Declared:
    return S.FnDecl;
  case PointerKind::Invalid:
  case PointerKind::Dummy:
    return S.Decl;
  }
  llvm_unreachable("invalid function pointer kind");
}

const FunctionDecl *FnPointer::asFunctionDecl() const {
  switch (Kind) {
  case PointerKind::Null:
  case PointerKind::Dummy:
  case PointerKind::Invalid:
    return nullptr;
  case PointerKind::Compiled:
    return S.FnPtr->getDecl();
  case PointerKind::Declared:
    return S.FnDecl;
  }
  llvm_unreachable("invalid function pointer kind");
}

Function *FnPointer::asFunction() const {
  return Kind == PointerKind::Compiled ? S.FnPtr : nullptr;
}

bool FnPointer::operator==(const FnPointer &RHS) const {
  return Kind == RHS.Kind && asValueDecl() == RHS.asValueDecl();
}

void FnPointer::print(llvm::raw_ostream &OS) const {
  if (const ValueDecl *Decl = asValueDecl()) {
    OS << *Decl;
  } else {
    OS << "nullptr";
  }
}

