//===- ObjectFileSchemaTest.cpp - Unit tests CAS object file schema -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ExecutionEngine/CASObjectFormat/ObjectFileSchema.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Memory.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformat;

namespace {

TEST(CASObjectFormatTests, Section) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8, support::little,
                       jitlink::getGenericEdgeKindName);
  jitlink::Section *Sections[]{
      &G.createSection("read", sys::Memory::MF_READ),
      &G.createSection("write", sys::Memory::MF_WRITE),
      &G.createSection("exec", sys::Memory::MF_EXEC),
      &G.createSection("all", sys::Memory::MF_RWE_MASK),
      &G.createSection("HUGE",
                       sys::Memory::ProtectionFlags(sys::Memory::MF_READ |
                                                    sys::Memory::MF_HUGE_HINT)),
  };

  std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
  ObjectFileSchema Schema(*CAS);
  for (jitlink::Section *S : Sections) {
    Optional<SectionRef> Ref = expectedToOptional(SectionRef::create(Schema, *S));
    ASSERT_TRUE(Ref);

    // Note: MF_HUGE_HINT is expected to be dropped because it's not in the RWE
    // mask.
    EXPECT_EQ(S->getProtectionFlags() & sys::Memory::MF_RWE_MASK,
              Ref->getProtectionFlags());
    Optional<cas::BlobRef> Name = expectedToOptional(Ref->getName());
    ASSERT_TRUE(Name);
    EXPECT_EQ(S->getName(), Name->getData());
  }
}

static const char BlockContentBytes[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                        0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
                                        0x1C, 0x1D, 0x1E, 0x1F, 0x00};
ArrayRef<char> BlockContent(BlockContentBytes);

TEST(CASObjectFormatTests, BlockData) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8, support::little,
                       jitlink::getGenericEdgeKindName);
  jitlink::Section &Section = G.createSection("section", sys::Memory::MF_EXEC);
  jitlink::Block *Blocks[] = {
      &G.createZeroFillBlock(Section, 0, 0, /*Alignment=*/1, 0),
      &G.createZeroFillBlock(Section, 8, 64, /*Alignment=*/1, 0),
      &G.createZeroFillBlock(Section, 8, 64, /*Alignment=*/16, 0),
      &G.createZeroFillBlock(Section, 8, 64, /*Alignment=*/16, 10),
      &G.createZeroFillBlock(Section, 8, 64, /*Alignment=*/16, 6),
      &G.createContentBlock(Section, BlockContent, 0, /*Alignment=*/1, 0),
      &G.createContentBlock(Section, BlockContent, 64, /*Alignment=*/16, 0),
      &G.createContentBlock(Section, BlockContent, 64, /*Alignment=*/16, 10),
      &G.createContentBlock(Section, BlockContent, 64, /*Alignment=*/16, 6),
      &G.createContentBlock(Section, "other data", 64, /*Alignment=*/16, 6),
  };

  std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
  ObjectFileSchema Schema(*CAS);
  for (jitlink::Block *B : Blocks) {
    Optional<BlockDataRef> Ref = expectedToOptional(BlockDataRef::create(
        Schema, *B));
    ASSERT_TRUE(Ref);
    EXPECT_EQ(B->getSize(), Ref->getSize());
    EXPECT_EQ(B->getAlignment(), Ref->getAlignment());
    EXPECT_EQ(B->getAlignmentOffset(), Ref->getAlignmentOffset());
    EXPECT_EQ(B->isZeroFill(), Ref->isZeroFill());

    Expected<Optional<cas::BlobRef>> Content = Ref->getContent();
    EXPECT_TRUE(bool(Content));
    if (!Content) {
      consumeError(Content.takeError());
      continue;
    }
    if (B->isZeroFill()) {
      EXPECT_EQ(None, *Content);
      continue;
    }
    EXPECT_EQ(StringRef(B->getContent().begin(), B->getContent().size()),
              (*Content)->getData());
  }
}

