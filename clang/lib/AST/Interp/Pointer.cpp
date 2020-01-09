//===--- Pointer.cpp - Types for the constexpr VM ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Pointer.h"
#include "Function.h"
#include "InterpBlock.h"
#include "PrimType.h"
#include "Record.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecordLayout.h"

using namespace clang;
using namespace clang::interp;

//===----------------------------------------------------------------------===//
// BlockPointer Implementation
//===----------------------------------------------------------------------===//

bool BlockPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  APValue::LValueBase Base;
  CharUnits Offset;
  llvm::SmallVector<APValue::LValuePathEntry, 5> LPath;
  bool IsOnePastEnd;

  // Build the lvalue base from the block.
  if (auto *VD = getDeclDesc()->asValueDecl())
    Base = VD;
  else if (auto *E = getDeclDesc()->asExpr())
    Base = E;
  else
    llvm_unreachable("invalid allocation type");

  if (isUnknownSize()) {
    IsOnePastEnd = false;
    Offset = CharUnits::Zero();
  } else {
    // TODO: compute the offset into the object.
    Offset = CharUnits::Zero();

    // Build the path into the object.
    BlockPointer Ptr = *this;
    while (Ptr.isField()) {
      if (Ptr.isArrayElement()) {
        LPath.push_back(APValue::LValuePathEntry::ArrayIndex(Ptr.getIndex()));
        Ptr = Ptr.getArray();
      } else {
        // TODO: figure out if base is virtual
        bool IsVirtual = false;
        // Create a path entry for the field.
        const Decl *Base = Ptr.getFieldDesc()->asDecl();
        LPath.push_back(APValue::LValuePathEntry({Base, IsVirtual}));
        Ptr = Ptr.getBase();
      }
    }

    IsOnePastEnd = isOnePastEnd();
  }
  Result = APValue(Base, Offset, LPath, IsOnePastEnd);
  return true;
}

BlockPointer BlockPointer::atIndex(uint64_t Idx) const {
  Descriptor *D = getArrayDesc();
  unsigned Off = Idx * elemSize();
  if (D->isArray())
    Off += D->isPrimitiveArray() ? sizeof(InitMap *) : sizeof(InlineDescriptor);
  return BlockPointer(Pointee, Base, Base + Off);
}

BlockPointer BlockPointer::atField(unsigned Off) const {
  return BlockPointer(Pointee, Offset + Off, Offset + Off);
}

BlockPointer BlockPointer::decay() const {
  // Null pointers cannot be narrowed.
  if (isUnknownSize())
    return *this;

  // Past-the-end pointers cannot be narrow.
  if (isOnePastEnd())
    return *this;

  // Adjust the pointer to point to the first element of the array.
  Descriptor *D = Offset == 0 ? getDeclDesc() : getDescriptor(Offset)->Desc;
  unsigned MetaOffset;
  if (D->isPrimitiveArray())
    MetaOffset = sizeof(InitMap *);
  else
    MetaOffset = sizeof(InlineDescriptor);
  return BlockPointer(Pointee, Offset, Offset + MetaOffset);
}

BlockPointer BlockPointer::getBase() const {
  assert(Offset == Base && "not an inner field");
  unsigned NewBase = Base - getDescriptor(Base)->Offset;
  return BlockPointer(Pointee, NewBase, NewBase);
}

BlockPointer BlockPointer::getArray() const {
  assert(Offset != Base && "not an array element");
  return BlockPointer(Pointee, Base, Base);
}

bool BlockPointer::isLive() const { return !Pointee->isDead(); }

bool BlockPointer::isField() const { return Base != 0; }

bool BlockPointer::isInitialized() const {
  if (getArrayDesc()->isPrimitiveArray()) {
    if (Pointee->isStatic())
      return true;
    // Primitive array field are stored in a bitset.
    InitMap *Map = getInitMap();
    if (!Map)
      return false;
    if (Map == (InitMap *)-1)
      return true;
    return Map->isInitialized(getOffset() / elemSize());
  }
  if (Base == 0)
    return true;
  return getDescriptor(Offset)->IsInitialized;
}

bool BlockPointer::isActive() const {
  if (Base == 0)
    return true;
  if (Base == Offset)
    return getDescriptor(Offset)->IsActive;
  if (getArrayDesc()->isPrimitiveArray())
    return getDescriptor(Base)->IsActive;
  return getDescriptor(Offset)->IsActive;
}

bool BlockPointer::isBaseClass() const {
  return isField() && getDescriptor(Offset)->IsBase;
}

bool BlockPointer::isConst() const {
  if (Base == 0)
    return getDeclDesc()->IsConst;
  if (Base == Offset)
    return getDescriptor(Offset)->IsConst;
  if (getArrayDesc()->isPrimitiveArray())
    return getDescriptor(Base)->IsConst;
  return getDescriptor(Offset)->IsConst;
}

bool BlockPointer::isLiteral() const {
  Descriptor *Desc = getDeclDesc();
  const Expr *E = Desc->asExpr();
  if (!E)
    return false;
  return !isa<MaterializeTemporaryExpr>(E);
}

