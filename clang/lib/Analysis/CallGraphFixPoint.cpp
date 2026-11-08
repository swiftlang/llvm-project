//===- CallGraphFixPoint.cpp - Fix-point over the call graph ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/CallGraphFixPoint.h"
#include "clang/Analysis/CallGraph.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <deque>

using namespace clang;

// The most times one run applies F to a function.
#ifndef CALLGRAPH_FIXPOINT_MAX_ANALYSES
#define CALLGRAPH_FIXPOINT_MAX_ANALYSES 10
#endif

namespace {

/// A FIFO queue that ignores an element already in it.
template <typename T> class SetQueue {
  std::deque<T> Queue;
  llvm::DenseSet<T> Members;

public:
  explicit SetQueue(llvm::ArrayRef<T> Init) {
    for (const T &V : Init)
      push_back(V);
  }
  void push_back(const T &V) {
    if (Members.insert(V).second)
      Queue.push_back(V);
  }
  bool empty() const { return Queue.empty(); }
  T drop_front() {
    T V = Queue.front();
    Queue.pop_front();
    Members.erase(V);
    return V;
  }
};

} // namespace

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

CallGraphFixPoint::CallGraphFixPoint(TranslationUnitDecl *TU) {
  buildGraph(TU, Order, Callers);
}

void CallGraphFixPoint::run(llvm::function_ref<bool(const FunctionDecl *)> F) {
  SetQueue<const FunctionDecl *> Work(Order);
  llvm::DenseMap<const FunctionDecl *, unsigned> Counts;

  Counts.reserve(Order.size());
  while (!Work.empty()) {
    const FunctionDecl *FD = Work.drop_front();

    if (++Counts[FD] > CALLGRAPH_FIXPOINT_MAX_ANALYSES)
      continue;
    if (F(FD))
      for (const FunctionDecl *Caller : Callers.lookup(FD))
        Work.push_back(Caller);
  }
}
