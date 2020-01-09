//===--- InterpHelper.cpp - Auxiliary utilities -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InterpHelper.h"
#include "InterpState.h"
#include "PrimType.h"
#include "Record.h"
#include "clang/AST/Type.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APSInt.h"

using namespace clang;
using namespace interp;

//===----------------------------------------------------------------------===//
// Pointer to APValue conversion
//===----------------------------------------------------------------------===//

static llvm::APSInt GetInt(PrimType Ty, const BlockPointer &Ptr) {
  INT_TYPE_SWITCH(Ty, return Ptr.deref<T>().toAPSInt());
}

static llvm::APFloat GetFloat(const BlockPointer &Ptr) {
  return Ptr.deref<Real>().toAPFloat();
}

static bool PointerToValue(const ASTContext &Ctx, QualType Ty,
                           const BlockPointer &Ptr, APValue &R);

static bool RecordToValue(const ASTContext &Ctx, const BlockPointer &Ptr,
                          APValue &R) {
  auto *Record = Ptr.getRecord();
  assert(Record && "Missing record descriptor");

  bool Ok = true;
  if (Record->isUnion()) {
    const FieldDecl *ActiveField = nullptr;
    APValue Value;
    for (auto &F : Record->fields()) {
      const BlockPointer &FP = Ptr.atField(F.getOffset());
      if (FP.isActive()) {
        if (llvm::Optional<PrimType> T = F.getDesc()->ElemTy) {
          TYPE_SWITCH(*T, {
            if (!FP.deref<T>().toAPValue(Ctx, Value))
              return false;
          });
        } else {
          Ok &= PointerToValue(Ctx, F.getDesc()->getType(), FP, Value);
        }
        break;
      }
    }
    R = APValue(ActiveField, Value);
  } else {
    unsigned NF = Record->getNumFields();
    unsigned NB = Record->getNumBases();
    unsigned NV = Ptr.isBaseClass() ? 0 : Record->getNumVirtualBases();

    R = APValue(APValue::UninitStruct(), NB, NF);

    for (unsigned I = 0; I < NF; ++I) {
      const Record::Field *FD = Record->getField(I);
      const BlockPointer &FP = Ptr.atField(FD->getOffset());
      APValue &Value = R.getStructField(I);

      if (llvm::Optional<PrimType> T = FD->getDesc()->elementType()) {
        TYPE_SWITCH(*T, {
          if (!FP.deref<T>().toAPValue(Ctx, Value))
            return false;
        });
      } else {
        Ok &= PointerToValue(Ctx, FD->getDecl()->getType(), FP, Value);
      }
    }

    for (unsigned I = 0; I < NB; ++I) {
      const Record::Base *BD = Record->getBase(I);
      const BlockPointer &BP = Ptr.atBase(BD->getOffset(), /*isVirtual=*/false);
      Ok &= RecordToValue(Ctx, BP, R.getStructBase(I));
    }

    for (unsigned I = 0; I < NV; ++I) {
      const Record::Base *VD = Record->getVirtualBase(I);
      const BlockPointer &VP = Ptr.atBase(VD->getOffset(), /*isVirtual=*/true);
      Ok &= RecordToValue(Ctx, VP, R.getStructBase(NB + I));
    }
  }
  return Ok;
}

static bool ArrayToValue(const ASTContext &Ctx, QualType ElemTy,
                         const BlockPointer &Ptr, APValue &R) {
  const size_t NumElems = Ptr.getNumElems();
  R = APValue(APValue::UninitArray{}, NumElems, NumElems);

  bool Ok = true;
  for (unsigned I = 0; I < NumElems; ++I) {
    APValue &Slot = R.getArrayInitializedElt(I);
    const BlockPointer &EP = Ptr.atIndex(I);
    if (llvm::Optional<PrimType> T = Ptr.elementType()) {
      TYPE_SWITCH(*T, {
        if (!EP.deref<T>().toAPValue(Ctx, Slot))
          return false;
      });
    } else {
      Ok &= PointerToValue(Ctx, ElemTy, EP, Slot);
    }
  }
  return Ok;
}

