//===--- InterpLoop.h - Interpreter for the constexpr VM --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Definition of the interpreter state and entry point.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_INTERPLOOP_H
#define LLVM_CLANG_AST_INTERP_INTERPLOOP_H

#include "InterpState.h"

namespace clang {
namespace interp {

/// Interpreter entry point.
bool Interpret(InterpState &S, APValue &Result);

} // namespace interp
} // namespace clang

#endif
