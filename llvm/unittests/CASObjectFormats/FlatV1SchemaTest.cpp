//===- FlatV1SchemaTest.cpp - Unit tests CAS object file schema -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CASObjectFormats/FlatV1.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Memory.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::flatv1;

namespace {

static const char BlockContentBytes[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                         0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
                                         0x1C, 0x1D, 0x1E, 0x1F, 0x00};
ArrayRef<char> BlockContent(BlockContentBytes);

TEST(FlatV1SchemaTest, RoundTrip) {
  jitlink::LinkGraph G("graph", Triple("x86_64-apple-darwin"), 8,
                       support::little, jitlink::getGenericEdgeKindName);
  jitlink::Section &Section = G.createSection("section", sys::Memory::MF_EXEC);
  jitlink::Symbol &S1 = G.addExternalSymbol("S1", 0, jitlink::Linkage::Strong);
  jitlink::Symbol &S2 = G.addExternalSymbol("S2", 0, jitlink::Linkage::Weak);

  // Add a defined symbol so there are two types of targets.
  jitlink::Block &Z = G.createZeroFillBlock(Section, 256, 0, 256, 0);
  jitlink::Symbol &S3 =
      G.addDefinedSymbol(Z, 0, "S3", 0, jitlink::Linkage::Strong,
                         jitlink::Scope::Default, false, false);

  auto createBlock = [&]() -> jitlink::Block & {
    jitlink::Block &B = G.createContentBlock(Section, BlockContent, 0, 256, 0);

    // Create arrays of each field. Sort by the order the edges should be sorted
    // (precedence is offset, kind, target name, and addend).
    jitlink::Edge::OffsetT Offsets[] = {0, 0, 0, 0, 0, 0, 8, 16};
    jitlink::Edge::Kind Kinds[] = {
        jitlink::Edge::FirstKeepAlive,  jitlink::Edge::FirstKeepAlive,
        jitlink::Edge::FirstKeepAlive,  jitlink::Edge::FirstKeepAlive,
        jitlink::Edge::FirstRelocation, jitlink::Edge::FirstRelocation + 1,
        jitlink::Edge::FirstKeepAlive,  jitlink::Edge::FirstKeepAlive,
    };
    jitlink::Symbol *Targets[] = {&S1, &S1, &S2, &S3, &S1, &S1, &S1, &S1};
    jitlink::Edge::AddendT Addends[] = {0, 24, 0, 0, 0, -4, 0, 0};

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

    return B;
  };

  jitlink::Block &B = createBlock();

  // Add a few symbols pointing at "B".
  G.addDefinedSymbol(B, 0, "B1", 0, jitlink::Linkage::Strong,
                     jitlink::Scope::Default, false, false);
  G.addDefinedSymbol(B, 0, "B2", 0, jitlink::Linkage::Weak,
                     jitlink::Scope::Default, false, false);
  G.addDefinedSymbol(B, 0, "B3", 0, jitlink::Linkage::Strong,
                     jitlink::Scope::Hidden, false, false);
  G.addDefinedSymbol(B, 0, "B4", 0, jitlink::Linkage::Weak,
                     jitlink::Scope::Hidden, false, false);

  // Creaate duplicate block;
  jitlink::Block &DupB = createBlock();
  G.addDefinedSymbol(DupB, 0, "DupB1", 0, jitlink::Linkage::Strong,
                     jitlink::Scope::Default, false, false);

  std::unique_ptr<jitlink::LinkGraph> RoundTripG;
  {
    // Convert to cas.o.
    std::unique_ptr<cas::CASDB> CAS = cas::createInMemoryCAS();
    ObjectFileSchema Schema(*CAS);
    auto CU = CompileUnitRef::create(Schema, G);
    EXPECT_THAT_EXPECTED(CU, Succeeded());
    
    // Convert back to LinkGraph.
    auto RoundTripGOrError =
        CU->createLinkGraph("round-tripped", jitlink::getGenericEdgeKindName);
    EXPECT_THAT_EXPECTED(RoundTripGOrError, Succeeded());
    RoundTripG = std::move(*RoundTripGOrError);
  }

  // Check linkage for named symbols.
  auto mapSymbols = [](const jitlink::LinkGraph &G,
                       StringMap<const jitlink::Symbol *> &Map) {
    for (const jitlink::Symbol *S : G.defined_symbols())
      if (S->hasName())
        Map[S->getName()] = S;
    for (const jitlink::Symbol *S : G.absolute_symbols())
      Map[S->getName()] = S;
    for (const jitlink::Symbol *S : G.external_symbols())
      Map[S->getName()] = S;
  };
  auto collectSymbols = [](const jitlink::LinkGraph &G,
                           SmallVectorImpl<const jitlink::Symbol *> &Vector) {
    for (const jitlink::Symbol *S : G.defined_symbols())
      Vector.push_back(S);
    for (const jitlink::Symbol *S : G.absolute_symbols())
      Vector.push_back(S);
    for (const jitlink::Symbol *S : G.external_symbols())
      Vector.push_back(S);
  };
  StringMap<const jitlink::Symbol *> RoundTripSymbols;
  mapSymbols(*RoundTripG, RoundTripSymbols);
  SmallVector<const jitlink::Symbol *> OriginalSymbols;
  collectSymbols(G, OriginalSymbols);
  for (const jitlink::Symbol *S : OriginalSymbols) {
    if (!S->hasName()) // Skip for now.
      continue;

    const jitlink::Symbol *RoundTripS = RoundTripSymbols.lookup(S->getName());
    StringRef RoundTripName = RoundTripS ? RoundTripS->getName() : "";
    // Check the name rather than for nullptr so we get a good error message.
    EXPECT_EQ(S->getName(), RoundTripName);
    if (!RoundTripS)
      continue;

    EXPECT_EQ(S->isDefined(), RoundTripS->isDefined());
    EXPECT_EQ(S->isAbsolute(), RoundTripS->isAbsolute());
    EXPECT_EQ(S->isExternal(), RoundTripS->isExternal());
    EXPECT_EQ(S->getLinkage(), RoundTripS->getLinkage());
    EXPECT_EQ(S->getScope(), RoundTripS->getScope());
    if (S->isDefined()) {
      const jitlink::Block &B = S->getBlock();
      const jitlink::Block &RoundTripB = RoundTripS->getBlock();

      auto getSortedEdges = [](const jitlink::Block &B) {
        std::vector<jitlink::Edge> Edges;
        llvm::copy(B.edges(), std::back_inserter(Edges));
        llvm::sort(
            Edges, [](const jitlink::Edge &LHS, const jitlink::Edge &RHS) {
              if (LHS.getOffset() != RHS.getOffset())
                return LHS.getOffset() < RHS.getOffset();
              if (LHS.getAddend() != RHS.getAddend())
                return LHS.getAddend() < RHS.getAddend();
              if (LHS.getKind() != RHS.getKind())
                return LHS.getKind() < RHS.getKind();
              return LHS.getTarget().getName() < RHS.getTarget().getName();
            });
        return Edges;
      };
      std::vector<jitlink::Edge> Edges = getSortedEdges(B);
      std::vector<jitlink::Edge> RoundTripEdges = getSortedEdges(RoundTripB);

      ASSERT_EQ(Edges.size(), RoundTripEdges.size());
      for (unsigned i = 0, e = Edges.size(); i != e; ++i) {
        const jitlink::Edge &E = Edges[i];
        const jitlink::Edge &RoundTripE = RoundTripEdges[i];
        EXPECT_EQ(E.getOffset(), RoundTripE.getOffset());
        EXPECT_EQ(E.getKind(), RoundTripE.getKind());
        EXPECT_EQ(E.isRelocation(), RoundTripE.isRelocation());
        EXPECT_EQ(E.isKeepAlive(), RoundTripE.isKeepAlive());
        EXPECT_EQ(E.getTarget().getName(), RoundTripE.getTarget().getName());
        EXPECT_EQ(E.getAddend(), RoundTripE.getAddend());
      }
    }
  }
}

} // namespace
