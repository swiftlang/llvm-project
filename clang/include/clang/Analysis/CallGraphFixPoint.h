//===- CallGraphFixPoint.h - Fix-point over the call graph ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file declares FixPoint, which applies a function to elements, and
//  again to the dependents of each element whose result changed, until nothing
//  changes. CallGraphFixPoint is FixPoint over the functions defined in a
//  translation unit, callees first.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_CALLGRAPHFIXPOINT_H
#define LLVM_CLANG_ANALYSIS_CALLGRAPHFIXPOINT_H

#include "clang/AST/Decl.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallVector.h"
#include <deque>

// The most times one run applies F to an element.
#ifndef CALLGRAPH_FIXPOINT_MAX_ANALYSES
#define CALLGRAPH_FIXPOINT_MAX_ANALYSES 10
#endif

namespace clang {

/// A FIFO queue that ignores an element already in it.
template <typename T> class FixPointSetQueue {
  std::deque<T> Queue;
  llvm::DenseSet<T> Members;

public:
  explicit FixPointSetQueue(llvm::ArrayRef<T> Init) {
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

template <typename T> class FixPoint {
public:
  /// \p Elements are the elements to apply F to; their order only breaks ties.
  /// \p Dependents maps an element to the elements, all in \p Elements, to
  /// re-apply F to when its result changes. F is first applied to an element
  /// before its dependents, cycles aside.
  FixPoint(llvm::ArrayRef<T> Elements,
           llvm::DenseMap<T, llvm::SmallVector<T, 2>> Dependents)
      : Dependents(std::move(Dependents)) {
    sortDependenciesFirst(Elements);
  }

  /// Applies \p F to the elements in order. A true return means F changed the
  /// element's result, so its dependents are queued again. F is applied to an
  /// element at most CALLGRAPH_FIXPOINT_MAX_ANALYSES times per run, so the run
  /// ends even if F never converges. Can be called repeatedly.
  void run(llvm::function_ref<bool(T)> F) {
    FixPointSetQueue<T> Work(Order);
    llvm::DenseMap<T, unsigned> Counts;

    Counts.reserve(Order.size());
    while (!Work.empty()) {
      T V = Work.drop_front();

      if (++Counts[V] > CALLGRAPH_FIXPOINT_MAX_ANALYSES)
        continue;
      if (F(V))
        for (const T &D : Dependents.lookup(V))
          Work.push_back(D);
    }
  }

  /// The elements, dependencies first.
  llvm::ArrayRef<T> getOrder() const { return Order; }

private:
  // Sets Order to the reverse post-order of a DFS over the dependents edges.
  void sortDependenciesFirst(llvm::ArrayRef<T> Elements) {
    llvm::DenseSet<T> Visited;
    llvm::SmallVector<T> Post;
    // The DFS stack: an element and the index of its next dependent.
    llvm::SmallVector<std::pair<T, size_t>> Stack;

    Post.reserve(Elements.size());
    Stack.reserve(Elements.size());
    Visited.reserve(Elements.size());
    for (const T &Root : Elements) {
      if (!Visited.insert(Root).second)
        continue;
      Stack.push_back({Root, 0});
      while (!Stack.empty()) {
        auto &[V, Next] = Stack.back();
        auto It = Dependents.find(V);

        if (It != Dependents.end() && Next < It->second.size()) {
          T D = It->second[Next++];
          if (Visited.insert(D).second)
            Stack.push_back({D, 0});
          continue;
        }
        Post.push_back(V);
        Stack.pop_back();
      }
    }
    Order.assign(Post.rbegin(), Post.rend());
  }

  llvm::DenseMap<T, llvm::SmallVector<T, 2>> Dependents;
  /// Dependencies first. Read only after construction.
  llvm::SmallVector<T> Order;
};

class CallGraphFixPoint {
public:
  explicit CallGraphFixPoint(TranslationUnitDecl *TU);

  /// Applies \p F to the defined functions of the TU, callees first. A true
  /// return means F changed the function's result, so its callers are queued
  /// again. See FixPoint::run.
  void run(llvm::function_ref<bool(const FunctionDecl *)> F) { FP.run(F); }

private:
  FixPoint<const FunctionDecl *> FP;
};

} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_CALLGRAPHFIXPOINT_H