static bool ComplexToValue(const ASTContext &Ctx, const BlockPointer &Ptr,
                           APValue &R) {
  const BlockPointer &PtrReal = Ptr.atIndex(0);
  const BlockPointer &PtrImag = Ptr.atIndex(1);
  if (llvm::Optional<PrimType> T = Ptr.elementType()) {
    switch (*T) {
    case PT_Sint8:
    case PT_Uint8:
    case PT_Sint16:
    case PT_Uint16:
    case PT_Sint32:
    case PT_Uint32:
    case PT_Sint64:
    case PT_Uint64:
    case PT_SintFP:
    case PT_UintFP:
      R = APValue(GetInt(*T, PtrReal), GetInt(*T, PtrImag));
      return true;

    case PT_RealFP:
      R = APValue(GetFloat(PtrReal), GetFloat(PtrImag));
      return true;

    default:
      break;
    }
  }
  llvm_unreachable("invalid complex element type");
}

static bool VectorToValue(const ASTContext &Ctx, const BlockPointer &Ptr,
                          APValue &R) {
  if (llvm::Optional<PrimType> T = Ptr.elementType()) {
    SmallVector<APValue, 4> Elems;
    for (unsigned I = 0, N = Ptr.getNumElems(); I < N; ++I) {
      const BlockPointer &ElemPtr = Ptr.atIndex(I);
      TYPE_SWITCH(*T, {
        APValue Temp;
        if (!ElemPtr.deref<T>().toAPValue(Ctx, Temp))
          return false;
        Elems.push_back(Temp);
      });
    }
    R = APValue(Elems.data(), Elems.size());
    return true;
  }
  llvm_unreachable("invalid vector element type");
}

static bool PointerToValue(const ASTContext &Ctx, QualType Ty,
                           const BlockPointer &Ptr, APValue &R) {
  if (auto *AT = Ty->getAs<AtomicType>())
    Ty = AT->getValueType();

  if (Ty->getAs<RecordType>())
    return RecordToValue(Ctx, Ptr, R);

  if (auto *AT = Ty->getAsArrayTypeUnsafe())
    return ArrayToValue(Ctx, AT->getElementType(), Ptr.decay(), R);

  if (Ty->getAs<ComplexType>())
    return ComplexToValue(Ctx, Ptr.decay(), R);

  if (Ty->getAs<VectorType>())
    return VectorToValue(Ctx, Ptr.decay(), R);

  llvm_unreachable("invalid pointer to convert");
}

//===----------------------------------------------------------------------===//
// Pointer deep copy
//===----------------------------------------------------------------------===//

static bool CopyRecord(InterpState &S, CodePtr PC, const BlockPointer &From,
                       const BlockPointer &To) {
  auto *R = From.getRecord();
  for (auto &Field : R->fields()) {
    unsigned Offset = Field.getOffset();
    const auto &ToField = To.atField(Offset);
    const auto &FromField = From.atField(Offset);
    if (!Copy(S, PC, FromField, ToField))
      return false;
  }

  for (auto &Base : R->bases()) {
    unsigned Offset = Base.getOffset();
    const auto &ToField = To.atBase(Offset, /*isVirtual=*/false);
    const auto &FromField = From.atBase(Offset, /*isVirtual=*/false);
    if (!CopyRecord(S, PC, FromField, ToField))
      return false;
    ToField.initialize();
  }

  if (!From.isBaseClass()) {
    for (auto &Virt : R->virtual_bases()) {
      unsigned Offset = Virt.getOffset();
      const auto &ToField = To.atBase(Offset, /*isVirtual=*/true);
      const auto &FromField = From.atBase(Offset, /*isVirtual=*/true);
      if (!CopyRecord(S, PC, FromField, ToField))
        return false;
      ToField.initialize();
    }
  }

  return true;
}

static bool CopyArray(InterpState &S, CodePtr PC, const BlockPointer &From,
                      const BlockPointer &To) {
  if (llvm::Optional<PrimType> T = From.elementType()) {
    for (unsigned I = 0; I < From.getNumElems(); ++I) {
      const auto &FromField = From.atIndex(I);
      const auto &ToField = To.atIndex(I);
      TYPE_SWITCH(*T, ToField.deref<T>() = FromField.deref<T>());
      ToField.initialize();
    }
  } else {
    for (unsigned I = 0; I < From.getNumElems(); ++I) {
      const auto &FromField = From.atIndex(I);
      if (!S.CheckLoad(PC, FromField, AK_Read))
        return false;
      const auto &ToField = To.atIndex(I);
      if (!Copy(S, PC, FromField, ToField))
        return false;
      ToField.initialize();
      ToField.activate();
    }
  }
  return true;
}