bool BlockPointer::isRoot() const {
  return Base == 0 && Offset == 0;
}

bool BlockPointer::isArrayElement() const { return Base != Offset; }

bool BlockPointer::isMutable() const {
  if (Base == 0)
    return false;
  if (Base == Offset)
    return getDescriptor(Offset)->IsMutable;
  if (getArrayDesc()->isPrimitiveArray())
    return getDescriptor(Base)->IsMutable;
  return getDescriptor(Offset)->IsMutable;
}

void BlockPointer::initialize() const {
  Descriptor *Desc = getArrayDesc();
  if (Desc->isPrimitiveArray()) {
    if (!Pointee->isStatic()) {
      // Primitive array initializer.
      InitMap *&Map = getInitMap();
      if (Map == (InitMap *)-1)
        return;
      if (Map == nullptr)
        Map = InitMap::allocate(Desc->getNumElems());
      if (Map->initialize(getOffset() / elemSize())) {
        free(Map);
        Map = (InitMap *)-1;
      }
    }
  } else {
    // Field has its bit in an inline descriptor.
    assert(Base != 0 && "Only composite fields can be initialised");
    getDescriptor(Offset)->IsInitialized = true;
  }
}

void BlockPointer::activate() const {
  // Field has its bit in an inline descriptor.
  assert(Base != 0 && "Only composite fields can be initialised");
  getDescriptor(Offset)->IsActive = true;
}

void BlockPointer::deactivate() const {
  // TODO: implement active member change
}

Descriptor *BlockPointer::getDeclDesc() const {
  return Pointee->getDescriptor();
}

Descriptor *BlockPointer::getFieldDesc() const {
  if (Offset == 0)
    return getDeclDesc();
  return getDescriptor(Offset)->Desc;
}

Descriptor *BlockPointer::getArrayDesc() const {
  if (Base == 0)
    return getDeclDesc();
  return getDescriptor(Base)->Desc;
}

QualType BlockPointer::getFieldType() const {
  Descriptor *Desc = Base == 0 ? getDeclDesc() : getDescriptor(Base)->Desc;
  if (Offset == Base)
    return Desc->getType();
  Descriptor *ElemDesc = Desc->getElemDesc();
  if (ElemDesc)
    return ElemDesc->getType();
  return Desc->getType()->getAsArrayTypeUnsafe()->getElementType();
}

llvm::Optional<PrimType> BlockPointer::elementType() const {
  return getArrayDesc()->ElemTy;
}

uint64_t BlockPointer::getIndex() const {
  if (auto ElemSize = elemSize())
    return getOffset() / ElemSize;
  return 0;
}

unsigned BlockPointer::getOffset() const {
  Descriptor *D = getArrayDesc();
  if (!D->isArray())
    return Offset - Base;
  if (!D->isPrimitiveArray())
    return Offset - Base - sizeof(InlineDescriptor);
  return Offset - Base - sizeof(InitMap *);
}

bool BlockPointer::hasSameBase(const BlockPointer &RHS) const {
  return !Pointee->isDead() && Pointee == RHS.Pointee;
}

bool BlockPointer::hasSameArray(const BlockPointer &RHS) const {
  if (Base != RHS.Base)
    return false;
  return getArrayDesc()->IsArray;
}

CmpResult BlockPointer::compare(const BlockPointer &RHS) const {
  return Compare(Offset, RHS.Offset);
}

void BlockPointer::print(llvm::raw_ostream &OS) const {
  OS << "{";
  if (const ValueDecl *Decl = getDeclDesc()->asValueDecl())
    OS << *Decl << ", ";
  OS << Base << ", " << Offset << ", " << Pointee->getSize();
  OS << "}";
}

void BlockPointer::print(llvm::raw_ostream &OS, ASTContext &Ctx,
                         QualType Ty) const {
  auto printDesc = [&OS, &Ctx](Descriptor *Desc) {
    if (auto *D = Desc->asDecl()) {
      // Subfields or named values.
      if (auto *VD = dyn_cast<ValueDecl>(D)) {
        OS << *VD;
        return;
      }
      // Base classes.
      if (isa<RecordDecl>(D)) {
        return;
      }
    }
    // Temporary expression.
    if (auto *E = Desc->asExpr()) {
      E->printPretty(OS, nullptr, Ctx.getPrintingPolicy());
      return;
    }
    llvm_unreachable("Invalid descriptor type");
  };

  if (!Ty->isReferenceType())
    OS << "&";
  llvm::SmallVector<BlockPointer, 2> Levels;
  for (BlockPointer F = *this; !F.isRoot(); F = F.getContainingObject())
    Levels.push_back(F);

  printDesc(getDeclDesc());
  for (auto It = Levels.rbegin(); It != Levels.rend(); ++It) {
    if (It->inArray()) {
      OS << "[" << It->getIndex() << "]";
      continue;
    }
    if (auto Index = It->getIndex()) {
      OS << " + " << Index;
      continue;
    }
    OS << ".";
    printDesc(It->getFieldDesc());
  }
}

std::string BlockPointer::getAsString(ASTContext &Ctx, QualType Ty) const {
  std::string Result;
  llvm::raw_string_ostream Out(Result);
  print(Out, Ctx, Ty);
  Out.flush();
  return Result;
}

