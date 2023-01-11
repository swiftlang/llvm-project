//===- CASObjectFormats/LinkGraph.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CASObjectFormats/LinkGraph.h"
#include "llvm/CASObjectFormats/CASObjectReader.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::reader;
using namespace llvm::jitlink;

namespace {

class LinkGraphBuilder {
public:
  LinkGraphBuilder(const CASObjectReader &Reader, StringRef Name,
                   const Triple &TT, unsigned PointerSize,
                   support::endianness Endianness,
                   LinkGraph::GetEdgeKindNameFunction GetEdgeKindName);

  struct SectionInfo {
    jitlink::Section *Section = nullptr;
    unsigned Size = 0;
    unsigned Alignment = 1;
  };

  Error addSymbol(CASSymbolRef Ref, const CASSymbol &Info);
  Expected<Symbol *> createSymbol(const CASSymbol &Info);
  Expected<Block *> createBlock(const CASBlock &Info);
  Section *createSection(const CASSection &Info);

  Expected<Symbol *> getOrCreateSymbol(CASSymbolRef Ref);
  Expected<Block *> getOrCreateBlock(CASBlockRef Ref);
  Expected<SectionInfo &> getOrCreateSection(CASSectionRef Ref);

  Block *getExistingBlock(CASBlockRef Ref) const;

  Error addFixups();
  Error addEdge(Block &Blk, const CASBlockFixup &Fixup);

  std::unique_ptr<LinkGraph> takeLinkGraph() { return std::move(LG); }

  const CASObjectReader &Reader;
  std::unique_ptr<LinkGraph> LG;

  DenseMap<CASSymbolRef, Symbol *> Symbols;
  DenseMap<CASBlockRef, Block *> Blocks;
  DenseMap<CASSectionRef, SectionInfo> Sections;

  struct BlockForFixup {
    CASBlockRef Ref;
    Block *Blk;
  };
  SmallVector<BlockForFixup> BlocksForFixup;
};

} // anonymous namespace

LinkGraphBuilder::LinkGraphBuilder(
    const CASObjectReader &Reader, StringRef Name, const Triple &TT,
    unsigned PointerSize, support::endianness Endianness,
    LinkGraph::GetEdgeKindNameFunction GetEdgeKindName)
    : Reader(Reader) {
  LG.reset(
      new LinkGraph(Name.str(), TT, PointerSize, Endianness, GetEdgeKindName));
}

Error LinkGraphBuilder::addSymbol(CASSymbolRef Ref, const CASSymbol &Info) {
  Expected<Symbol *> ExpSym = createSymbol(Info);
  if (!ExpSym)
    return ExpSym.takeError();
  Symbols[Ref] = *ExpSym;
  return Error::success();
}

Expected<Symbol *> LinkGraphBuilder::createSymbol(const CASSymbol &Info) {
  if (!Info.isDefined()) {
    return &LG->addExternalSymbol(Info.Name, /*Size=*/0,
                                  Info.Linkage == Linkage::Weak);
  }

  Expected<Block *> Block = getOrCreateBlock(*Info.BlockRef);
  if (!Block)
    return Block.takeError();
  auto &Symbol =
      Info.Name.empty()
          ? LG->addAnonymousSymbol(**Block, Info.Offset,
                                   /*Size=*/0, Info.IsCallable, Info.IsLive)
          : LG->addDefinedSymbol(**Block, Info.Offset, Info.Name, /*Size=*/0,
                                 Info.Linkage, Info.Scope, Info.IsCallable,
                                 Info.IsLive);
  Symbol.setAutoHide(Info.IsAutoHide);
  return &Symbol;
}

static orc::ExecutorAddr
getAlignedAddress(LinkGraphBuilder::SectionInfo &Section, uint64_t Size,
                  uint64_t Alignment, uint64_t AlignmentOffset) {
  uint64_t Address = alignTo(Section.Size + AlignmentOffset, Align(Alignment)) -
                     AlignmentOffset;
  Section.Size = Address + Size;
  if (Alignment > Section.Alignment)
    Section.Alignment = Alignment;
  return orc::ExecutorAddr(Address);
}

Expected<Block *> LinkGraphBuilder::createBlock(const CASBlock &Info) {
  Expected<SectionInfo &> SectionInfo = getOrCreateSection(Info.SectionRef);
  if (!SectionInfo)
    return SectionInfo.takeError();
  auto Address = getAlignedAddress(*SectionInfo, Info.Size, Info.Alignment,
                                   Info.AlignmentOffset);
  auto &B =
      Info.isZeroFill()
          ? LG->createZeroFillBlock(*SectionInfo->Section, Info.Size, Address,
                                    Info.Alignment, Info.AlignmentOffset)
          : LG->createContentBlock(
                *SectionInfo->Section,
                ArrayRef(Info.Content->begin(), Info.Content->end()),
                Address, Info.Alignment, Info.AlignmentOffset);
  return &B;
}

