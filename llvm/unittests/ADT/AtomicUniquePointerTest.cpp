//===- llvm/unittests/ADT/AtomicUniquePointerTest.cpp ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/AtomicUniquePointer.h"
#include "llvm/Support/ThreadPool.h"
#include "gtest/gtest.h"
#include <memory>

using namespace llvm;

namespace {

TEST(AtomicUniquePointerTest, Create) {
  AtomicUniquePointer<int> ImplPtr;
  SmallVector<int*, 4> Results(4);

  auto createImpl = [&ImplPtr, &Results](unsigned Idx) {
    auto Impl = std::make_unique<int>();
    int *ExistingImpl = nullptr;
    if (ImplPtr.compare_exchange_strong(ExistingImpl, Impl)) {
      EXPECT_EQ(ExistingImpl, nullptr);
      EXPECT_EQ(Impl.get(), nullptr);
    } else {
      EXPECT_NE(ExistingImpl, nullptr);
      EXPECT_NE(Impl.get(), nullptr);
    }
    Results[Idx] = ImplPtr.load();
  };

  ThreadPool Threads;
  for (unsigned I = 0; I < 4; ++I)
    Threads.async(createImpl, I);
  Threads.wait();

  EXPECT_NE(ImplPtr.load(), nullptr);
 
  // All the threads should get the same results.
  SmallVector<int*, 4> ExpectedPtrs(4, ImplPtr.load());
  EXPECT_EQ(Results, ExpectedPtrs);
}


TEST(AtomicUniquePointerTest, LinkList) {
  // Test atomic insertion. Insert node S as Root -> S -> Root.Next.
  struct DummyAtomicList {
    AtomicUniquePointer<DummyAtomicList> Next;
  };
  DummyAtomicList Root;
  auto insertNode = [&Root]() {
    auto S = std::make_unique<DummyAtomicList>();
    DummyAtomicList *CurrentHead = nullptr;
    while (!Root.Next.compare_exchange_weak(CurrentHead, S))
      S->Next.exchange(std::unique_ptr<DummyAtomicList>(CurrentHead)).release();

    EXPECT_EQ(CurrentHead, S.get());
    S.release();
  };

  ThreadPool Threads;
  for (unsigned I = 0; I < 100; ++I)
    Threads.async(insertNode);

  Threads.wait();

  // Compute the size of the linked list and verify.
  unsigned Size = 0;
  DummyAtomicList *Node = &Root;
  while (Node->Next.load()) {
    ++Size;
    Node = Node->Next.load();
  }

  EXPECT_EQ(Size, 100U);
}

} // namespace