TEST(CASObjectFormatTests, LeafBlock) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8,
                       support::little, jitlink::getGenericEdgeKindName);
  jitlink::Section &Section1 =
      G.createSection("section1", sys::Memory::MF_EXEC);
  jitlink::Section &Section2 =
      G.createSection("section2", sys::Memory::MF_EXEC);
  jitlink::Block *Blocks[] = {
      &G.createZeroFillBlock(Section1, 0, 0, /*Alignment=*/1, 0),
      &G.createZeroFillBlock(Section1, 8, 64, /*Alignment=*/1, 0),
      &G.createZeroFillBlock(Section1, 8, 64, /*Alignment=*/16, 0),
      &G.createZeroFillBlock(Section1, 8, 64, /*Alignment=*/16, 10),
      &G.createZeroFillBlock(Section2, 8, 64, /*Alignment=*/16, 6),
      &G.createContentBlock(Section2, BlockContent, 0, /*Alignment=*/1, 0),
      &G.createContentBlock(Section2, BlockContent, 64, /*Alignment=*/16, 0),
      &G.createContentBlock(Section2, BlockContent, 64, /*Alignment=*/16, 10),
      &G.createContentBlock(Section1, BlockContent, 64, /*Alignment=*/16, 6),
      &G.createContentBlock(Section1, "other data", 64, /*Alignment=*/16, 6),
  };

  std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
  ObjectFileSchema Schema(*CAS);
  for (jitlink::Block *B : Blocks) {
    // This is the API being tested.
    Optional<BlockRef> Block = expectedToOptional(BlockRef::create(
        Schema, *B,
        [](const jitlink::Symbol &, jitlink::Edge::Kind, bool IsFromData,
           jitlink::Edge::AddendT &,
           Optional<StringRef> &) -> Expected<TargetRef> {
          return createStringError(inconvertibleErrorCode(),
                                   "expected a leaf block");
        }));
    ASSERT_TRUE(Block);

    // Independently look up block data and section.
    Optional<BlockDataRef> BlockData =
        expectedToOptional(BlockDataRef::create(Schema, *B));
    Optional<SectionRef> Section =
        expectedToOptional(SectionRef::create(Schema, B->getSection()));
    ASSERT_TRUE(BlockData);
    ASSERT_TRUE(Section);

    // Check that the block is correct.
    EXPECT_EQ(Block->getDataID(), *BlockData);
    EXPECT_EQ(Block->getSectionID(), *Section);
    EXPECT_FALSE(Block->hasEdges());
    EXPECT_FALSE(Block->getFixupsID());
    EXPECT_FALSE(Block->getTargetInfoID());
    EXPECT_FALSE(Block->getTargetsID());
  }
}

TEST(CASObjectFormatTests, SymbolFlags) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8,
                       support::little, jitlink::getGenericEdgeKindName);
  jitlink::Section &Section = G.createSection("section", sys::Memory::MF_EXEC);
  jitlink::Block &Block = G.createContentBlock(Section, BlockContent, 0, 16, 0);
  jitlink::Symbol &StrongDefaultDead =
      G.addDefinedSymbol(Block, 0, "symbol1", 0, jitlink::Linkage::Strong,
                         jitlink::Scope::Default, false, false);
  jitlink::Symbol &StrongDefaultLive =
      G.addDefinedSymbol(Block, 0, "symbol2", 0, jitlink::Linkage::Strong,
                         jitlink::Scope::Default, false, true);
  jitlink::Symbol &StrongHiddenDead =
      G.addDefinedSymbol(Block, 0, "symbol3", 0, jitlink::Linkage::Strong,
                         jitlink::Scope::Hidden, false, false);
  jitlink::Symbol &StrongLocalDead =
      G.addDefinedSymbol(Block, 0, "symbol4", 0, jitlink::Linkage::Strong,
                         jitlink::Scope::Local, false, false);
  jitlink::Symbol &WeakDefaultDead =
      G.addDefinedSymbol(Block, 0, "symbol5", 0, jitlink::Linkage::Weak,
                         jitlink::Scope::Default, false, false);

  SymbolRef::Flags StrongDefaultDeadFlags =
      SymbolRef::getFlags(StrongDefaultDead);
  SymbolRef::Flags StrongDefaultLiveFlags =
      SymbolRef::getFlags(StrongDefaultLive);
  SymbolRef::Flags StrongHiddenDeadFlags =
      SymbolRef::getFlags(StrongHiddenDead);
  SymbolRef::Flags StrongLocalDeadFlags = SymbolRef::getFlags(StrongLocalDead);
  SymbolRef::Flags WeakDefaultDeadFlags = SymbolRef::getFlags(WeakDefaultDead);

  for (SymbolRef::Flags F : {
           StrongDefaultDeadFlags,
           StrongDefaultLiveFlags,
           StrongHiddenDeadFlags,
           StrongLocalDeadFlags,
       })
    EXPECT_EQ(SymbolRef::M_Never, F.Merge);
  for (SymbolRef::Flags F : {
           StrongDefaultDeadFlags,
           StrongHiddenDeadFlags,
           WeakDefaultDeadFlags,
       })
    EXPECT_EQ(SymbolRef::DS_LinkUnit, F.DeadStrip);
  for (SymbolRef::Flags F : {
           StrongDefaultDeadFlags,
           StrongDefaultLiveFlags,
           WeakDefaultDeadFlags,
       })
    EXPECT_EQ(SymbolRef::S_Global, F.Scope);

  EXPECT_EQ(SymbolRef::DS_Never, StrongDefaultLiveFlags.DeadStrip);
  EXPECT_EQ(SymbolRef::DS_CompileUnit, StrongLocalDeadFlags.DeadStrip);
  EXPECT_EQ(SymbolRef::S_Hidden, StrongHiddenDeadFlags.Scope);
  EXPECT_EQ(SymbolRef::S_Local, StrongLocalDeadFlags.Scope);
  EXPECT_EQ(SymbolRef::M_ByName, WeakDefaultDeadFlags.Merge);
}

