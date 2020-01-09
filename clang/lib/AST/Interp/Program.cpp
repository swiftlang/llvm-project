//===--- Program.cpp - Bytecode for the constexpr VM ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Program.h"
#include "ByteCodeStmtGen.h"
#include "Context.h"
#include "Function.h"
#include "Opcode.h"
#include "PrimType.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"

using namespace clang;
using namespace clang::interp;

unsigned Program::createGlobalString(const Expr *E, const StringLiteral *S) {
  const size_t CharWidth = S->getCharByteWidth();
  const size_t BitWidth = CharWidth * Ctx.getCharBit();
  CharUnits StringLength = CharUnits::fromQuantity(S->getByteLength());

  PrimType CharType;
  switch (CharWidth) {
  case 1:
    CharType = PT_Sint8;
    break;
  case 2:
    CharType = PT_Uint16;
    break;
  case 4:
    CharType = PT_Uint32;
    break;
  default:
    llvm_unreachable("unsupported character width");
  }

  // Create a descriptor for the string.
  Descriptor *Desc = allocateDescriptor(E, CharType, S->getLength() + 1,
                                        /*isConst=*/true,
                                        /*isTemporary=*/false,
                                        /*isMutable=*/false,
                                        StringLength);

  // Allocate storage for the string.
  // The byte length does not include the null terminator.
  unsigned I = Globals.size();
  unsigned Sz = Desc->getAllocSize();
  auto *G = new (Allocator, Sz) Global(Desc, /*isStatic=*/true);
  Globals.push_back(G);

  // Construct the string in storage.
  Pointer Ptr(Pointer(G->block()).decay());
  for (unsigned I = 0, N = S->getLength(); I <= N; ++I) {
    Pointer Field(Ptr.atIndex(I));
    const uint32_t CodePoint = I == N ? 0 : S->getCodeUnit(I);
    switch (CharType) {
    case PT_Sint8: {
      using T = PrimConv<PT_Sint8>::T;
      Field.asBlock()->deref<T>() = T::from(CodePoint, BitWidth);
      break;
    }
    case PT_Uint16: {
      using T = PrimConv<PT_Uint16>::T;
      Field.asBlock()->deref<T>() = T::from(CodePoint, BitWidth);
      break;
    }
    case PT_Uint32: {
      using T = PrimConv<PT_Uint32>::T;
      Field.asBlock()->deref<T>() = T::from(CodePoint, BitWidth);
      break;
    }
    default:
      llvm_unreachable("unsupported character type");
    }
  }
  return I;
}

unsigned Program::createGlobalString(StringRef Str) {
  const ASTContext &ASTCtx = Ctx.getASTContext();
  StringLiteral *Res = ASTCtx.getPredefinedStringLiteralFromCache(Str);
  return createGlobalString(Res, Res);
}


Pointer Program::getPtrGlobal(const VarDecl *VD) {
  auto It = GlobalIndices.find(VD);
  assert(It != GlobalIndices.end() && "missing global");
  return getPtrGlobal(GlobalLocation(It->second));
}

Block *Program::getGlobal(GlobalLocation Idx) {
  assert(Idx.Index < Globals.size());
  if (auto *G = Globals[Idx.Index].dyn_cast<Global *>())
    return G->block();
  if (auto *VD = Globals[Idx.Index].dyn_cast<const VarDecl *>()) {
    Global *G = allocateGlobal(VD, VD->getType(), !VD->hasLocalStorage());
    assert(G && "cannot create global");
    Globals[Idx.Index] = G;
    if (auto T = Ctx.classify(VD->getType())) {
      Block *B = G->block();

      // Find a value from any of the redeclarations.
      APValue *Val;
      for (const VarDecl *Decl : VD->redecls()) {
        Val = Decl->getEvaluatedValue();
        if (Val)
          break;
      }

      // Set the block to that value.
      if (Val && Val->isInt()) {
        switch (*T) {
        INTEGRAL_CASES({
          new (&B->deref<T>()) T(T::from(Val->getInt().getExtValue()));
        })
        default:
          return nullptr;
        }
      }
      return B;
    }
    return nullptr;
  }
  llvm_unreachable("invalid global");
}

