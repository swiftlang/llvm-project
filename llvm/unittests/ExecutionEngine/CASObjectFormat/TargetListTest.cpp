//===- TargetListTest.cpp - Unit tests for TargetList ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/CASObjectFormat/ObjectFileSchema.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Memory.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformat;

namespace {

template <typename T>
static Error unwrapExpected(Expected<T> &&E, Optional<T> &O) {
  O.reset();
  if (!E)
    return E.takeError();
  O = std::move(*E);
  return Error::success();
}

class TargetListTest : public ::testing::Test {
protected:
  void SetUp() override {
    G.emplace("graph", Triple("x86_64-apple-darwin"), 8, support::little,
              jitlink::getGenericEdgeKindName);
    Section = &G->createSection("section", sys::Memory::MF_EXEC);
    Z = &G->createZeroFillBlock(*Section, 256, 0, 256, 0);
    ExternalS = &G->addExternalSymbol("External", 0, jitlink::Linkage::Strong);
    DefinedS =
        &G->addDefinedSymbol(*Z, 0, "Defined", 0, jitlink::Linkage::Strong,
                             jitlink::Scope::Default, false, false);

    // Prepare dependencies in the CAS.
    CAS = cas::createInMemoryCAS();
    Schema.emplace(*CAS);

    // Create ExternalS.
    ASSERT_THAT_ERROR(
        unwrapExpected(IndirectSymbolRef::create(*Schema, *ExternalS),
                       CreatedExternalS),
        Succeeded());

    // Create the block and symbol for DefinedS.
    ASSERT_THAT_ERROR(
        unwrapExpected(BlockRef::create(
                           *Schema, *Z,
                           [](const jitlink::Symbol &S, jitlink::Edge::Kind,
                              bool IsFromData, jitlink::Edge::AddendT &,
                              Optional<StringRef> &) -> Expected<TargetRef> {
                             return createStringError(inconvertibleErrorCode(),
                                                      "expected leaf block");
                           }),
                       ZeroBlock),
        Succeeded());

    ASSERT_THAT_ERROR(
        unwrapExpected(
            SymbolRef::create(
                *Schema, *DefinedS,
                [&](const jitlink::Block &B) -> Expected<SymbolDefinitionRef> {
                  assert(&B == Z);
                  return ZeroBlock->getAsSymbolDefinition();
                }),
            CreatedDefinedS),
        Succeeded());

    ASSERT_THAT_ERROR(
        unwrapExpected(IndirectSymbolRef::createAbstractBackedge(*Schema),
                       CreatedAbstractBackedgeS),
        Succeeded());
  }
  void TearDown() override {
    CreatedAbstractBackedgeS.reset();
    CreatedDefinedS.reset();
    ZeroBlock.reset();
    CreatedExternalS.reset();
    Schema.reset();
    CAS.reset();
    Section = nullptr;
    Z = nullptr;
    ExternalS = nullptr;
    DefinedS = nullptr;
    G.reset();
  }

  Optional<jitlink::LinkGraph> G;
  jitlink::Section *Section = nullptr;
  jitlink::Block *Z = nullptr;
  jitlink::Symbol *ExternalS = nullptr;
  jitlink::Symbol *DefinedS = nullptr;
  std::unique_ptr<cas::CASDB> CAS;
  Optional<ObjectFileSchema> Schema;
  Optional<IndirectSymbolRef> CreatedExternalS;
  Optional<BlockRef> ZeroBlock;
  Optional<SymbolRef> CreatedDefinedS;
  Optional<IndirectSymbolRef> CreatedAbstractBackedgeS;
};

TEST_F(TargetListTest, empty) {
  EXPECT_TRUE(TargetList().empty());
  EXPECT_EQ(0U, TargetList().size());
  EXPECT_FALSE(TargetList().hasAbstractBackedge());

#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
  EXPECT_DEATH(expectedToOptional(TargetList()[0]), "past the end");
#endif
}

TEST_F(TargetListTest, symbols) {
  TargetRef Input[] = {
      CreatedExternalS->getAsTarget(),
      CreatedDefinedS->getAsTarget(),
  };

  // Create a list.
  Optional<TargetListRef> ListRef =
      expectedToOptional(TargetListRef::create(*Schema, Input));
  ASSERT_TRUE(ListRef);
  TargetList List = ListRef->getTargets();

  // Check the content.
  ASSERT_FALSE(List.empty());
  ASSERT_EQ(std::extent<decltype(Input)>::value, List.size());
  EXPECT_FALSE(List.hasAbstractBackedge());
  for (size_t I = 0, E = List.size(); I != E; ++I) {
    Optional<TargetRef> Ref = expectedToOptional(List[I]);
    ASSERT_TRUE(Ref);
    ASSERT_EQ(Input[I].getID(), Ref->getID());
    ASSERT_EQ(Input[I].getKindString(), Ref->getKindString());
  }
}

TEST_F(TargetListTest, symbolsWithAbstractBackedge) {
  TargetRef Input[] = {
      CreatedExternalS->getAsTarget(),
      CreatedDefinedS->getAsTarget(),
      CreatedAbstractBackedgeS->getAsTarget(),
  };

  // Create a list.
  Optional<TargetListRef> ListRef =
      expectedToOptional(TargetListRef::create(*Schema, Input));
  ASSERT_TRUE(ListRef);
  EXPECT_TRUE(ListRef->hasAbstractBackedge());
  TargetList List = ListRef->getTargets();

  // Check the content.
  ASSERT_FALSE(List.empty());
  ASSERT_EQ(std::extent<decltype(Input)>::value, List.size());
  EXPECT_TRUE(List.hasAbstractBackedge());
  for (size_t I = 0, E = List.size(); I != E; ++I) {
    Optional<TargetRef> Ref = expectedToOptional(List[I]);
    ASSERT_TRUE(Ref);
    ASSERT_EQ(Input[I].getID(), Ref->getID());
    ASSERT_EQ(Input[I].getKindString(), Ref->getKindString());
  }
}

} // end namespace