Section *LinkGraphBuilder::createSection(const CASSection &Info) {
  return &LG->createSection(Info.Name, Info.Prot);
}

Expected<Symbol *> LinkGraphBuilder::getOrCreateSymbol(CASSymbolRef Ref) {
  Symbol *&Sym = Symbols[Ref];
  if (!Sym) {
    Expected<CASSymbol> Info = Reader.materialize(Ref);
    if (!Info)
      return Info.takeError();
    Expected<Symbol *> ExpSym = createSymbol(*Info);
    if (!ExpSym)
      return ExpSym.takeError();
    Sym = *ExpSym;
  }
  return Sym;
}

Expected<Block *> LinkGraphBuilder::getOrCreateBlock(CASBlockRef Ref) {
  Block *&Blk = Blocks[Ref];
  if (!Blk) {
    Expected<CASBlock> Info = Reader.materialize(Ref);
    if (!Info)
      return Info.takeError();
    Expected<Block *> ExpBlk = createBlock(*Info);
    if (!ExpBlk)
      return ExpBlk.takeError();
    Blk = *ExpBlk;
    BlocksForFixup.push_back(BlockForFixup{Ref, Blk});
  }
  return Blk;
}

Expected<LinkGraphBuilder::SectionInfo &>
LinkGraphBuilder::getOrCreateSection(CASSectionRef Ref) {
  SectionInfo &Sec = Sections[Ref];
  if (!Sec.Section) {
    Expected<CASSection> Info = Reader.materialize(Ref);
    if (!Info)
      return Info.takeError();
    Sec.Section = createSection(*Info);
  }
  return Sec;
}

Block *LinkGraphBuilder::getExistingBlock(CASBlockRef Ref) const {
  auto BI = Blocks.find(Ref);
  assert(BI != Blocks.end());
  return BI->second;
}

Error LinkGraphBuilder::addFixups() {
  // Check for \p size() at each iteration because new blocks can be added
  // while reading the fixups, due to new symbols getting discovered when using
  // nestedv1 format.
  // FIXME: Get nestedv1 to provide all the symbols that can be referenced from
  // the translation unit, via \p forEachSymbol().
  for (unsigned I = 0; I < BlocksForFixup.size(); ++I) {
    const BlockForFixup &BlkFF = BlocksForFixup[I];
    Block *Blk = getExistingBlock(BlkFF.Ref);
    Error E = Reader.materializeFixups(
        BlkFF.Ref, [&](const CASBlockFixup &Fixup) -> Error {
          return this->addEdge(*Blk, Fixup);
        });
    if (E)
      return E;
  }
  return Error::success();
}

Error LinkGraphBuilder::addEdge(Block &Parent, const CASBlockFixup &Fixup) {
  Expected<Symbol *> ExpSym = getOrCreateSymbol(Fixup.TargetRef);
  if (!ExpSym)
    return ExpSym.takeError();
  Parent.addEdge(Fixup.Kind, Fixup.Offset, **ExpSym, Fixup.Addend);
  return Error::success();
}

Expected<std::unique_ptr<LinkGraph>>
casobjectformats::createLinkGraph(const CASObjectReader &Reader,
                                  StringRef Name) {
  Triple TT = Reader.getTargetTriple();
  unsigned PointerSize;
  if (TT.isArch64Bit())
    PointerSize = 8;
  else if (TT.isArch32Bit())
    PointerSize = 4;
  else if (TT.isArch16Bit())
    PointerSize = 2;
  else
    return createStringError(
        inconvertibleErrorCode(),
        "could not infer pointer width from target triple: " + TT.str());
  support::endianness Endianness = TT.isLittleEndian()
                                       ? support::endianness::little
                                       : support::endianness::big;
  Expected<LinkGraph::GetEdgeKindNameFunction> GetEdgeKindName =
      getGetEdgeKindNameFunction(TT);
  if (!GetEdgeKindName)
    return GetEdgeKindName.takeError();

  LinkGraphBuilder Builder(Reader, Name, TT, PointerSize, Endianness,
                           *GetEdgeKindName);
  Error E =
      Reader.forEachSymbol([&](CASSymbolRef Ref, CASSymbol Info) -> Error {
        return Builder.addSymbol(Ref, Info);
      });
  if (E)
    return std::move(E);
  E = Builder.addFixups();
  if (E)
    return std::move(E);
  return Builder.takeLinkGraph();
}
