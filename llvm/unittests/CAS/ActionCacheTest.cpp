//===- ActionCacheTest.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/ActionCache.h"
#include "TestingNamespaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Testing/Support/Error.h"
#include "llvm/Testing/Support/SupportHelpers.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

struct ActionCacheParam {
  std::unique_ptr<ActionCache> Cache;
  Optional<unittest::TempDir> D;
};

class ActionCacheTest
    : public ::testing::TestWithParam<
          std::function<ActionCacheParam(const Namespace &)>> {
protected:
  SmallVector<unittest::TempDir> Ds;
  std::unique_ptr<ActionCache> createActionCache(const Namespace &NS) {
    auto P = GetParam()(NS);
    if (P.D)
      Ds.push_back(std::move(*P.D));
    return std::move(P.Cache);
  }
  void TearDown() { Ds.clear(); }
};

static uint64_t Values[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
struct VectorOfIDs {
  SmallVector<UniqueIDRef> V;
  VectorOfIDs() {
    constexpr size_t NumHashBytes = sizeof(uint64_t);
    const Namespace &NS = cas::testing::getNamespace(NumHashBytes);
    for (uint64_t &Value : Values)
      V.emplace_back(NS, makeArrayRef(reinterpret_cast<const uint8_t *>(&Value),
                                      NumHashBytes));
  }
};

template <class T> static std::string toString(const T &V) {
  SmallString<128> Printed;
  raw_svector_ostream(Printed) << V;
  return Printed.str().str();
}

TEST_P(ActionCacheTest, getEmpty) {
  const Namespace &NS = cas::testing::getNamespace(sizeof(uint64_t));
  std::unique_ptr<ActionCache> Cache = createActionCache(NS);
  {
    UniqueID ID;
    ASSERT_THAT_ERROR(Cache->get(ActionDescription("action1"), ID),
                      Succeeded());
    EXPECT_FALSE(ID);
  }
  {
    Optional<UniqueIDRef> ID;
    ASSERT_THAT_ERROR(Cache->get(ActionDescription("action1"), ID),
                      Succeeded());
    EXPECT_FALSE(ID);
  }
}

TEST_P(ActionCacheTest, put) {
  const Namespace &NS = cas::testing::getNamespace(sizeof(uint64_t));
  std::unique_ptr<ActionCache> Cache = createActionCache(NS);
  VectorOfIDs IDs;

  SmallVector<ActionDescription> Actions;
  Actions.push_back(ActionDescription("action"));
  Actions.push_back(ActionDescription("action1"));
  Actions.push_back(ActionDescription("action", None, "extra"));
  Actions.push_back(ActionDescription("action", IDs.V[0]));
  Actions.push_back(ActionDescription("action", IDs.V[0], "extra"));
  Actions.push_back(ActionDescription("action", IDs.V[1]));
  Actions.push_back(
      ActionDescription("action", makeArrayRef(IDs.V).take_front(2)));
  Actions.push_back(
      ActionDescription("action", makeArrayRef(IDs.V).take_front(3)));

  SmallVector<UniqueIDRef> Results;
  for (size_t I = 0, E = Actions.size(); I != E; ++I)
    Results.push_back(makeArrayRef(IDs.V).drop_back(I).back());

  Optional<UniqueIDRef> ResultID;
  for (size_t I = 0, E = Actions.size(); I != E; ++I) {
    Optional<UniqueIDRef> PrevResultID = ResultID;

    ASSERT_THAT_ERROR(Cache->get(Actions[I], ResultID), Succeeded());
    EXPECT_FALSE(ResultID);
    ASSERT_THAT_ERROR(Cache->put(Actions[I], Results[I]), Succeeded());
    ASSERT_THAT_ERROR(Cache->get(Actions[I], ResultID), Succeeded());
    EXPECT_EQ(Results[I], ResultID);

    if (!PrevResultID)
      continue;

    // Attempt a collision.
    ASSERT_THAT_ERROR(Cache->put(Actions[I], *PrevResultID), Failed());
    EXPECT_THAT_ERROR(Cache->get(Actions[I], ResultID), Succeeded());
    EXPECT_EQ(Results[I], ResultID);
  }
}

INSTANTIATE_TEST_SUITE_P(
    InMemoryActionCache, ActionCacheTest,
    ::testing::Values([](const Namespace &NS) {
      return ActionCacheParam{createInMemoryActionCache(NS), None};
    }));
INSTANTIATE_TEST_SUITE_P(OnDiskActionCache, ActionCacheTest,
                         ::testing::Values([](const Namespace &NS) {
                           ActionCacheParam P;
                           P.D.emplace("on-disk-cache", /*Unique=*/true);
                           P.Cache = cantFail(
                               createOnDiskActionCache(NS, P.D->path()));
                           return P;
                         }));

} // end anonymous namespace