llvm::Optional<GlobalLocation> Program::getGlobal(const VarDecl *VD) {
  auto It = GlobalIndices.find(VD);
  if (It != GlobalIndices.end())
    return GlobalLocation(It->second);
  // Find any previous declarations which were aleady evaluated.
  for (const Decl *P = VD; P; P = P->getPreviousDecl()) {
    auto It = GlobalIndices.find(P);
    if (It != GlobalIndices.end()) {
      unsigned Index = It->second;
      GlobalIndices[VD] = Index;
      return GlobalLocation(Index);
    }
  }
  return {};
}

llvm::Optional<GlobalLocation> Program::createGlobal(const VarDecl *VD) {
  if (auto G = createGlobal(VD, VD->getType(), !VD->hasLocalStorage())) {
    for (const Decl *P = VD; P; P = P->getPreviousDecl()) {
      GlobalIndices[P] = G->Index;
    }
    return G;
  }
  return {};
}

llvm::Optional<GlobalLocation> Program::prepareGlobal(const VarDecl *VD) {
  unsigned I = Globals.size();
  for (const Decl *P = VD; P; P = P->getPreviousDecl()) {
    GlobalIndices[P] = I;
  }
  Globals.push_back(VD);
  return GlobalLocation(I);
}

llvm::Optional<GlobalLocation> Program::createGlobal(const Expr *E,
                                                     QualType Ty) {
  return createGlobal(E, Ty, /*isStatic=*/true);
}

llvm::Optional<GlobalLocation> Program::createGlobal(const DeclTy &D,
                                                     QualType Ty,
                                                     bool IsStatic) {
  if (auto *G = allocateGlobal(D, Ty, IsStatic)) {
    unsigned I = Globals.size();
    Globals.push_back(G);
    return GlobalLocation(I);
  }
  return {};
}

Program::Global *Program::allocateGlobal(const DeclTy &D, QualType Ty, bool IsStatic) {
  // Create a descriptor for the global.
  Descriptor *Desc;
  const bool IsConst = Ty.isConstQualified();
  const bool IsTemporary = D.dyn_cast<const Expr *>();
  if (auto T = Ctx.classify(Ty)) {
    Desc = createDescriptor(D, Ty.getTypePtr(), *T, IsConst, IsTemporary);
  } else {
    Desc = createDescriptor(D, Ty.getTypePtr(), IsConst, IsTemporary);
  }
  if (!Desc)
    return nullptr;

  // Allocate a block for storage.
  auto *G = new (Allocator, Desc->getAllocSize())
      Global(getCurrentDecl(), Desc, IsStatic);
  G->block()->invokeCtor();
  return G;
}

void Program::removeGlobal(const VarDecl *VD) {
  GlobalIndices.erase(GlobalIndices.find(VD));
}

Function *Program::getFunction(const FunctionDecl *F) {
  F = F->getDefinition();
  auto It = Funcs.find(F);
  return It == Funcs.end() ? nullptr : It->second.get();
}

llvm::Expected<Function *> Program::getOrCreateFunction(State &S,
                                                        const FunctionDecl *F) {
  if (Function *Func = getFunction(F)) {
    return Func;
  }

  // Try to compile the function if it wasn't compiled yet.
  if (const FunctionDecl *FD = F->getDefinition())
    return ByteCodeStmtGen<ByteCodeEmitter>(Ctx, *this, S).compileFunc(FD);

  // A relocation which traps if not resolved.
  return nullptr;
}

