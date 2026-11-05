//===- LifetimeInference.cpp - Inferring lifetimes -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/LifetimeInference.h"
#include "clang/AST/Attr.h"

namespace clang::lifetimes {

void LifetimeInference::addParamOracle(const ParmVarDecl *PVD, const Attr *A) {
  ParamOracles[PVD].push_back(A);
}

void LifetimeInference::addThisOracles(
    const FunctionDecl *FD, llvm::SmallVector<const Attr *, 2> Attrs) {
  ThisOracles[FD] = std::move(Attrs);
}

} // namespace clang::lifetimes
