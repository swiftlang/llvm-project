//===- AtomicSharedPtr.h - Atomic access for std::shared_ptr ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_ATOMICSHAREDPTR_H
#define LLVM_ADT_ATOMICSHAREDPTR_H

#include <atomic>
#include <memory>

namespace llvm {

/// Wrap a shared pointer to guarantee atomic access.
template <class T> class AtomicSharedPtr {
public:
  operator std::shared_ptr<T>() const { return load(); }
  std::shared_ptr<T> load() const { return std::atomic_load(&P); }
  void store(std::shared_ptr<T> RHS) { std::atomic_store(&P, std::move(RHS)); }
  bool compare_exchange_strong(std::shared_ptr<T> &Expected,
                               std::shared_ptr<T> Desired) {
    return std::atomic_compare_exchange_strong(&P, &Expected,
                                               std::move(Desired));
  }

  std::shared_ptr<T> take() {
    return std::atomic_exchange(&P, std::shared_ptr<T>());
  }

  AtomicSharedPtr() = default;
  AtomicSharedPtr(std::shared_ptr<T> P) : P(std::move(P)) {}
  AtomicSharedPtr(AtomicSharedPtr &&RHS) : P(RHS.take()) {}
  AtomicSharedPtr(const AtomicSharedPtr &RHS) : P(RHS.load()) {}
  AtomicSharedPtr &operator=(AtomicSharedPtr &&RHS) {
    store(RHS.take());
    return *this;
  }
  AtomicSharedPtr &operator=(const AtomicSharedPtr &RHS) {
    store(RHS.load());
    return *this;
  }
  AtomicSharedPtr &operator=(std::shared_ptr<T> RHS) {
    store(std::move(RHS));
    return *this;
  }

private:
  std::shared_ptr<T> P;
};

} // end namespace llvm

#endif // LLVM_ADT_ATOMICSHAREDPTR_H