bool BlockPointer::operator==(const BlockPointer &RHS) const {
  return Pointee == RHS.Pointee && Base == RHS.Base && Offset == RHS.Offset;
}

//===----------------------------------------------------------------------===//
// ExternPointer implementation
//===----------------------------------------------------------------------===//

bool ExternPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  llvm::SmallVector<APValue::LValuePathEntry, 5> LPath;
  CharUnits Offset = CharUnits::Zero();
  Descriptor *D = Desc;

  for (const PointerPath::Entry &E : Path->Entries) {
    switch (E.K) {
    case PointerPath::Entry::Kind::Base:
    case PointerPath::Entry::Kind::Virt: {
      const RecordDecl *DerivedDecl = D->ElemRecord->getDecl();
      const ASTRecordLayout &RL = Ctx.getASTRecordLayout(DerivedDecl);

      if (E.K == PointerPath::Entry::Kind::Base)
        Offset += RL.getBaseClassOffset(E.U.BaseEntry);
      else
        Offset += RL.getVBaseClassOffset(E.U.BaseEntry);

      LPath.push_back({ APValue::BaseOrMemberType{ E.U.BaseEntry, 0 } });
      D = E.Desc;
      continue;
    }
    case PointerPath::Entry::Kind::Field: {
      const RecordDecl *ParentDecl = E.U.FieldEntry->getParent();
      const ASTRecordLayout &RL = Ctx.getASTRecordLayout(ParentDecl);
      unsigned FI = E.U.FieldEntry->getFieldIndex();
      Offset += CharUnits::fromQuantity(RL.getFieldOffset(FI));
      LPath.push_back({ APValue::BaseOrMemberType{ E.U.FieldEntry, 0 } });
      D = E.Desc;
      continue;
    }
    case PointerPath::Entry::Kind::Index: {
      if (!D->isArray()) {
        Offset += D->getTargetSize() * E.U.Index;
        Result = APValue(Base, Offset, LPath, E.U.Index != 0);
        return true;
      }
      Offset += D->getTargetElementSize() * E.U.Index;
      LPath.push_back(APValue::LValuePathEntry::ArrayIndex(E.U.Index));
      D = D->getElemDesc();
      continue;
    }
    }
  }

  Result = APValue(Base, Offset, LPath, /*isOnePastEnd=*/false);
  return true;
}

ExternPointer ExternPointer::atIndex(uint64_t Idx) const {
  if (Path->Entries.empty()) {
    if (Idx == 0)
      return *this;
    PointerPath::Entry E(Desc->getElemDesc(), Idx);
    return ExternPointer(*this, PointerPath{{E}});
  }

  auto &E = *Path->Entries.rbegin();
  switch (E.K) {
    case PointerPath::Entry::Kind::Base:
    case PointerPath::Entry::Kind::Virt:
    case PointerPath::Entry::Kind::Field: {
      if (Idx == 0)
        return *this;
      PointerPath NewPath(*Path);
      NewPath.Entries.emplace_back(E.Desc, Idx);
      return ExternPointer(*this, std::move(NewPath));
    }
    case PointerPath::Entry::Kind::Index: {
      PointerPath NewPath(*Path);
      *NewPath.Entries.rbegin() = PointerPath::Entry(E.Desc, Idx);
      return ExternPointer(*this, std::move(NewPath));
    }
  }
  llvm_unreachable("invalid path entry kind");
}

ExternPointer ExternPointer::atField(unsigned Off) const {
  auto *F = getRecord()->getFieldByOffset(Off);

  PointerPath NewPath(*Path);
  NewPath.Entries.emplace_back(F->getDesc(), F->getDecl());
  return ExternPointer(*this, std::move(NewPath));
}

ExternPointer ExternPointer::atBase(unsigned Off, bool IsVirtual) const {
  PointerPath NewPath(*Path);

  if (IsVirtual) {
    // If the base is virtual, drop all other bases.
    while (!NewPath.Entries.empty()) {
      switch (NewPath.Entries.rbegin()->K) {
      case PointerPath::Entry::Kind::Base:
      case PointerPath::Entry::Kind::Virt:
        NewPath.Entries.pop_back();
        continue;
      case PointerPath::Entry::Kind::Field:
      case PointerPath::Entry::Kind::Index:
        break;
      }
      break;
    }
  }

  auto *F = getRecord()->getBaseByOffset(Off);
  NewPath.Entries.emplace_back(F->getDesc(), F->getDecl(), IsVirtual);
  return ExternPointer(*this, std::move(NewPath));
}

ExternPointer ExternPointer::decay() const {
  PointerPath NewPath(*Path);
  NewPath.Entries.emplace_back(getFieldDesc()->getElemDesc(), (uint64_t)0);
  return ExternPointer(*this, std::move(NewPath));
}

