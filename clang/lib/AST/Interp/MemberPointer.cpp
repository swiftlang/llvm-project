//===--- MemberPointer.cpp - Member pointer for the VM ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MemberPointer.h"

using namespace clang;
using namespace clang::interp;

bool MemberPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  if (F.getInt())
    return false;

  SmallVector<const CXXRecordDecl *, 4> APSteps;
  for (unsigned I = 1, N = Steps.size(); I < N; ++I) {
    APSteps.push_back(Steps[I]);
  }
  Result = APValue(F.getPointer(), IsDerived, APSteps);
  return true;
}

bool MemberPointer::operator==(const MemberPointer &RHS) const {
  if (!getDecl() || !RHS.getDecl())
    return !getDecl() && !RHS.getDecl();
  return F == RHS.F && IsDerived == RHS.IsDerived && Steps == RHS.Steps;
}

bool MemberPointer::toBase(const CXXRecordDecl *B) {
  if (isZero())
    return true;
  if (Steps.size() == 1) {
    IsDerived = true;
    Steps.push_back(B);
    return true;
  }
  if (IsDerived) {
    Steps.push_back(B);
    return true;
  }
  if (*(Steps.rbegin() + 1) != B)
    return false;
  Steps.pop_back();
  return true;
}

bool MemberPointer::toDerived(const CXXRecordDecl *D) {
  if (isZero())
    return true;
  // No dangling derived member - extend path.
  if (!IsDerived) {
    Steps.push_back(D);
    return true;
  }
  // The class we are casting to must match the class in the path.
  // The only path which is allowed is the one that traces back to base.
  if (*(Steps.rbegin() + 1) != D)
    return false;
  Steps.pop_back();
  if (Steps.size() == 1)
    IsDerived = false;
  return true;
}
