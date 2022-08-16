//===- LazyAtomicPointerTest.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/LazyAtomicPointer.h"
#include "llvm/Support/ThreadPool.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(LazyAtomicPointer, loadOrGenerate) {
  int Value = 0;
  LazyAtomicPointer<int> Ptr;
  ThreadPool Threads;
  for (unsigned I = 0; I < 4; ++I)
    Threads.async([&]() {
      Ptr.loadOrGenerate([&]() {
        // Make sure this is only called once.
        static std::atomic<bool> Once(false);
        bool Current = false;
        EXPECT_TRUE(Once.compare_exchange_strong(Current, true));
        return &Value;
      });
    });

  Threads.wait();

  EXPECT_EQ(Ptr.load(), &Value);
}

TEST(LazyAtomicPointer, BusyState) {
  int Value = 0;
  LazyAtomicPointer<int> Ptr;
  ThreadPool Threads;

  std::mutex BusyStart, BusyEnd;
  BusyStart.lock();
  BusyEnd.lock();
  Threads.async([&]() {
    Ptr.loadOrGenerate([&]() {
      BusyStart.unlock();
      while (!BusyEnd.try_lock()) {
        // wait till the lock is unlocked.
      }
      return &Value;
    });
  });

  // Wait for busy state.
  std::lock_guard<std::mutex> BusyLockG(BusyStart);
  int *ExistingValue = nullptr;
  // Busy state will not exchange the value.
  EXPECT_FALSE(Ptr.compare_exchange_weak(ExistingValue, nullptr));
  // Busy state return nullptr on load/compare_exchange_weak.
  EXPECT_EQ(ExistingValue, nullptr);
  EXPECT_EQ(Ptr.load(), nullptr);

  // End busy state.
  BusyEnd.unlock();
  Threads.wait();
  EXPECT_EQ(Ptr.load(), &Value);
}

} // namespace