TEST(CASObjectFormatTests, BlockSymbols) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8,
                       support::little, jitlink::getGenericEdgeKindName);
  jitlink::Section &Section = G.createSection("section", sys::Memory::MF_EXEC);
  jitlink::Block &Block = G.createContentBlock(Section, BlockContent, 0, 16, 0);
  jitlink::Symbol *Symbols[] = {
      &G.addCommonSymbol("common-symbol1", jitlink::Scope::Default, Section, 0,
                         16, 16, true),
      &G.addCommonSymbol("common-symbol2", jitlink::Scope::Default, Section, 0,
                         16, 16, false),
      &G.addCommonSymbol("common-symbol3", jitlink::Scope::Hidden, Section, 0,
                         16, 16, false),
      &G.addCommonSymbol("common-symbol4", jitlink::Scope::Local, Section, 0,
                         16, 16, false),
      &G.addAnonymousSymbol(Block, 0, 0, false, false),
      &G.addAnonymousSymbol(Block, 0, 0, false, true),
      &G.addAnonymousSymbol(Block, 0, 0, true, true),
      &G.addAnonymousSymbol(Block, 0, 0, true, true),
      &G.addAnonymousSymbol(Block, 8, 0, true, true),
      &G.addAnonymousSymbol(Block, 8, 8, true, true),
      &G.addDefinedSymbol(Block, 0, "defined-symbol", 0,
                          jitlink::Linkage::Strong, jitlink::Scope::Default,
                          false, false),
  };

  std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
  ObjectFileSchema Schema(*CAS);
  auto createTarget = [](const jitlink::Symbol &S, jitlink::Edge::Kind,
                         bool IsFromData, jitlink::Edge::AddendT &,
                         Optional<StringRef> &) -> Expected<TargetRef> {
    return createStringError(inconvertibleErrorCode(), "expected leaf blocks");
  };
  auto createSymbolDefinition =
      [&](const jitlink::Block &B) -> Expected<SymbolDefinitionRef> {
    if (Expected<BlockRef> Block =
            BlockRef::create(Schema, B, createTarget))
      return Block->getAsSymbolDefinition();
    else
      return Block.takeError();
  };

  for (jitlink::Symbol *S : Symbols) {
    // This is the API being tested.
    Optional<SymbolRef> Symbol =
        expectedToOptional(SymbolRef::create(Schema, *S, createSymbolDefinition));
    ASSERT_TRUE(Symbol);

    // Check the flags match.
    EXPECT_EQ(SymbolRef::getFlags(*S), Symbol->getFlags());

    // Get out a block independently and check that it's correct.
    Optional<BlockRef> Block = expectedToOptional(
        BlockRef::create(Schema, S->getBlock(), createTarget));
    ASSERT_TRUE(Block);

    // Check that the block is correct.
    EXPECT_EQ(Symbol->getDefinitionID(), *Block);
  }
}

