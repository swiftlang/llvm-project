//===--- Builtin.h - Builtins for the constexpr VM --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Builtin dispatch method definition.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_BUILTIN_H
#define LLVM_CLANG_AST_INTERP_BUILTIN_H

#include "Function.h"

namespace clang {
namespace interp {
class InterpState;

bool InterpBuiltin(InterpState &S, CodePtr OpPC, unsigned BuiltinOp);

} // namespace interp
} // namespace clang

#endif