ExternPointer ExternPointer::getBase() const {
  assert(!Path->Entries.empty() && "not a derived class");

  auto &E = *Path->Entries.rbegin();
  switch (E.K) {
    case PointerPath::Entry::Kind::Base:
    case PointerPath::Entry::Kind::Virt: {
      PointerPath NewPath(*Path);
      NewPath.Entries.pop_back();
      return ExternPointer(*this, std::move(NewPath));
    }
    case PointerPath::Entry::Kind::Field:
      llvm_unreachable("ExternPointer::getBase Field");
    case PointerPath::Entry::Kind::Index:
      llvm_unreachable("ExternPointer::getBase Index");
  }
  llvm_unreachable("invalid path entry kind");
}

bool ExternPointer::isOnePastEnd() const {
  return !isUnknownSize() && getIndex() == getNumElems();
}

bool ExternPointer::inArray() const {
  return !Path->Entries.empty() && Path->Entries.rbegin()->isIndex();
}

bool ExternPointer::isBaseClass() const {
  if (Path->Entries.empty())
    return false;
  return Path->Entries.rbegin()->K == PointerPath::Entry::Kind::Base;
}

uint64_t ExternPointer::getIndex() const {
  if (Path->Entries.empty())
    return 0;
  auto &F = *Path->Entries.rbegin();
  return F.isIndex() ? F.U.Index : 0;
}

bool ExternPointer::hasSameBase(const ExternPointer &RHS) const {
  return Base == RHS.Base;
}

bool ExternPointer::hasSameArray(const ExternPointer &B) const {
  if (Base != B.Base)
    return false;

  // TODO: implement this.
  return true;
}

CmpResult ExternPointer::compare(const ExternPointer &B) const {
  if (Base != B.Base)
    return CmpResult::Unordered;

  Descriptor *D = Desc;
  const unsigned N = std::min(Path->Entries.size(), B.Path->Entries.size());
  for (unsigned I = 0; I < N; ++I) {
    const auto &EntryA = Path->Entries[I];
    const auto &EntryB = B.Path->Entries[I];
    switch (EntryA.K) {
    case PointerPath::Entry::Kind::Base: {
      llvm_unreachable("PointerPath::Entry::Kind::Base compare");
    }
    case PointerPath::Entry::Kind::Virt: {
      llvm_unreachable("PointerPath::Entry::Kind::Virt compare");
    }
    case PointerPath::Entry::Kind::Field: {
      const Record *R = D->ElemRecord;
      const Record::Field *FieldA = R->getField(EntryA.U.FieldEntry);
      switch (EntryB.K) {
      case PointerPath::Entry::Kind::Base:
      case PointerPath::Entry::Kind::Virt:
        llvm_unreachable("invalid field");
      case PointerPath::Entry::Kind::Field: {
        if (EntryA.U.FieldEntry == EntryB.U.FieldEntry) {
          D = FieldA->getDesc();
          continue;
        }
        const Record::Field *FieldB = R->getField(EntryB.U.FieldEntry);
        return Compare(FieldA->getTargetOffset(), FieldB->getTargetOffset());
      }
      case PointerPath::Entry::Kind::Index:
        llvm_unreachable("invalid field");
      }
      break;
    }
    case PointerPath::Entry::Kind::Index: {
      switch (EntryB.K) {
      case PointerPath::Entry::Kind::Base:
      case PointerPath::Entry::Kind::Virt:
      case PointerPath::Entry::Kind::Field:
        llvm_unreachable("invalid field");
      case PointerPath::Entry::Kind::Index:
        if (EntryA.U.Index == EntryB.U.Index) {
          D = D->getElemDesc();
          continue;
        }
        return Compare(EntryA.U.Index, EntryB.U.Index);
      }
      break;
    }
    }
  }
  return Compare(Path->Entries.size(), B.Path->Entries.size());
}

void ExternPointer::print(llvm::raw_ostream &OS) const {
  OS << *Base;
  for (const PointerPath::Entry &E : Path->Entries) {
    switch (E.K) {
    case PointerPath::Entry::Kind::Base:
      OS << ".<" << *E.U.BaseEntry << ">";
      continue;
    case PointerPath::Entry::Kind::Virt:
      OS << ".<virtual " << *E.U.BaseEntry << ">";
      continue;
    case PointerPath::Entry::Kind::Field:
      OS << "." << *E.U.FieldEntry;
      continue;
    case PointerPath::Entry::Kind::Index:
      OS << "[" << E.U.Index << "]";
      continue;
    }
  }
}

void ExternPointer::print(llvm::raw_ostream &OS, ASTContext &Ctx,
                          QualType Ty) const {
  if (!Ty->isReferenceType())
    OS << "&";
  OS << *Base;
  for (const auto &E : Path->Entries) {
    switch (E.K) {
    case PointerPath::Entry::Kind::Base:
    case PointerPath::Entry::Kind::Virt:
      continue;
    case PointerPath::Entry::Kind::Field:
      OS << "." << *E.U.FieldEntry;
      continue;
    case PointerPath::Entry::Kind::Index:
      OS << "[" << E.U.Index << "]";
      continue;
    }
  }
}

std::string ExternPointer::getAsString(ASTContext &Ctx, QualType Ty) const {
  std::string Result;
  llvm::raw_string_ostream Out(Result);
  print(Out, Ctx, Ty);
  Out.flush();
  return Result;
}