TEST(CASObjectFormatTests, SymbolTable) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8,
                       support::little, jitlink::getGenericEdgeKindName);
  jitlink::Section &Section = G.createSection("section", sys::Memory::MF_EXEC);
  jitlink::Block &Block = G.createContentBlock(Section, BlockContent, 0, 16, 0);
  jitlink::Symbol *Symbols[] = {
      &G.addAnonymousSymbol(Block, 0, 0, false, /*IsLive*/ false),
      &G.addAnonymousSymbol(Block, 0, 0, false, /*IsLive*/ true),
      &G.addCommonSymbol("symbol1", jitlink::Scope::Default, Section, 0, 16, 16,
                         true),
      &G.addCommonSymbol("symbol2", jitlink::Scope::Default, Section, 0, 16, 16,
                         true),
  };

  std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
  ObjectFileSchema Schema(*CAS);
  auto createTarget = [](const jitlink::Symbol &, jitlink::Edge::Kind,
                         bool IsFromData, jitlink::Edge::AddendT &,
                         Optional<StringRef> &) -> Expected<TargetRef> {
    return createStringError(inconvertibleErrorCode(), "expected leaf blocks");
  };
  auto createSymbolDefinition =
      [&](const jitlink::Block &B) -> Expected<SymbolDefinitionRef> {
    if (Expected<BlockRef> Block =
            BlockRef::create(Schema, B, createTarget))
      return Block->getAsSymbolDefinition();
    else
      return Block.takeError();
  };

  SmallVector<SymbolRef> AllRefs;
  for (jitlink::Symbol *S : Symbols) {
    Optional<SymbolRef> Ref =
        expectedToOptional(SymbolRef::create(Schema, *S, createSymbolDefinition));
    ASSERT_TRUE(Ref);
    AllRefs.push_back(*Ref);
  }

  SmallVector<SymbolRef> TableRefs = {
      AllRefs[0], // anonymous, dead-strip=link
      AllRefs[3], // symbol2
      AllRefs[1], // anonymous, dead-strip=never
      AllRefs[0], // anonymous, dead-strip=link
      AllRefs[2], // symbol1
      AllRefs[1], // anonymous, dead-strip=never
  };

  // This is the API being tested.
  Optional<SymbolTableRef> Table =
      expectedToOptional(SymbolTableRef::create(Schema, TableRefs));
  ASSERT_TRUE(Table);

  // Check the order.
  SmallVector<SymbolRef> ExpectedOrder = {
      AllRefs[1], // anonymous, dead-strip=never
      AllRefs[0], // anonymous, dead-strip=link
      AllRefs[2], // symbol1
      AllRefs[3], // symbol2
  };
  EXPECT_EQ(2u, Table->getNumAnonymousSymbols());
  ASSERT_EQ(ExpectedOrder.size(), Table->getNumSymbols());
  for (size_t I = 0, E = ExpectedOrder.size(); I != E; ++I) {
    Optional<SymbolRef> Symbol = expectedToOptional(Table->getSymbol(I));
    ASSERT_TRUE(Symbol);
    EXPECT_EQ(ExpectedOrder[I].getID(), Symbol->getID());
  }
}

