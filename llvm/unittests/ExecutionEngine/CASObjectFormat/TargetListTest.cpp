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
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformat;

namespace {

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
    CreatedExternalS =
        expectedToOptional(IndirectSymbolRef::create(*Schema, *ExternalS));
    ASSERT_TRUE(CreatedExternalS);

    // Create the block and symbol for DefinedS.
    ZeroBlock = expectedToOptional(
        BlockRef::create(*Schema, *Z,
                         [](const jitlink::Symbol &S, jitlink::Edge::Kind,
                            bool IsFromData, jitlink::Edge::AddendT &,
                            Optional<StringRef> &) -> Expected<TargetRef> {
                           return createStringError(inconvertibleErrorCode(),
                                                    "expected leaf block");
                         }));
    ASSERT_TRUE(ZeroBlock);
    CreatedDefinedS = expectedToOptional(SymbolRef::create(
        *Schema, *DefinedS,
        [&](const jitlink::Block &B) -> Expected<SymbolDefinitionRef> {
          assert(&B == Z);
          return ZeroBlock->getAsSymbolDefinition();
        }));
    ASSERT_TRUE(CreatedDefinedS);

    CreatedAbstractBackedgeS =
        expectedToOptional(IndirectSymbolRef::createAbstractBackedge(*Schema));
    ASSERT_TRUE(CreatedAbstractBackedgeS);
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
