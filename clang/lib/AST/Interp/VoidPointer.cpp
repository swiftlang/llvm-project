//===--- VoidPointer.cpp - Void pointer for the VM --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VoidPointer.h"

using namespace clang;
using namespace clang::interp;

VoidPointer::VoidPointer() : SourceKind(Kind::Ptr) { new (&S.Ptr) Pointer(); }

VoidPointer::VoidPointer(CharUnits Offset) : SourceKind(Kind::Ptr) {
  new (&S.Ptr) Pointer(nullptr, Offset);
}

VoidPointer::VoidPointer(Pointer &&Ptr) : SourceKind(Kind::Ptr) {
  new (&S.Ptr) Pointer(std::move(Ptr));
}

VoidPointer::VoidPointer(FnPointer &&Ptr) : SourceKind(Kind::FnPtr) {
  new (&S.FnPtr) FnPointer(std::move(Ptr));
}

VoidPointer::VoidPointer(MemberPointer &&Ptr) : SourceKind(Kind::MemberPtr) {
  new (&S.MemberPtr) MemberPointer(std::move(Ptr));
}

VoidPointer::VoidPointer(ObjCBlockPointer &&Ptr)
    : SourceKind(Kind::ObjCBlockPtr) {
  new (&S.MemberPtr) ObjCBlockPointer(std::move(Ptr));
}

VoidPointer::VoidPointer(const AddrLabelExpr *Ptr)
    : SourceKind(Kind::LabelPtr) {
  S.LabelPtr = Ptr;
}

VoidPointer::VoidPointer(VoidPointer &&Ptr) { move(std::move(Ptr)); }

VoidPointer::VoidPointer(const VoidPointer &Ptr) { construct(Ptr); }

VoidPointer::~VoidPointer() { destroy(); }

VoidPointer &VoidPointer::operator=(VoidPointer &&Ptr) {
  destroy();
  move(std::move(Ptr));
  return *this;
}

VoidPointer &VoidPointer::operator=(const VoidPointer &Ptr) {
  destroy();
  construct(Ptr);
  return *this;
}

bool VoidPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr.toAPValue(Ctx, Result);
  case Kind::FnPtr:
    return S.FnPtr.toAPValue(Ctx, Result);
  case Kind::MemberPtr:
    return S.MemberPtr.toAPValue(Ctx, Result);
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr.toAPValue(Ctx, Result);
  case Kind::LabelPtr:
    Result = APValue(S.LabelPtr, CharUnits::Zero(), APValue::NoLValuePath{});
    return true;
  }
  llvm_unreachable("invalid source kind");
}

bool VoidPointer::operator==(const VoidPointer &RHS) const {
  if (SourceKind != RHS.SourceKind)
    return false;
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr == RHS.S.Ptr;
  case Kind::FnPtr:
    return S.FnPtr == RHS.S.FnPtr;
  case Kind::MemberPtr:
    return S.MemberPtr == RHS.S.MemberPtr;
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr == RHS.S.ObjCBlockPtr;
  case Kind::LabelPtr:
    return S.LabelPtr == RHS.S.LabelPtr;
  }
  llvm_unreachable("invalid source kind");
}

bool VoidPointer::isZero() const {
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr.isZero();
  case Kind::FnPtr:
    return S.FnPtr.isZero();
  case Kind::MemberPtr:
    return S.MemberPtr.isZero();
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr.isZero();
  case Kind::LabelPtr:
    return !S.LabelPtr;
  }
  llvm_unreachable("invalid source kind");
}

bool VoidPointer::isNonZero() const {
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr.isNonZero();
  case Kind::FnPtr:
    return S.FnPtr.isNonZero();
  case Kind::MemberPtr:
    return S.MemberPtr.isNonZero();
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr.isNonZero();
  case Kind::LabelPtr:
    return S.LabelPtr;
  }
  llvm_unreachable("invalid source kind");
}

bool VoidPointer::isWeak() const {
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr.isWeak();
  case Kind::FnPtr:
    return S.FnPtr.isWeak();
  case Kind::MemberPtr:
    return S.MemberPtr.isWeak();
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr.isWeak();
  case Kind::LabelPtr:
    return false;
  }
  llvm_unreachable("invalid source kind");
}

bool VoidPointer::convertsToBool() const {
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr.convertsToBool();
  case Kind::FnPtr:
    return S.FnPtr.convertsToBool();
  case Kind::MemberPtr:
    return S.MemberPtr.convertsToBool();
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr.convertsToBool();
  case Kind::LabelPtr:
    return true;
  }
  llvm_unreachable("invalid source kind");
}

void VoidPointer::print(llvm::raw_ostream &OS) const {
  switch (SourceKind) {
  case Kind::Ptr:
    return S.Ptr.print(OS);
  case Kind::FnPtr:
    return S.FnPtr.print(OS);
  case Kind::MemberPtr:
    return S.MemberPtr.print(OS);
  case Kind::ObjCBlockPtr:
    return S.ObjCBlockPtr.print(OS);
  case Kind::LabelPtr:
    OS << "&&&" << *S.LabelPtr->getLabel();
    break;
  }
}

void VoidPointer::move(VoidPointer &&Ptr) {
  SourceKind = Ptr.SourceKind;
  switch (SourceKind) {
  case Kind::Ptr:
    new (&S.Ptr) Pointer(std::move(Ptr.S.Ptr));
    break;
  case Kind::FnPtr:
    new (&S.FnPtr) FnPointer(std::move(Ptr.S.FnPtr));
    break;
  case Kind::MemberPtr:
    new (&S.MemberPtr) MemberPointer(std::move(Ptr.S.MemberPtr));
    break;
  case Kind::ObjCBlockPtr:
    new (&S.ObjCBlockPtr) ObjCBlockPointer(std::move(Ptr.S.ObjCBlockPtr));
    break;
  case Kind::LabelPtr:
    S.LabelPtr = Ptr.S.LabelPtr;
    break;
  }
}

void VoidPointer::construct(const VoidPointer &Ptr) {
  SourceKind = Ptr.SourceKind;
  switch (SourceKind) {
  case Kind::Ptr:
    new (&S.Ptr) Pointer(Ptr.S.Ptr);
    break;
  case Kind::FnPtr:
    new (&S.FnPtr) FnPointer(Ptr.S.FnPtr);
    break;
  case Kind::MemberPtr:
    new (&S.MemberPtr) MemberPointer(Ptr.S.MemberPtr);
    break;
  case Kind::ObjCBlockPtr:
    new (&S.ObjCBlockPtr) ObjCBlockPointer(Ptr.S.ObjCBlockPtr);
    break;
  case Kind::LabelPtr:
    S.LabelPtr = Ptr.S.LabelPtr;
    break;
  }
}

void VoidPointer::destroy() {
  switch (SourceKind) {
  case Kind::Ptr:
    S.Ptr.~Pointer();
    break;
  case Kind::FnPtr:
    S.FnPtr.~FnPointer();
    break;
  case Kind::MemberPtr:
    S.MemberPtr.~MemberPointer();
    break;
  case Kind::ObjCBlockPtr:
    S.ObjCBlockPtr.~ObjCBlockPointer();
    break;
  case Kind::LabelPtr:
    break;
  }
}
