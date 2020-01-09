//===--- TypeHelper.cpp - Type inspection methods ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TypeHelper.h"

namespace clang {
namespace interp {

GCCTypeClass evaluateBuiltinClassifyType(QualType T,
                                         const LangOptions &LangOpts) {
  assert(!T->isDependentType() && "unexpected dependent type");

  QualType CanTy = T.getCanonicalType();
  const BuiltinType *BT = dyn_cast<BuiltinType>(CanTy);

  switch (CanTy->getTypeClass()) {
#define TYPE(ID, BASE)
#define DEPENDENT_TYPE(ID, BASE) case Type::ID:
#define NON_CANONICAL_TYPE(ID, BASE) case Type::ID:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(ID, BASE) case Type::ID:
#include "clang/AST/TypeNodes.inc"
  case Type::Auto:
  case Type::DeducedTemplateSpecialization:
    llvm_unreachable("unexpected non-canonical or dependent type");

  case Type::Builtin:
    switch (BT->getKind()) {
#define BUILTIN_TYPE(ID, SINGLETON_ID)
#define SIGNED_TYPE(ID, SINGLETON_ID)                                          \
  case BuiltinType::ID:                                                        \
    return GCCTypeClass::Integer;
#define FLOATING_TYPE(ID, SINGLETON_ID)                                        \
  case BuiltinType::ID:                                                        \
    return GCCTypeClass::RealFloat;
#define PLACEHOLDER_TYPE(ID, SINGLETON_ID)                                     \
  case BuiltinType::ID:                                                        \
    break;
#include "clang/AST/BuiltinTypes.def"
    case BuiltinType::Void:
      return GCCTypeClass::Void;

    case BuiltinType::Bool:
      return GCCTypeClass::Bool;

    case BuiltinType::Char_U:
    case BuiltinType::UChar:
    case BuiltinType::WChar_U:
    case BuiltinType::Char8:
    case BuiltinType::Char16:
    case BuiltinType::Char32:
    case BuiltinType::UShort:
    case BuiltinType::UInt:
    case BuiltinType::ULong:
    case BuiltinType::ULongLong:
    case BuiltinType::UInt128:
      return GCCTypeClass::Integer;

    case BuiltinType::UShortAccum:
    case BuiltinType::UAccum:
    case BuiltinType::ULongAccum:
    case BuiltinType::UShortFract:
    case BuiltinType::UFract:
    case BuiltinType::ULongFract:
    case BuiltinType::SatUShortAccum:
    case BuiltinType::SatUAccum:
    case BuiltinType::SatULongAccum:
    case BuiltinType::SatUShortFract:
    case BuiltinType::SatUFract:
    case BuiltinType::SatULongFract:
      return GCCTypeClass::None;

    case BuiltinType::NullPtr:

    case BuiltinType::ObjCId:
    case BuiltinType::ObjCClass:
    case BuiltinType::ObjCSel:
#define IMAGE_TYPE(ImgType, Id, SingletonId, Access, Suffix)                   \
  case BuiltinType::Id:
#include "clang/Basic/OpenCLImageTypes.def"
#define EXT_OPAQUE_TYPE(ExtType, Id, Ext) case BuiltinType::Id:
#include "clang/Basic/OpenCLExtensionTypes.def"
    case BuiltinType::OCLSampler:
    case BuiltinType::OCLEvent:
    case BuiltinType::OCLClkEvent:
    case BuiltinType::OCLQueue:
    case BuiltinType::OCLReserveID:
#define SVE_TYPE(Name, Id, SingletonId) case BuiltinType::Id:
#include "clang/Basic/AArch64SVEACLETypes.def"
      return GCCTypeClass::None;

    case BuiltinType::Dependent:
      llvm_unreachable("unexpected dependent type");
    };
    llvm_unreachable("unexpected placeholder type");

  case Type::Enum:
    return LangOpts.CPlusPlus ? GCCTypeClass::Enum : GCCTypeClass::Integer;

  case Type::Pointer:
  case Type::ConstantArray:
  case Type::VariableArray:
  case Type::IncompleteArray:
  case Type::FunctionNoProto:
  case Type::FunctionProto:
    return GCCTypeClass::Pointer;

  case Type::MemberPointer:
    return CanTy->isMemberDataPointerType()
               ? GCCTypeClass::PointerToDataMember
               : GCCTypeClass::PointerToMemberFunction;

  case Type::Complex:
    return GCCTypeClass::Complex;

  case Type::Record:
    return CanTy->isUnionType() ? GCCTypeClass::Union
                                : GCCTypeClass::ClassOrStruct;

  case Type::Atomic:
    // GCC classifies _Atomic T the same as T.
    return evaluateBuiltinClassifyType(
        CanTy->castAs<AtomicType>()->getValueType(), LangOpts);

  case Type::BlockPointer:
  case Type::Vector:
  case Type::ExtVector:
  case Type::ObjCObject:
  case Type::ObjCInterface:
  case Type::ObjCObjectPointer:
  case Type::Pipe:
    // GCC classifies vectors as None. We follow its lead and classify all
    // other types that don't fit into the regular classification the same way.
    return GCCTypeClass::None;

  case Type::LValueReference:
  case Type::RValueReference:
    llvm_unreachable("invalid type for expression");
  }

  llvm_unreachable("unexpected type class");
}

CharUnits getAlignOfType(ASTContext &Ctx, QualType T,
                         UnaryExprOrTypeTrait ExprKind) {
  // C++ [expr.alignof]p3:
  //     When alignof is applied to a reference type, the result is the
  //     alignment of the referenced type.
  if (const ReferenceType *Ref = T->getAs<ReferenceType>())
    T = Ref->getPointeeType();

  if (T.getQualifiers().hasUnaligned())
    return CharUnits::One();

  const bool AlignOfReturnsPreferred =
      Ctx.getLangOpts().getClangABICompat() <= LangOptions::ClangABI::Ver7;

  // __alignof is defined to return the preferred alignment.
  // Before 8, clang returned the preferred alignment for alignof and _Alignof
  // as well.
  if (ExprKind == UETT_PreferredAlignOf || AlignOfReturnsPreferred)
    return Ctx.toCharUnitsFromBits(Ctx.getPreferredTypeAlign(T.getTypePtr()));
  // alignof and _Alignof are defined to return the ABI alignment.
  else if (ExprKind == UETT_AlignOf)
    return Ctx.getTypeAlignInChars(T.getTypePtr());
  else
    llvm_unreachable("GetAlignOfType on a non-alignment ExprKind");
}

CharUnits getAlignOfExpr(ASTContext &Ctx, const Expr *E,
                         UnaryExprOrTypeTrait ExprKind) {
  E = E->IgnoreParens();

  // The kinds of expressions that we have special-case logic here for
  // should be kept up to date with the special checks for those
  // expressions in Sema.

  // alignof decl is always accepted, even if it doesn't make sense: we default
  // to 1 in those cases.
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
    return Ctx.getDeclAlign(DRE->getDecl(),
                            /*RefAsPointee*/ true);

  if (const MemberExpr *ME = dyn_cast<MemberExpr>(E))
    return Ctx.getDeclAlign(ME->getMemberDecl(),
                            /*RefAsPointee*/ true);

  return getAlignOfType(Ctx, E->getType(), ExprKind);
}

} // namespace interp
} // namespace clang