bool ExternPointer::operator==(const ExternPointer &B) const {
  if (Base != B.Base || Path->Entries.size() != B.Path->Entries.size())
    return false;

  Descriptor *D = Desc;
  for (unsigned I = 0, N = Path->Entries.size(); I < N; ++I) {
    const auto &EntryA = Path->Entries[I];
    const auto &EntryB = B.Path->Entries[I];
    if (EntryA.K != EntryB.K)
      return false;

    switch (EntryA.K) {
    case PointerPath::Entry::Kind::Virt:
    case PointerPath::Entry::Kind::Base:
      if (EntryA.U.BaseEntry != EntryB.U.BaseEntry)
        return false;
      D = EntryA.Desc;
      continue;
    case PointerPath::Entry::Kind::Field: {
      const Record *R = D->ElemRecord;
      const Record::Field *FieldA = R->getField(EntryA.U.FieldEntry);
      if (EntryA.U.FieldEntry != EntryB.U.FieldEntry)
        return false;
      D = FieldA->getDesc();
      continue;
    }
    case PointerPath::Entry::Kind::Index:
      if (EntryA.U.Index != EntryB.U.Index)
        return false;
      D = D->getElemDesc();
      continue;
    }
  }

  return true;
}

Descriptor *ExternPointer::getFieldDesc() const {
  return Path->Entries.empty() ? Desc : Path->Entries.rbegin()->Desc;
}

Descriptor *ExternPointer::getArrayDesc() const {
  if (Path->Entries.empty() || !Path->Entries.rbegin()->isIndex())
    return getFieldDesc();
  if (Path->Entries.size() == 1)
    return Desc;
  return (Path->Entries.rbegin() + 1)->Desc;
}

//===----------------------------------------------------------------------===//
// TargetPointer implementation
//===----------------------------------------------------------------------===//

bool TargetPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  CharUnits Offset = getTargetOffset();
  llvm::SmallVector<APValue::LValuePathEntry, 5> LPath;
  Result = APValue(static_cast<const Expr *>(nullptr), Offset, LPath,
                   isOnePastEnd(), Offset.isZero());
  return true;
}

TargetPointer TargetPointer::atIndex(uint64_t Idx) const {
  CharUnits NewOffset = Idx * Desc->getTargetElementSize();
  return TargetPointer(Desc, BaseOffset, FieldOffset, NewOffset, InArray);
}

TargetPointer TargetPointer::atField(unsigned Off) const {
  auto *F = getRecord()->getFieldByOffset(Off);
  return TargetPointer(F->getDesc(), BaseOffset,
                       FieldOffset + ArrayOffset + F->getTargetOffset(),
                       CharUnits::Zero(), /*inArray=*/false);
}

TargetPointer TargetPointer::decay() const {
  return TargetPointer(Desc, BaseOffset, FieldOffset + ArrayOffset,
                       CharUnits::Zero(), /*inArray=*/true);
}

TargetPointer TargetPointer::getBase() const {
  llvm_unreachable("TargetPointer::getBase");
}

TargetPointer TargetPointer::atBase(unsigned Off, bool IsVirtual) const {
  if (IsVirtual)
    llvm_unreachable("TargetPointer::atBase");
  auto *B = getRecord()->getBaseByOffset(Off);
  return TargetPointer(B->getDesc(), BaseOffset,
                       FieldOffset + ArrayOffset + B->getTargetOffset(),
                       CharUnits::Zero(), /*inArray=*/false);
}

bool TargetPointer::isBaseClass() const {
  llvm_unreachable("TargetPointer::isBaseClass");
}

bool TargetPointer::isZero() const {
  return (BaseOffset + FieldOffset + ArrayOffset).isZero();
}

uint64_t TargetPointer::getIndex() const {
  if (ArrayOffset.isZero() || !Desc)
    return 0;
  return ArrayOffset / Desc->getTargetElementSize();
}

uint64_t TargetPointer::getNumElems() const {
  return Desc ? getArrayDesc()->getNumElems() : 0;
}

bool TargetPointer::hasSameArray(const TargetPointer &RHS) const {
  llvm_unreachable("TargetPointer::hasSameArray");
}

CmpResult TargetPointer::compare(const TargetPointer &RHS) const {
  llvm_unreachable("TargetPointer::compare");
}

void TargetPointer::print(llvm::raw_ostream &OS) const {
  OS << "{" << getTargetOffset().getQuantity() << "}";
}

void TargetPointer::print(llvm::raw_ostream &OS, ASTContext &Ctx,
                        QualType Ty) const {
  llvm_unreachable("TargetPointer::print");
}

std::string TargetPointer::getAsString(ASTContext &Ctx, QualType Ty) const {
  std::string Result;
  llvm::raw_string_ostream Out(Result);
  print(Out, Ctx, Ty);
  Out.flush();
  return Result;
}

bool TargetPointer::operator==(const TargetPointer &RHS) const {
  return getTargetOffset() == RHS.getTargetOffset();
}

//===----------------------------------------------------------------------===//
// TypeInfoPointer implementation
//===----------------------------------------------------------------------===//

