//===--- Record.cpp - struct and class metadata for the VM ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Record.h"
#include "clang/AST/RecordLayout.h"

using namespace clang;
using namespace clang::interp;

CharUnits Record::Field::getTargetOffset() const {
  const ASTRecordLayout &RL = Ctx->getASTRecordLayout(RD);
  return Ctx->toCharUnitsFromBits(RL.getFieldOffset(Decl->getFieldIndex()));
}

CharUnits Record::Base::getTargetOffset() const {
  const ASTRecordLayout &RL = Ctx->getASTRecordLayout(RD);
  if (IsVirtual)
    return RL.getVBaseClassOffset(Decl);
  return RL.getBaseClassOffset(Decl);
}

Record::Record(const RecordDecl *Decl, BaseList &&SrcBases,
               FieldList &&SrcFields, VirtualBaseList &&SrcVirtualBases,
               unsigned VirtualSize, unsigned BaseSize)
    : Decl(Decl), Bases(std::move(SrcBases)), Fields(std::move(SrcFields)),
      BaseSize(BaseSize), VirtualSize(VirtualSize) {
  for (Base &V : SrcVirtualBases) {
    InterpUnits Offset = V.getOffset() + BaseSize;
    VirtualBases.push_back({V.Decl, Offset, V.RD, V.Ctx, V.R, V.Desc, V.IsVirtual});
  }

  for (Base &B : Bases) {
    BaseMap[B.getDecl()] = &B;
    BaseOffsets[B.getOffset()] = &B;
  }
  for (Field &F : Fields) {
    FieldMap[F.getDecl()] = &F;
    FieldOffsets[F.getOffset()] = &F;
  }
  for (Base &V : VirtualBases) {
    VirtualBaseMap[V.getDecl()] = &V;
    BaseOffsets[V.getOffset()] = &V;
  }
}

const Record::Field *Record::getField(const FieldDecl *FD) const {
  auto It = FieldMap.find(FD);
  return It != FieldMap.end() ? It->second : nullptr;
}

const Record::Field *Record::getFieldByOffset(unsigned Offset) const {
  auto It = FieldOffsets.find(Offset);
  assert(It != FieldOffsets.end() && "Missing field");
  return It->second;
}

const Record::Base *Record::getBase(const RecordDecl *FD) const {
  auto It = BaseMap.find(FD);
  assert(It != BaseMap.end() && "Missing base");
  return It->second;
}

const Record::Base *Record::getBaseByOffset(unsigned Offset) const {
  auto It = BaseOffsets.find(Offset);
  assert(It != BaseOffsets.end() && "Missing field");
  return It->second;
}

const Record::Base *Record::getVirtualBase(const RecordDecl *FD) const {
  auto It = VirtualBaseMap.find(FD);
  assert(It != VirtualBaseMap.end() && "Missing virtual base");
  return It->second;
}