template <typename PtrTy>
Record *Lookup(PtrTy &This, const MemberPointer &Field) {
  Record *R = This.getRecord();
  // Get the offset inside the record.
  if (Field.isDerived()) {
    // Walk down the chain to derived classes.
    for (const CXXRecordDecl *Step : Field.steps()) {
      if (!This.isBaseClass())
        return nullptr;

      This = This.getBase();
      R = This.getRecord();

      if (R->getDecl() != Step)
        return nullptr;
    }
  } else {
    // Walk up the chain of base classes.
    for (const CXXRecordDecl *Step : Field.steps()) {
      This = This.atBase(R->getBase(Step)->getOffset(), /*isVirtual=*/false);
      R = This.getRecord();
    }
  }
  return R;
}

namespace clang {
namespace interp {

//===----------------------------------------------------------------------===//
// Pointer Return Check
//===----------------------------------------------------------------------===//

bool PointerToValue(const ASTContext &Ctx, const Pointer &Ptr,
                    APValue &Result) {
  if (const BlockPointer *BlockPtr = Ptr.asBlock()) {
    return ::PointerToValue(Ctx, BlockPtr->getDeclType(), *BlockPtr, Result);
  }
  llvm_unreachable("cannot convert to value");
}

bool CopyUnion(InterpState &S, CodePtr PC, const BlockPointer &From,
               const BlockPointer &To) {
  for (auto &Field : From.getRecord()->fields()) {
    unsigned Offset = Field.getOffset();
    const auto &FromField = From.atField(Offset);
    const auto &ToField = To.atField(Offset);
    if (!FromField.isActive())
      continue;
    if (!Copy(S, PC, FromField, ToField))
      return false;
    ToField.activate();
    return true;
  }
  return true;
}

bool Copy(InterpState &S, CodePtr PC, const BlockPointer &From,
          const BlockPointer &To) {
  if (From.inArray())
    return CopyArray(S, PC, From.decay(), To.decay());

  if (Record *R = From.getRecord()) {
    if (R->isUnion())
      return CopyUnion(S, PC, From, To);
    else
      return CopyRecord(S, PC, From, To);
  }

  if (llvm::Optional<PrimType> T = From.elementType()) {
    if (!S.CheckLoad(PC, From, AK_Read))
      return false;
    TYPE_SWITCH(*T, To.deref<T>() = From.deref<T>());
    To.initialize();
    To.activate();
    return true;
  }

  llvm_unreachable("invalid pointer to copy");
}

Record *PointerLookup(Pointer &This, const MemberPointer &Field) {
  return Lookup(This, Field);
}

const CXXMethodDecl *VirtualLookup(BlockPointer &This,
                                   const CXXMethodDecl *MD) {
  // Traverse the chain of derived classes, from base to derived.
  auto *Impl = MD;
  while (This.isBaseClass() && This.isInitialized()) {
    // Step into the derived class.
    This = This.getBase();

    // Fetch the class declaration.
    Record *R = This.getRecord();
    assert(R != nullptr && "virtual dispatch allowed only on records");
    auto *Class = dyn_cast<CXXRecordDecl>(R->getDecl());
    assert(Class != nullptr && "not a CXX record");

    // Find the most overriden method.
    auto *Override = MD->getCorrespondingMethodDeclaredInClass(Class, false);
    if (Override) {
      Impl = Override;
    }
  }
  return Impl;
}

const CXXMethodDecl *IndirectLookup(BlockPointer &This,
                                    const MemberPointer &Ptr) {
  // Find the member and create a function pointer.
  if (auto *Decl = dyn_cast<CXXMethodDecl>(Ptr.getDecl())) {
    if (Lookup(This, Ptr)) {
      if (!Decl->isVirtual())
        return Decl;
      return VirtualLookup(This, Decl);
    }
  }
  // Not found - bail out.
  return nullptr;
}

} // namespace interp
} // namespace clang