bool TypeInfoPointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  auto Base = APValue::LValueBase::getTypeInfo(TypeInfoLValue(Ty), InfoTy);
  SmallVector<APValue::LValuePathEntry, 1> Entries;
  Result = APValue(Base, CharUnits::Zero(), Entries,
                   /*isOnePastEnd=*/Index == getNumElems(),
                   /*isNullPtr=*/false);
  return true;
}

TypeInfoPointer TypeInfoPointer::atIndex(uint64_t Idx) const {
  return TypeInfoPointer(Ty, InfoTy, Idx);
}

TypeInfoPointer TypeInfoPointer::atField(unsigned Off) const {
  llvm_unreachable("atField invalid for std::type_info");
}

TypeInfoPointer TypeInfoPointer::decay() const {
  llvm_unreachable("decay invalid for std::type_info");
}

TypeInfoPointer TypeInfoPointer::getBase() const {
  llvm_unreachable("getBase invalid for std::type_info");
}

TypeInfoPointer TypeInfoPointer::atBase(unsigned Off, bool IsVirtual) const {
  llvm_unreachable("atBase invalid for std::type_info");
}

const FieldDecl *TypeInfoPointer::getField() const {
  llvm_unreachable("getField invalid for std::type_info");
}

QualType TypeInfoPointer::getFieldType() const {
  llvm_unreachable("getFieldType invalid for std::type_info");
}

Record *TypeInfoPointer::getRecord() const {
  llvm_unreachable("getRecord invalid for std::type_info");
}

CmpResult TypeInfoPointer::compare(const TypeInfoPointer &RHS) const {
  if (Ty != RHS.Ty)
    return CmpResult::Unordered;
  return Compare(Index, RHS.Index);
}

bool TypeInfoPointer::operator==(const TypeInfoPointer &RHS) const {
  return Ty == RHS.Ty && Index == RHS.Index;
}

void TypeInfoPointer::print(llvm::raw_ostream &OS) const {
  OS << "typeid(" << QualType(Ty, 0).getAsString() << ")";
}

void TypeInfoPointer::print(llvm::raw_ostream &OS, ASTContext &Ctx,
                            QualType) const {
  OS << "typeid(";
  QualType(Ty, 0).print(OS, Ctx.getPrintingPolicy());
  OS << ")";
}

std::string TypeInfoPointer::getAsString(ASTContext &Ctx, QualType Ty) const {
  std::string Result;
  llvm::raw_string_ostream Out(Result);
  print(Out, Ctx, Ty);
  Out.flush();
  return Result;
}

//===----------------------------------------------------------------------===//
// Pointer implementation
//===----------------------------------------------------------------------===//

#define DISPATCH(METHOD_CALL)                                                  \
  switch (U.K) {                                                               \
  case PointerKind::TargetPtr:                                                 \
    return U.TargetPtr.METHOD_CALL;                                            \
  case PointerKind::BlockPtr:                                                  \
    return U.BlockPtr.METHOD_CALL;                                             \
  case PointerKind::ExternPtr:                                                 \
    return U.ExternPtr.METHOD_CALL;                                            \
  case PointerKind::TypeInfoPtr:                                               \
    return U.TypeInfoPtr.METHOD_CALL;                                          \
  case PointerKind::InvalidPtr:                                                \
    llvm_unreachable("invalid pointer");                                       \
  }                                                                            \
  llvm_unreachable("invalid pointer kind");

Pointer Pointer::atIndex(uint64_t Idx) const { DISPATCH(atIndex(Idx)); }
Pointer Pointer::atField(unsigned Off) const { DISPATCH(atField(Off)); }
Pointer Pointer::decay() const { DISPATCH(decay()); }
Pointer Pointer::getBase() const { DISPATCH(getBase()); }
Pointer Pointer::atBase(unsigned Off, bool IsVirtual) const {
  DISPATCH(atBase(Off, IsVirtual));
}

bool Pointer::inArray() const { DISPATCH(inArray()); }
bool Pointer::isUnknownSize() const { DISPATCH(isUnknownSize()); }
bool Pointer::isOnePastEnd() const { DISPATCH(isOnePastEnd()); }
bool Pointer::isBaseClass() const { DISPATCH(isBaseClass()); }
bool Pointer::isLiteral() const { DISPATCH(isLiteral()); }

const FieldDecl *Pointer::getField() const { DISPATCH(getField()); }
QualType Pointer::getFieldType() const { DISPATCH(getFieldType()); }
Record *Pointer::getRecord() const { DISPATCH(getRecord()); }

uint64_t Pointer::getIndex() const { DISPATCH(getIndex()); }
unsigned Pointer::getNumElems() const { DISPATCH(getNumElems()); }

void Pointer::print(llvm::raw_ostream &OS) const { DISPATCH(print(OS)); }

void Pointer::print(llvm::raw_ostream &OS, ASTContext &Ctx, QualType Ty) const {
  DISPATCH(print(OS, Ctx, Ty));
}

std::string Pointer::getAsString(ASTContext &Ctx, QualType Ty) const {
  DISPATCH(getAsString(Ctx, Ty));
}

