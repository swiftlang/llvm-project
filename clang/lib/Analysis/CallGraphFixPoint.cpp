//===- CallGraphFixPoint.cpp - Fix-point over the call graph ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/CallGraphFixPoint.h"
#include "clang/Analysis/CallGraph.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace clang;

// Fills \p Order with the defined functions of \p TU, callees first, and
// \p Callers with the callers of each.
static void buildGraph(
    TranslationUnitDecl *TU, llvm::SmallVector<const FunctionDecl *> &Order,
    llvm::DenseMap<const FunctionDecl *,
                   llvm::SmallVector<const FunctionDecl *, 2>> &Callers) {
  CallGraph CG;

  CG.addToCallGraph(TU);
  for (CallGraphNode *Node : llvm::post_order(&CG)) {
    if (!Node->getDecl()) // the virtual root
      continue;

    const FunctionDecl *Caller = Node->getDefinition();

    if (!Caller)
      continue;
    Order.push_back(Caller);

    // A caller has one edge per call site.
    llvm::SmallPtrSet<const FunctionDecl *, 8> Seen;

    for (CallGraphNode *Callee : Node->callees())
      if (const FunctionDecl *FD = Callee->getDefinition();
          FD && Seen.insert(FD).second)
        Callers[FD].push_back(Caller);
  }
}

static FixPoint<const FunctionDecl *> build(TranslationUnitDecl *TU) {
  llvm::SmallVector<const FunctionDecl *> Order;
  llvm::DenseMap<const FunctionDecl *,
                 llvm::SmallVector<const FunctionDecl *, 2>>
      Callers;

  buildGraph(TU, Order, Callers);
  return FixPoint<const FunctionDecl *>(Order, std::move(Callers));
}

CallGraphFixPoint::CallGraphFixPoint(TranslationUnitDecl *TU)
    : FP(build(TU)) {}
