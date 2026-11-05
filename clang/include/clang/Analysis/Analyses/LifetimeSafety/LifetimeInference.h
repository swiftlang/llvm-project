//===- LifetimeInference.h - Inferring lifetimes ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The lifetime attributes/properties inference module.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMEINFERENCE_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMEINFERENCE_H

#include "clang/AST/Decl.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
class Attr;
class Declarator;
} // namespace clang

namespace clang::lifetimes {

/// The attributes written in the source, kept by the test mode instead of
/// being attached, for the parameters and for `this`.
using ParamOracleMap =
    llvm::DenseMap<const ParmVarDecl *, llvm::SmallVector<const Attr *, 2>>;
using ThisOracleMap =
    llvm::DenseMap<const FunctionDecl *, llvm::SmallVector<const Attr *, 2>>;

class LifetimeInference {
public:
  /// Test mode: keeps the attribute \p A written on the parameter \p PVD as an
  /// oracle, instead of keeping it in AST.
  void addParamOracle(const ParmVarDecl *PVD, const Attr *A);

  /// Test mode: keeps the attributes written on the implicit object parameter
  /// of \p FD as oracles, instead of keeping it in AST.
  void addThisOracles(const FunctionDecl *FD,
                      llvm::SmallVector<const Attr *, 2> Attrs);

  /// Test mode: the lifetime attributes of the implicit object parameter are
  /// dropped while the declarator's type is built, before any FunctionDecl
  /// exists. They wait here, with their declarator, until the declared
  /// function is known and they are saved as its oracles.
  struct PendingThisOracleSlot {
    const Declarator *D = nullptr;
    llvm::SmallVector<const Attr *, 2> Attrs;

    void add(const Declarator *Dcl, const Attr *A) {
      // A stale slot of another declarator, e.g. a lambda's, is dropped.
      if (D != Dcl)
        reset();
      D = Dcl;
      Attrs.push_back(A);
    }
    void reset() {
      D = nullptr;
      Attrs.clear();
    }
  } PendingThisOracle;

private:
  ParamOracleMap ParamOracles;
  ThisOracleMap ThisOracles;
};

} // namespace clang::lifetimes

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMEINFERENCE_H