bool Pointer::toAPValue(const ASTContext &Ctx, APValue &Result) const {
  DISPATCH(toAPValue(Ctx, Result));
}

bool Pointer::isZero() const { DISPATCH(isZero()); }
bool Pointer::isNonZero() const { DISPATCH(isNonZero()); }

bool Pointer::isInitialized() const {
  return U.K == PointerKind::BlockPtr && U.BlockPtr.isInitialized();
}

bool Pointer::isWeak() const {
  switch (U.K) {
  case PointerKind::TargetPtr:
    return false;
  case PointerKind::BlockPtr:
    return false;
  case PointerKind::ExternPtr: {
    auto *Decl = U.ExternPtr.getDecl();
    return Decl && (Decl->isWeak() || isa<ParmVarDecl>(Decl));
  }
  case PointerKind::TypeInfoPtr:
    return false;
  case PointerKind::InvalidPtr:
    llvm_unreachable("invalid pointer");
  }
  llvm_unreachable("invalid pointer kind");
}

bool Pointer::convertsToBool() const {
  switch (U.K) {
  case PointerKind::TargetPtr:
    return true;
  case PointerKind::BlockPtr:
    return true;
  case PointerKind::ExternPtr: {
    auto *Decl = U.ExternPtr.getDecl();
    return !Decl->isWeak() && !isa<ParmVarDecl>(Decl);
  }
  case PointerKind::TypeInfoPtr:
    return true;
  case PointerKind::InvalidPtr:
    llvm_unreachable("invalid pointer");
  }
  llvm_unreachable("invalid pointer kind");
}

bool Pointer::hasSameBase(const Pointer &RHS) const {
  if (U.K != RHS.U.K)
    return false;

  switch (U.K) {
  case PointerKind::TargetPtr:
    return true;
  case PointerKind::BlockPtr:
    return U.BlockPtr.hasSameBase(RHS.U.BlockPtr);
  case PointerKind::ExternPtr:
    return U.ExternPtr.hasSameBase(RHS.U.ExternPtr);
  case PointerKind::TypeInfoPtr:
    return U.TypeInfoPtr.hasSameBase(RHS.U.TypeInfoPtr);
  case PointerKind::InvalidPtr:
    llvm_unreachable("invalid pointer");
  }
  llvm_unreachable("invalid pointer kind");
}

bool Pointer::hasSameArray(const Pointer &RHS) const {
  if (U.K != RHS.U.K)
    return false;

  switch (U.K) {
  case PointerKind::TargetPtr:
    return U.TargetPtr.hasSameArray(RHS.U.TargetPtr);
  case PointerKind::BlockPtr:
    return U.BlockPtr.hasSameArray(RHS.U.BlockPtr);
  case PointerKind::ExternPtr:
    return U.ExternPtr.hasSameArray(RHS.U.ExternPtr);
  case PointerKind::TypeInfoPtr:
    return U.TypeInfoPtr.hasSameArray(RHS.U.TypeInfoPtr);
  case PointerKind::InvalidPtr:
    llvm_unreachable("invalid pointer");
  }
  llvm_unreachable("invalid pointer kind");
}

CmpResult Pointer::compare(const Pointer &RHS) const {
  if (U.K != RHS.U.K)
    return CmpResult::Unordered;
  switch (U.K) {
  case PointerKind::TargetPtr:
    return U.TargetPtr.compare(RHS.U.TargetPtr);
  case PointerKind::BlockPtr:
    return U.BlockPtr.compare(RHS.U.BlockPtr);
  case PointerKind::ExternPtr:
    return U.ExternPtr.compare(RHS.U.ExternPtr);
  case PointerKind::TypeInfoPtr:
    return U.TypeInfoPtr.compare(RHS.U.TypeInfoPtr);
  case PointerKind::InvalidPtr:
    llvm_unreachable("invalid pointer");
  }
  llvm_unreachable("invalid pointer kind");
}

bool Pointer::operator==(const Pointer &RHS) const {
  if (U.K != RHS.U.K)
    return false;

  switch (U.K) {
  case PointerKind::TargetPtr:
    return U.TargetPtr == RHS.U.TargetPtr;
  case PointerKind::BlockPtr:
    return U.BlockPtr == RHS.U.BlockPtr;
  case PointerKind::ExternPtr:
    return U.ExternPtr == RHS.U.ExternPtr;
  case PointerKind::TypeInfoPtr:
    return U.TypeInfoPtr == RHS.U.TypeInfoPtr;
  case PointerKind::InvalidPtr:
    llvm_unreachable("invalid pointer");
  }
  llvm_unreachable("invalid pointer kind");
}