Record *Program::getOrCreateRecord(const RecordDecl *RD) {
  // Use the actual definition as a key.
  RD = RD->getDefinition();
  if (!RD || RD->isInvalidDecl())
    return nullptr;

  // Deduplicate records.
  auto It = Records.find(RD);
  if (It != Records.end()) {
    return It->second;
  }

  // Number of bytes required by fields and base classes.
  unsigned Size = 0;
  // Number of bytes required by virtual base.
  unsigned VSize = 0;

  // Reserve space for base classes and register them as fields of the object.
  Record::BaseList Bases;
  Record::VirtualBaseList VBases;
  const ASTContext &ASTCtx = Ctx.getASTContext();
  if (auto *CD = dyn_cast<CXXRecordDecl>(RD)) {
    for (const CXXBaseSpecifier &Spec : CD->bases()) {
      if (Spec.isVirtual())
        continue;

      QualType Ty = Spec.getType();
      auto *BD = dyn_cast<CXXRecordDecl>(Ty->castAs<RecordType>()->getDecl());
      Record *BR = getOrCreateRecord(BD);
      if (!BR)
        return nullptr;

      Descriptor *Desc = allocateDescriptor(BD, BR, /*isConst=*/false,
                                            /*isTemporary=*/false,
                                            /*isMutable=*/false,
                                            ASTCtx.getTypeSizeInChars(Ty));
      if (!Desc)
        return nullptr;


      Size += align(sizeof(InlineDescriptor));
      Bases.push_back({BD, Size, RD, &ASTCtx, BR, Desc, /*isVirtual=*/false});
      Size += align(BR->getSize());
    }

    for (const CXXBaseSpecifier &Spec : CD->vbases()) {
      QualType Ty = Spec.getType();
      auto *BD = dyn_cast<CXXRecordDecl>(Ty->castAs<RecordType>()->getDecl());
      Record *BR = getOrCreateRecord(BD);
      if (!BR)
        return nullptr;

      Descriptor *Desc = allocateDescriptor(BD, BR, /*isConst=*/false,
                                            /*isTemporary=*/false,
                                            /*isMutable=*/false,
                                            ASTCtx.getTypeSizeInChars(Ty));
      if (!Desc)
        return nullptr;

      VSize += align(sizeof(InlineDescriptor));
      VBases.push_back({BD, VSize, RD, &ASTCtx, BR, Desc, /*isVirtual=*/true});
      VSize += align(BR->getSize());
    }
  }

  // Reserve space for fields.
  Record::FieldList Fields;
  for (const FieldDecl *FD : RD->fields()) {
    // Do not assign space and metadata for invalid declarations.
    if (FD->isInvalidDecl())
      continue;

    // Reserve space for the field's descriptor and the offset.
    Size += align(sizeof(InlineDescriptor));

    // Classify the field and add its metadata.
    QualType FT = FD->getType();
    const bool IsConst = FT.isConstQualified();
    const bool IsMutable = FD->isMutable();
    Descriptor *Desc;
    if (llvm::Optional<PrimType> T = Ctx.classify(FT)) {
      Desc = createDescriptor(FD, FT.getTypePtr(), *T, IsConst,
                              /*isTemporary=*/false, IsMutable);
    } else {
      Desc = createDescriptor(FD, FT.getTypePtr(), IsConst,
                              /*isTemporary=*/false, IsMutable);
    }
    if (!Desc)
      return nullptr;

    Fields.push_back({FD, Size, RD, &ASTCtx, Desc});
    Size += align(Desc->getAllocSize());
  }

  Record *R = new (Allocator) Record(RD, std::move(Bases), std::move(Fields),
                                     std::move(VBases), VSize, Size);
  Records.insert({RD, R});
  return R;
}

Descriptor *Program::createDescriptor(const DeclTy &D, const Type *Ty,
                                      PrimType Type, bool IsConst,
                                      bool IsTemporary, bool IsMutable) {
  return allocateDescriptor(D, Type, IsConst, IsTemporary, IsMutable,
                            Ctx.getASTContext().getTypeSizeInChars(Ty));
}

