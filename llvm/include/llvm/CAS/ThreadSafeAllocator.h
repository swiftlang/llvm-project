//===- ThreadSafeAllocator.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_THREADSAFEALLOCATOR_H
#define LLVM_CAS_THREADSAFEALLOCATOR_H

#include "llvm/Support/Allocator.h"
#include <mutex>

namespace llvm {
namespace cas {

template <class AllocatorType> class ThreadSafeAllocator {
public:
  auto Allocate(size_t N = 1) {
    std::lock_guard<std::mutex> Lock(Mutex);
    return Alloc.Allocate(N);
  }

private:
  AllocatorType Alloc;
  std::mutex Mutex;
};

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_THREADSAFEALLOCATOR_H