void Pointer::operator=(const Pointer &P) {
  Block *Old = nullptr;

  switch (U.K) {
  case PointerKind::TargetPtr:
    break;
  case PointerKind::BlockPtr:
    Old = U.BlockPtr.Pointee;
    Old->removePointer(&U.BlockPtr);
    break;
  case PointerKind::ExternPtr:
    break;
  case PointerKind::TypeInfoPtr:
    break;
  case PointerKind::InvalidPtr:
    break;
  }

  switch (P.U.K) {
  case PointerKind::TargetPtr:
    new (&U.TargetPtr) TargetPointer(P.U.TargetPtr);
    break;
  case PointerKind::BlockPtr:
    new (&U.BlockPtr) BlockPointer(P.U.BlockPtr);
    U.BlockPtr.Pointee->addPointer(&U.BlockPtr);
    break;
  case PointerKind::ExternPtr:
    new (&U.ExternPtr) ExternPointer(P.U.ExternPtr);
    break;
  case PointerKind::TypeInfoPtr:
    new (&U.TypeInfoPtr) TypeInfoPointer(P.U.TypeInfoPtr);
    break;
  case PointerKind::InvalidPtr:
    U.K = PointerKind::InvalidPtr;
    break;
  }

  if (Old)
    Old->cleanup();
}

void Pointer::operator=(Pointer &&P) {
  Block *Old = nullptr;

  switch (U.K) {
  case PointerKind::TargetPtr:
    break;
  case PointerKind::BlockPtr:
    Old = U.BlockPtr.Pointee;
    Old->removePointer(&U.BlockPtr);
    break;
  case PointerKind::ExternPtr:
    break;
  case PointerKind::TypeInfoPtr:
    break;
  case PointerKind::InvalidPtr:
    break;
  }

  switch (P.U.K) {
  case PointerKind::TargetPtr:
    new (&U.TargetPtr) TargetPointer(std::move(P.U.TargetPtr));
    break;
  case PointerKind::BlockPtr:
    new (&U.BlockPtr) BlockPointer(std::move(P.U.BlockPtr));
    U.BlockPtr.Pointee->movePointer(&P.U.BlockPtr, &U.BlockPtr);
    break;
  case PointerKind::ExternPtr:
    new (&U.ExternPtr) ExternPointer(std::move(P.U.ExternPtr));
    break;
  case PointerKind::TypeInfoPtr:
    new (&U.TypeInfoPtr) TypeInfoPointer(std::move(P.U.TypeInfoPtr));
    break;
  case PointerKind::InvalidPtr:
    U.K = PointerKind::InvalidPtr;
    break;
  }

  if (Old)
    Old->cleanup();
}

//===----------------------------------------------------------------------===//
// Pointer storage implementation
//===----------------------------------------------------------------------===//

Pointer::Storage::Storage(Block *B) {
  if (B) {
    B->addPointer(new (&BlockPtr) BlockPointer(B));
  } else {
    new (&TargetPtr) TargetPointer();
  }
}

Pointer::Storage::Storage(Block *B, unsigned Base, unsigned Offset) {
  if (B) {
    B->addPointer(new (&BlockPtr) BlockPointer(B, Base, Offset));
  } else {
    new (&TargetPtr) TargetPointer();
  }
}

Pointer::Storage::Storage(BlockPointer &&Ptr) : BlockPtr(std::move(Ptr)) {
  Ptr.Pointee->movePointer(&Ptr, &BlockPtr);
}

Pointer::Storage::Storage(const Storage &S) {
  switch (S.K) {
  case PointerKind::TargetPtr:
    new (&TargetPtr) TargetPointer(S.TargetPtr);
    break;
  case PointerKind::BlockPtr:
    new (&BlockPtr) BlockPointer(S.BlockPtr);
    BlockPtr.Pointee->addPointer(&BlockPtr);
    break;
  case PointerKind::ExternPtr:
    new (&ExternPtr) ExternPointer(S.ExternPtr);
    break;
  case PointerKind::TypeInfoPtr:
    new (&TypeInfoPtr) TypeInfoPointer(S.TypeInfoPtr);
    break;
  case PointerKind::InvalidPtr:
    K = PointerKind::InvalidPtr;
    break;
  }
}

Pointer::Storage::Storage(Storage &&S) {
  switch (S.K) {
  case PointerKind::TargetPtr:
    new (&TargetPtr) TargetPointer(std::move(S.TargetPtr));
    break;
  case PointerKind::BlockPtr:
    new (&BlockPtr) BlockPointer(std::move(S.BlockPtr));
    BlockPtr.Pointee->movePointer(&S.BlockPtr, &BlockPtr);
    break;
  case PointerKind::ExternPtr:
    new (&ExternPtr) ExternPointer(std::move(S.ExternPtr));
    break;
  case PointerKind::TypeInfoPtr:
    new (&TypeInfoPtr) TypeInfoPointer(std::move(S.TypeInfoPtr));
    break;
  case PointerKind::InvalidPtr:
    K = PointerKind::InvalidPtr;
    break;
  }
}

Pointer::Storage::~Storage() {
  switch (ExternPtr.K) {
  case PointerKind::TargetPtr:
    TargetPtr.~TargetPointer();
    break;
  case PointerKind::BlockPtr:
    BlockPtr.Pointee->removePointer(&BlockPtr);
    BlockPtr.Pointee->cleanup();
    BlockPtr.~BlockPointer();
    break;
  case PointerKind::ExternPtr:
    ExternPtr.~ExternPointer();
    break;
  case PointerKind::TypeInfoPtr:
    TypeInfoPtr.~TypeInfoPointer();
    break;
  case PointerKind::InvalidPtr:
    break;
  }
}
