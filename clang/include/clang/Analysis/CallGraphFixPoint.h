//===- CallGraphFixPoint.h - Fix-point over the call graph ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file declares CallGraphFixPoint, which applies a function to the
//  functions defined in a translation unit, callees first, and applies it again
//  to the callers of each function whose result changed, until nothing changes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_CALLGRAPHFIXPOINT_H
#define LLVM_CLANG_ANALYSIS_CALLGRAPHFIXPOINT_H

#include "clang/AST/Decl.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {

class CallGraphFixPoint {
public:
  explicit CallGraphFixPoint(TranslationUnitDecl *TU);

  /// Applies \p F to the defined functions of the TU, callees first. A true
  /// return means F changed the function's result, so its callers are queued
  /// again. F is applied to a function at most
  /// CALLGRAPH_FIXPOINT_MAX_ANALYSES times per run, so the run ends even if F
  /// never converges. Can be called repeatedly.
  void run(llvm::function_ref<bool(const FunctionDecl *)> F);

private:
  llvm::DenseMap<const FunctionDecl *,
                 llvm::SmallVector<const FunctionDecl *, 2>>
      Callers;
  /// The defined functions, callees first. Read only after construction.
  llvm::SmallVector<const FunctionDecl *> Order;
};

} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_CALLGRAPHFIXPOINT_H
