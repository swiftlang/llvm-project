//===--- PrimType.h - Types for the constexpr VM ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the VM types and helpers operating on types.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_PRIMTYPE_H
#define LLVM_CLANG_AST_INTERP_PRIMTYPE_H

#include "Boolean.h"
#include "FixedIntegral.h"
#include "FnPointer.h"
#include "Integral.h"
#include "MemberPointer.h"
#include "ObjCBlockPointer.h"
#include "Pointer.h"
#include "Real.h"
#include "VoidPointer.h"
#include <climits>
#include <cstddef>
#include <cstdint>

namespace clang {
namespace interp {

/// Enumeration of the primitive types of the VM.
enum PrimType : unsigned {
  PT_Sint8,
  PT_Uint8,
  PT_Sint16,
  PT_Uint16,
  PT_Sint32,
  PT_Uint32,
  PT_Sint64,
  PT_Uint64,
  PT_SintFP,
  PT_UintFP,
  PT_RealFP,
  PT_Bool,
  PT_Ptr,
  PT_FnPtr,
  PT_MemPtr,
  PT_VoidPtr,
  PT_ObjCBlockPtr,
};

/// Mapping from primitive types to their representation.
template <PrimType T> struct PrimConv;
template <> struct PrimConv<PT_Sint8> { using T = Integral<8, true>; };
template <> struct PrimConv<PT_Uint8> { using T = Integral<8, false>; };
template <> struct PrimConv<PT_Sint16> { using T = Integral<16, true>; };
template <> struct PrimConv<PT_Uint16> { using T = Integral<16, false>; };
template <> struct PrimConv<PT_Sint32> { using T = Integral<32, true>; };
template <> struct PrimConv<PT_Uint32> { using T = Integral<32, false>; };
template <> struct PrimConv<PT_Sint64> { using T = Integral<64, true>; };
template <> struct PrimConv<PT_Uint64> { using T = Integral<64, false>; };
template <> struct PrimConv<PT_SintFP> { using T = FixedIntegral<true>; };
template <> struct PrimConv<PT_UintFP> { using T = FixedIntegral<false>; };
template <> struct PrimConv<PT_RealFP> { using T = Real; };
template <> struct PrimConv<PT_Bool> { using T = Boolean; };
template <> struct PrimConv<PT_Ptr> { using T = Pointer; };
template <> struct PrimConv<PT_FnPtr> { using T = FnPointer; };
template <> struct PrimConv<PT_MemPtr> { using T = MemberPointer; };
template <> struct PrimConv<PT_VoidPtr> { using T = VoidPointer; };
template <> struct PrimConv<PT_ObjCBlockPtr> { using T = ObjCBlockPointer; };

/// Returns the size of a primitive type in bytes.
size_t primSize(PrimType Type);

/// Aligns a size to the pointer alignment.
constexpr size_t align(size_t Size) {
  return ((Size + alignof(void *) - 1) / alignof(void *)) * alignof(void *);
}

constexpr bool isPrimitiveIntegral(PrimType Type) {
  switch (Type) {
  case PT_Bool:
  case PT_Sint8:
  case PT_Uint8:
  case PT_Sint16:
  case PT_Uint16:
  case PT_Sint32:
  case PT_Uint32:
  case PT_Sint64:
  case PT_Uint64:
    return true;
  default:
    return false;
  }
}

constexpr bool isFixedIntegral(PrimType Type) {
  switch (Type) {
  case PT_SintFP:
  case PT_UintFP:
    return true;
  default:
    return false;
  }
}

constexpr bool isIntegral(PrimType Type) {
  return isPrimitiveIntegral(Type) || isFixedIntegral(Type);
}

constexpr bool isFixedReal(PrimType Type) {
  return Type == PT_RealFP;
}

constexpr bool isPointer(PrimType Type) {
  switch (Type) {
    case PT_Ptr:
    case PT_FnPtr:
    case PT_MemPtr:
    case PT_VoidPtr:
    case PT_ObjCBlockPtr:
      return true;
    default:
      return false;
  }
}

} // namespace interp
} // namespace clang

/// Helper macro to simplify type switches.
/// The macro implicitly exposes a type T in the scope of the inner block.
#define TYPE_SWITCH_CASE(Name, B) \
  case Name: { using T = PrimConv<Name>::T; do {B;} while(0); break; }
#define INTEGRAL_CASES(B)                                                      \
  TYPE_SWITCH_CASE(PT_Sint8, B)                                                \
  TYPE_SWITCH_CASE(PT_Uint8, B)                                                \
  TYPE_SWITCH_CASE(PT_Sint16, B)                                               \
  TYPE_SWITCH_CASE(PT_Uint16, B)                                               \
  TYPE_SWITCH_CASE(PT_Sint32, B)                                               \
  TYPE_SWITCH_CASE(PT_Uint32, B)                                               \
  TYPE_SWITCH_CASE(PT_Sint64, B)                                               \
  TYPE_SWITCH_CASE(PT_Uint64, B)                                               \
  TYPE_SWITCH_CASE(PT_Bool, B)
#define NUMERIC_CASES(B)                                                       \
  INTEGRAL_CASES(B)                                                            \
  TYPE_SWITCH_CASE(PT_SintFP, B)                                               \
  TYPE_SWITCH_CASE(PT_UintFP, B)                                               \
  TYPE_SWITCH_CASE(PT_RealFP, B)
#define TYPE_SWITCH(Expr, B)                                                   \
  switch (Expr) {                                                              \
    NUMERIC_CASES(B)                                                           \
    TYPE_SWITCH_CASE(PT_Ptr, B)                                                \
    TYPE_SWITCH_CASE(PT_FnPtr, B)                                              \
    TYPE_SWITCH_CASE(PT_MemPtr, B)                                             \
    TYPE_SWITCH_CASE(PT_VoidPtr, B)                                            \
    TYPE_SWITCH_CASE(PT_ObjCBlockPtr, B)                                       \
  }
#define COMPOSITE_TYPE_SWITCH(Expr, B, D)                                      \
  switch (Expr) {                                                              \
    TYPE_SWITCH_CASE(PT_SintFP, B)                                             \
    TYPE_SWITCH_CASE(PT_UintFP, B)                                             \
    TYPE_SWITCH_CASE(PT_RealFP, B)                                             \
    TYPE_SWITCH_CASE(PT_Ptr, B)                                                \
    TYPE_SWITCH_CASE(PT_MemPtr, B)                                             \
    TYPE_SWITCH_CASE(PT_VoidPtr, B)                                            \
    TYPE_SWITCH_CASE(PT_ObjCBlockPtr, B)                                       \
    default: do { D; } while (0); break;                                       \
  }
#define INT_TYPE_SWITCH(Expr, B)                                               \
  switch (Expr) {                                                              \
    TYPE_SWITCH_CASE(PT_Sint8, B)                                              \
    TYPE_SWITCH_CASE(PT_Uint8, B)                                              \
    TYPE_SWITCH_CASE(PT_Sint16, B)                                             \
    TYPE_SWITCH_CASE(PT_Uint16, B)                                             \
    TYPE_SWITCH_CASE(PT_Sint32, B)                                             \
    TYPE_SWITCH_CASE(PT_Uint32, B)                                             \
    TYPE_SWITCH_CASE(PT_Sint64, B)                                             \
    TYPE_SWITCH_CASE(PT_Uint64, B)                                             \
    TYPE_SWITCH_CASE(PT_SintFP, B)                                             \
    TYPE_SWITCH_CASE(PT_UintFP, B)                                             \
    default: llvm_unreachable("not an integer");                               \
  }

#endif