Descriptor *Program::createDescriptor(const DeclTy &D, const Type *Ty,
                                      bool IsConst, bool IsTemporary,
                                      bool IsMutable) {
  const ASTContext &ASTCtx = Ctx.getASTContext();


  // Classes and structures.
  if (auto *RT = Ty->getAs<RecordType>())
    if (auto *Record = getOrCreateRecord(RT->getDecl()))
      return allocateDescriptor(D, Record, IsConst, IsTemporary, IsMutable,
                                ASTCtx.getTypeSizeInChars(Ty));

  // Arrays.
  if (auto ArrayType = Ty->getAsArrayTypeUnsafe()) {
    QualType ElemTy = ArrayType->getElementType();

    // Array of unknown bounds - cannot be accessed and pointer arithmetic
    // is forbidden on pointers to such objects.
    if (isa<IncompleteArrayType>(ArrayType)) {
      if (llvm::Optional<PrimType> T = Ctx.classify(ElemTy)) {
        return allocateDescriptor(D, *T, IsTemporary,
                                  Descriptor::UnknownSize{},
                                  ASTCtx.getTypeSizeInChars(ElemTy));
      } else {
        Descriptor *Desc =
            createDescriptor(D, ElemTy.getTypePtr(), IsConst, IsTemporary);
        if (!Desc)
          return nullptr;
        return allocateDescriptor(D, Desc, IsTemporary,
                                  Descriptor::UnknownSize{},
                                  ASTCtx.getTypeSizeInChars(ElemTy));
      }
    }

    // Array of known bounds.
    if (auto CAT = dyn_cast<ConstantArrayType>(ArrayType)) {
      size_t NumElems = CAT->getSize().getZExtValue();
      if (llvm::Optional<PrimType> T = Ctx.classify(ElemTy)) {
        // Arrays of primitives.
        unsigned ElemSize = primSize(*T);
        if (std::numeric_limits<unsigned>::max() / ElemSize <= NumElems) {
          return {};
        }
        return allocateDescriptor(D, *T, NumElems, IsConst, IsTemporary,
                                  IsMutable, ASTCtx.getTypeSizeInChars(Ty));
      } else {
        // Arrays of composites. In this case, the array is a list of pointers,
        // followed by the actual elements.
        Descriptor *Desc =
            createDescriptor(D, ElemTy.getTypePtr(), IsConst, IsTemporary);
        if (!Desc)
          return nullptr;
        InterpUnits ElemSize = Desc->getAllocSize() + sizeof(InlineDescriptor);
        if (std::numeric_limits<unsigned>::max() / ElemSize <= NumElems)
          return {};
        return allocateDescriptor(D, Desc, NumElems, IsConst, IsTemporary,
                                  IsMutable, ASTCtx.getTypeSizeInChars(Ty));
      }
    }
  }

  // Atomic types.
  if (auto *AT = Ty->getAs<AtomicType>())
    return createDescriptor(D, AT->getValueType().getTypePtr(), IsConst,
                            IsTemporary, IsMutable);

  // Complex types - represented as arrays of elements.
  if (auto *CT = Ty->getAs<ComplexType>())
    return allocateDescriptor(D, *Ctx.classify(CT->getElementType()), 2,
                              IsConst, IsTemporary, IsMutable,
                              ASTCtx.getTypeSizeInChars(Ty));

  // Vector types - represented as arrays of elements.
  if (auto *VT = Ty->getAs<VectorType>())
    return allocateDescriptor(D, *Ctx.classify(VT->getElementType()),
                              VT->getNumElements(), IsConst, IsTemporary,
                              IsMutable, ASTCtx.getTypeSizeInChars(Ty));

  return nullptr;
}

Descriptor *Program::createDescriptor(const DeclTy &D, QualType Ty) {
  if (llvm::Optional<PrimType> T = Ctx.classify(Ty)) {
    return createDescriptor(D, Ty.getTypePtr(), *T, Ty.isConstQualified());
  } else {
    return createDescriptor(D, Ty.getTypePtr(), Ty.isConstQualified());
  }
}
