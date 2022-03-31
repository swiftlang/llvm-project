//===- CAS.h - CAS object file format ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines manifest constants for the CAS object file format.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BINARYFORMAT_CAS_H
#define LLVM_BINARYFORMAT_CAS_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Optional.h"
#include <vector>

namespace llvm {
namespace casobject {

// Object file magic string.
constexpr const char *CASObjectMagic = "CASID:";

enum class CASLinakgeType : uint8_t {
  CAS_LINKAGE_STRONG = 0,
  CAS_LINKAGE_WEAK = 1
};

enum class CASScopeType : uint8_t {
  CAS_SCOPE_DEFAULT = 0,
  CAS_SCOPE_HIDDEN = 1,
  CAS_SCOPE_LOCAL = 2
};

enum class CASMemProtType: uint8_t {
  CAS_MEMPROT_NONE = 0,
  CAS_MEMPROT_READ = 1U << 0,
  CAS_MEMPROT_WRITE = 1U << 1,
  CAS_MEMPROT_EXEC = 1U << 2
};

enum class CASSymbolKind: uint8_t {
  CAS_SYMBOL_STATIC = 0,
  CAS_SYMBOL_UNDEF = 1,
  CAS_SYMBOL_DYNAMIC = 2,
  CAS_SYMBOL_FUTURE = 3
};

enum class CASSymbolAttr {
  CAS_SYM_ATTR_NONE = 0,
  CAS_SYM_ATTR_STATIC = 1
};

struct CASSection {
  StringRef Name;
  uint8_t Prot;
};

struct CASContent {
  uint64_t Size;
  uint64_t Alignment;
  uint64_t AlignmentOffset;
  CASMemProtType MemProt;
  StringRef Content;
};

struct CASSymbol {
  CASSymbolKind Kind;
  StringRef Name;
  CASScopeType Scope;
  CASLinakgeType Linkage;
};

} // namespace casobject
} // namespace llvm

#endif
