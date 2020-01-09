//===--- HeapUtils.h - Utilities to handle the heap -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_INTERPHELPER_H
#define LLVM_CLANG_AST_INTERP_INTERPHELPER_H

#include "MemberPointer.h"
#include "Pointer.h"
#include "Source.h"

namespace clang {
namespace interp {

/// Converts a composite value to an APValue.
bool PointerToValue(const ASTContext &Ctx, const Pointer &Ptr, APValue &Result);

/// Copies a union.
bool CopyUnion(InterpState &S, CodePtr OpPC, const BlockPointer &From,
               const BlockPointer &To);

/// Copies a block to another.
bool Copy(InterpState &S, CodePtr OpPC, const BlockPointer &From,
          const BlockPointer &To);

/// Finds the record to which a pointer points and ajudsts this to it.
Record *PointerLookup(Pointer &This, const MemberPointer &Field);

/// Finds a virtual override and adjusts This to point to the object.
const CXXMethodDecl *VirtualLookup(BlockPointer &This, const CXXMethodDecl *MD);

/// Indirect method lookup.
const CXXMethodDecl *IndirectLookup(BlockPointer &This, const MemberPointer &Ptr);

} // namespace interp
} // namespace clang

#endif