TEST(CASObjectFormatTests, BlockWithEdges) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8,
                       support::little, jitlink::getGenericEdgeKindName);
  jitlink::Section &Section = G.createSection("section", sys::Memory::MF_EXEC);
  jitlink::Symbol &S1 = G.addExternalSymbol("S1", 0, jitlink::Linkage::Strong);
  jitlink::Symbol &S2 = G.addExternalSymbol("S2", 0, jitlink::Linkage::Strong);

  // Add a defined symbol so there are two types of targets.
  jitlink::Block &Z = G.createZeroFillBlock(Section, 256, 0, 256, 0);
  jitlink::Symbol &S3 =
      G.addDefinedSymbol(Z, 0, "S3", 0, jitlink::Linkage::Strong,
                         jitlink::Scope::Default, false, false);

  jitlink::Block &B = G.createContentBlock(Section, BlockContent, 0, 256, 0);

  // Create arrays of each field. Sort by the order the edges should be sorted
  // (precedence is offset, kind, target name, and addend).
  jitlink::Edge::OffsetT Offsets[] = {0, 0, 0, 0, 0, 0, 8, 16};
  jitlink::Edge::Kind Kinds[] = {
      jitlink::Edge::FirstKeepAlive,
      jitlink::Edge::FirstKeepAlive,
      jitlink::Edge::FirstKeepAlive,
      jitlink::Edge::FirstKeepAlive,
      jitlink::Edge::FirstRelocation,
      jitlink::Edge::FirstRelocation + 1,
      jitlink::Edge::FirstKeepAlive,
      jitlink::Edge::FirstKeepAlive,
  };
  jitlink::Symbol *Targets[] = {&S1, &S1, &S2, &S3, &S1, &S1, &S1, &S1};
  jitlink::Edge::AddendT Addends[] = {0, 24, 0, 0, 0, 0, 0, 0};

  // Add the edges, out of order.
  size_t AddOrder[] = {2, 1, 0, 6, 7, 3, 5, 4};
  static_assert(std::extent<decltype(AddOrder)>::value ==
                    std::extent<decltype(Offsets)>::value,
                "Check array sizes");
  static_assert(std::extent<decltype(AddOrder)>::value ==
                    std::extent<decltype(Kinds)>::value,
                "Check array sizes");
  static_assert(std::extent<decltype(AddOrder)>::value ==
                    std::extent<decltype(Targets)>::value,
                "Check array sizes");
  static_assert(std::extent<decltype(AddOrder)>::value ==
                    std::extent<decltype(Addends)>::value,
                "Check array sizes");
  for (size_t I : AddOrder)
    B.addEdge(Kinds[I], Offsets[I], *Targets[I], Addends[I]);

  // Prepare dependencies in the CAS.
  std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
  ObjectFileSchema Schema(*CAS);

  // Create the external symbols.
  SmallDenseMap<jitlink::Symbol *, Optional<TargetRef>, 8> CreatedSymbols;
  for (jitlink::Symbol *S : {&S1, &S2}) {
    auto Indirect = expectedToOptional(IndirectSymbolRef::create(Schema, *S));
    ASSERT_TRUE(Indirect);
    CreatedSymbols[S] = Indirect->getAsTarget();
  }

  // Create the block and symbol for S3.
  Optional<BlockRef> ZeroBlock = expectedToOptional(BlockRef::create(
      Schema, Z,
      [](const jitlink::Symbol &S, jitlink::Edge::Kind, bool IsFromData,
         jitlink::Edge::AddendT &,
         Optional<StringRef> &) -> Expected<TargetRef> {
        return createStringError(inconvertibleErrorCode(),
                                 "expected leaf block");
      }));
  ASSERT_TRUE(ZeroBlock);
  Optional<SymbolRef> DirectS3 = expectedToOptional(SymbolRef::create(
      Schema, S3, [&](const jitlink::Block &B) -> Expected<SymbolDefinitionRef> {
        assert(&B == &Z);
        return ZeroBlock->getAsSymbolDefinition();
      }));
  ASSERT_TRUE(DirectS3);
  CreatedSymbols[&S3] = DirectS3->getAsTarget();

  auto createTarget = [&](const jitlink::Symbol &S, jitlink::Edge::Kind,
                          bool IsFromData, jitlink::Edge::AddendT &,
                          Optional<StringRef> &) -> Expected<TargetRef> {
    return *CreatedSymbols.lookup(&S);
  };

  // This is the API being tested, which should build an edge list.
  Optional<BlockRef> Block =
      expectedToOptional(BlockRef::create(Schema, B, createTarget));
  ASSERT_TRUE(Block);
  EXPECT_TRUE(Block->hasEdges());
  Optional<TargetInfoListRef> TargetInfoList = cantFail(Block->getTargetInfo());
  FixupList Fixups = cantFail(Block->getFixups());
  Optional<TargetListRef> TargetList = cantFail(Block->getTargets());
  ASSERT_EQ(std::extent<decltype(AddOrder)>::value,
            size_t(std::distance(Fixups.begin(), Fixups.end())));
  ASSERT_EQ(std::extent<decltype(AddOrder)>::value,
            TargetInfoList->getNumEdges());
  ASSERT_EQ(3u, TargetList->getNumTargets());

  // Check the fixups and targets, in sorted order.
  FixupList::iterator F = Fixups.begin();
  for (size_t I = 0, E = std::extent<decltype(AddOrder)>::value; I != E;
       ++I, ++F) {
    size_t TargetIndex = TargetInfoList->getTargetIndex(I);
    ASSERT_LT(TargetIndex, TargetList->getNumTargets());
    Optional<TargetRef> Target =
        expectedToOptional(TargetList->getTarget(TargetIndex));
    ASSERT_TRUE(Target);
    TargetRef ExpectedTarget = *CreatedSymbols.lookup(Targets[I]);

    // These are easy to test.
    EXPECT_EQ(Kinds[I], F->Kind);
    EXPECT_EQ(Offsets[I], F->Offset);
    EXPECT_EQ(ExpectedTarget.getID(), Target->getID());
    EXPECT_EQ(Addends[I], TargetInfoList->getAddend(I));
  }
}

} // namespace
