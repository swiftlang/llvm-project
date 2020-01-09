//===--- Source.h - Source location provider for the VM  --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines a program which organises and links multiple bytecode functions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_SOURCE_H
#define LLVM_CLANG_AST_INTERP_SOURCE_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "llvm/Support/Endian.h"

namespace clang {
namespace interp {
class Function;
class GlobalLocation;

/// Pointer into the code segment.
class CodePtr {
public:
  CodePtr() : Ptr(nullptr) {}

  CodePtr &operator+=(int32_t Offset) {
    Ptr += Offset;
    return *this;
  }

  int32_t operator-(const CodePtr &RHS) const {
    assert(Ptr != nullptr && RHS.Ptr != nullptr && "Invalid code pointer");
    return Ptr - RHS.Ptr;
  }

  CodePtr operator-(size_t RHS) const {
    assert(Ptr != nullptr && "Invalid code pointer");
    return CodePtr(Ptr - RHS);
  }

  bool operator!=(const CodePtr &RHS) const { return Ptr != RHS.Ptr; }

  /// Reads data and advances the pointer.
  template <typename T> T read() {
    T Result;
    memcpy(&Result, Ptr, sizeof(T));
    Ptr += sizeof(T);
    return Result;
  }

private:
  /// Constructor used by Function to generate pointers.
  CodePtr(const char *Ptr) : Ptr(Ptr) {}

private:
  friend class Function;

  /// Pointer into the code owned by a function.
  const char *Ptr;
};

/// Describes the statement/declaration an opcode was generated from.
class SourceInfo {
public:
  SourceInfo() {}
  SourceInfo(const Stmt *E) : Source(E) {}
  SourceInfo(const Decl *D) : Source(D) {}

  SourceLocation getLoc() const;

  const Stmt *asStmt() const { return Source.dyn_cast<const Stmt *>(); }
  const Decl *asDecl() const { return Source.dyn_cast<const Decl *>(); }
  const Expr *asExpr() const;

  operator bool() const { return !Source.isNull(); }

private:
  llvm::PointerUnion<const Decl *, const Stmt *> Source;
};

using SourceMap = std::vector<std::pair<unsigned, SourceInfo>>;

/// Interface for classes which map locations to sources.
class SourceMapper {
public:
  virtual ~SourceMapper() {}

  /// Returns source information for a given PC in a function.
  virtual SourceInfo getSource(Function *F, CodePtr PC) const = 0;
};

} // namespace interp
} // namespace clang

#endif
