//===- CASObjectFormats/FlatV1.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CASObjectFormats/FlatV1.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/CASObjectFormats/ObjectFormatHelpers.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/Support/EndianStream.h"
#include <memory>

// FIXME: For cl::opt. Should thread through or delete.
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::flatv1;

const StringLiteral CompileUnitRef::KindString;
const StringLiteral NameRef::KindString;
const StringLiteral SectionRef::KindString;
const StringLiteral BlockRef::KindString;
const StringLiteral SymbolRef::KindString;
const StringLiteral EdgeListRef::KindString;
const StringLiteral BlockContentRef::KindString;

void ObjectFileSchema::anchor() {}

cl::opt<bool> NestEdgesInBlock("nest-edges-in-block",
                               cl::desc("Put edges in block"), cl::init(false));
cl::opt<bool> EncodeIndexInCU("encode-index-in-cu",
                              cl::desc("Encode all indexes in CU"),
                              cl::init(false));
cl::opt<bool> NameInSymbols("name-in-symbols",
                            cl::desc("Encode symbol name in SymbolRef"),
                            cl::init(false));
cl::opt<bool> UseBlockContentNode("use-block-content",
                            cl::desc("Use block-content"),
                            cl::init(false));

Expected<cas::NodeRef>
ObjectFileSchema::createFromLinkGraphImpl(const jitlink::LinkGraph &G,
                                          raw_ostream *DebugOS) const {
  return CompileUnitRef::create(*this, G, DebugOS);
}

Expected<std::unique_ptr<jitlink::LinkGraph>>
ObjectFileSchema::createLinkGraphImpl(
    cas::NodeRef RootNode, StringRef Name,
    jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName,
    raw_ostream *) const {
  if (!isRootNode(RootNode))
    return createStringError(inconvertibleErrorCode(), "invalid root node");
  auto CU = CompileUnitRef::get(*this, RootNode);
  if (!CU)
    return CU.takeError();
  return CU->createLinkGraph(Name, GetEdgeKindName);
}

ObjectFileSchema::ObjectFileSchema(cas::CASDB &CAS) : SchemaBase(CAS) {
  // Fill the cache immediately to preserve thread-safety.
  if (Error E = fillCache())
    report_fatal_error(std::move(E));
}

Error ObjectFileSchema::fillCache() {
  Optional<cas::CASID> RootKindID;
  const unsigned Version = 0; // Bump this to error on old object files.
  if (Expected<cas::NodeRef> ExpectedRootKind =
          CAS.createNode(None, "cas.o:flatv1:schema:" + Twine(Version).str()))
    RootKindID = *ExpectedRootKind;
  else
    return ExpectedRootKind.takeError();

  StringRef AllKindStrings[] = {
      BlockRef::KindString,  CompileUnitRef::KindString,
      NameRef::KindString,   SectionRef::KindString,
      SymbolRef::KindString, EdgeListRef::KindString,
      BlockContentRef::KindString,
  };
  cas::CASID Refs[] = {*RootKindID};
  SmallVector<cas::CASID> IDs = {*RootKindID};
  for (StringRef KS : AllKindStrings) {
    auto ExpectedID = CAS.createNode(Refs, KS);
    if (!ExpectedID)
      return ExpectedID.takeError();
    IDs.push_back(*ExpectedID);
    KindStrings.push_back(std::make_pair(KindStrings.size(), KS));
    assert(KindStrings.size() < UCHAR_MAX &&
           "Ran out of bits for kind strings");
  }

  auto ExpectedTypeID = CAS.createNode(IDs, "cas.o:flatv1:root");
  if (!ExpectedTypeID)
    return ExpectedTypeID.takeError();
  RootNodeTypeID = *ExpectedTypeID;
  return Error::success();
}

Optional<StringRef>
ObjectFileSchema::getKindString(const cas::NodeRef &Node) const {
  assert(&Node.getCAS() == &CAS);
  StringRef Data = Node.getData();
  if (Data.empty())
    return None;

  unsigned char ID = Data[0];
  for (auto &I : KindStrings)
    if (I.first == ID)
      return I.second;
  return None;
}

bool ObjectFileSchema::isRootNode(const cas::NodeRef &Node) const {
  if (Node.getNumReferences() < 1)
    return false;
  return Node.getReference(0) == *RootNodeTypeID;
}

bool ObjectFileSchema::isNode(const cas::NodeRef &Node) const {
  // This is a very weak check!
  return bool(getKindString(Node));
}

Expected<ObjectFormatNodeRef::Builder>
ObjectFormatNodeRef::Builder::startRootNode(const ObjectFileSchema &Schema,
                                            StringRef KindString) {
  Builder B(Schema);
  B.IDs.push_back(Schema.getRootNodeTypeID());

  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Error ObjectFormatNodeRef::Builder::startNodeImpl(StringRef KindString) {
  Optional<unsigned char> TypeID = Schema->getKindStringID(KindString);
  if (!TypeID)
    return createStringError(inconvertibleErrorCode(),
                             "invalid object format kind string: " +
                                 KindString);
  Data.push_back(*TypeID);
  return Error::success();
}

Expected<ObjectFormatNodeRef::Builder>
ObjectFormatNodeRef::Builder::startNode(const ObjectFileSchema &Schema,
                                        StringRef KindString) {
  Builder B(Schema);
  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Expected<ObjectFormatNodeRef> ObjectFormatNodeRef::Builder::build() {
  return ObjectFormatNodeRef::get(*Schema, Schema->CAS.createNode(IDs, Data));
}

StringRef ObjectFormatNodeRef::getKindString() const {
  Optional<StringRef> KS = getSchema().getKindString(*this);
  assert(KS && "Expected valid kind string");
  return *KS;
}

Optional<unsigned char>
ObjectFileSchema::getKindStringID(StringRef KindString) const {
  for (auto &I : KindStrings)
    if (I.second == KindString)
      return I.first;
  return None;
}

static Expected<StringRef> consumeDataOfSize(StringRef &Data, unsigned Size) {
  if (Data.size() < Size)
    return createStringError(inconvertibleErrorCode(),
                             "Requested data go beyond the buffer");

  auto Ret = Data.take_front(Size);
  Data = Data.drop_front(Size);

  return Ret;
}

static bool compareBlocksByAddress(const jitlink::Block *LHS,
                                   const jitlink::Block *RHS) {
  if (LHS == RHS)
    return false;

  JITTargetAddress LAddr = LHS->getAddress();
  JITTargetAddress RAddr = RHS->getAddress();
  if (LAddr != RAddr)
    return LAddr < RAddr;

  return LHS->getSize() < RHS->getSize();
}

static bool compareEdges(const jitlink::Edge *LHS, const jitlink::Edge *RHS) {
  if (LHS == RHS)
    return false;

  if (LHS->getOffset() != RHS->getOffset())
    return LHS->getOffset() < RHS->getOffset();

  if (LHS->getAddend() != RHS->getAddend())
    return LHS->getAddend() < RHS->getAddend();

  if (LHS->getKind() != RHS->getKind())
    return LHS->getKind() < RHS->getKind();

  return helpers::compareSymbolsBySemanticsAnd(
      &LHS->getTarget(), &RHS->getTarget(), helpers::compareSymbolsByAddress);
}


static uint64_t getAlignedAddress(LinkGraphBuilder::SectionInfo &Section,
                                  uint64_t Size, uint64_t Alignment,
                                  uint64_t AlignmentOffset) {
  uint64_t Address = alignTo(Section.Size + AlignmentOffset, Align(Alignment)) -
                     AlignmentOffset;
  Section.Size = Address + Size;
  if (Alignment > Section.Alignment)
    Section.Alignment = Alignment;
  return Address;
}

Expected<ObjectFormatNodeRef>
ObjectFormatNodeRef::get(const ObjectFileSchema &Schema,
                         Expected<cas::NodeRef> Ref) {
  if (!Ref)
    return Ref.takeError();
  if (!Schema.isNode(*Ref))
    return createStringError(
        inconvertibleErrorCode(),
        "invalid kind-string for node in object-file-schema");
  return ObjectFormatNodeRef(Schema, *Ref);
}

Expected<NameRef> NameRef::create(CompileUnitBuilder &CUB, StringRef Name) {
  auto B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Name);
  return get(B->build());
}

Expected<NameRef> NameRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  if (Specific->getNumReferences())
    return createStringError(inconvertibleErrorCode(), "corrupt name");

  return NameRef(*Specific);
}

Expected<SectionRef> SectionRef::create(CompileUnitBuilder &CUB,
                                        const jitlink::Section &S) {
  auto B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.push_back(
      uint8_t(data::encodeProtectionFlags(S.getProtectionFlags())));

  B->Data.append(S.getName());

  return get(B->build());
}

Error SectionRef::materialize(LinkGraphBuilder &LGB, unsigned Idx) const {
  auto Flags =
      decodeProtectionFlags((data::SectionProtectionFlags)getData()[0]);

  StringRef Name(getData().drop_front(1));

  auto &Section = LGB.getLinkGraph()->createSection(Name, Flags);
  return LGB.addSection(Idx, &Section);
}

Expected<SectionRef> SectionRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return SectionRef(*Specific);
}

Expected<EdgeListRef> EdgeListRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return EdgeListRef(*Specific);
}

static Error encodeEdge(CompileUnitBuilder &CUB, SmallVectorImpl<char> &Data,
                        const jitlink::Edge *E, bool ForceDirectIndex = false) {

  auto SymbolIndex = CUB.getSymbolIndex(E->getTarget());
  if (!SymbolIndex)
    return SymbolIndex.takeError();

  if (EncodeIndexInCU && !ForceDirectIndex)
    CUB.encodeIndex(*SymbolIndex);
  else
    encoding::writeVBR8(*SymbolIndex, Data);

  // FIXME: Some of the fields are not stable.
  unsigned char Bits = 0;
  Bits |= E->isKeepAlive();
  encoding::writeVBR8(Bits, Data);
  encoding::writeVBR8(E->getKind(), Data);
  encoding::writeVBR8(E->getOffset(), Data);
  encoding::writeVBR8(E->getAddend(), Data);
  return Error::success();
}

static Error decodeEdge(LinkGraphBuilder &LGB, StringRef Data,
                        jitlink::Block &Parent, unsigned BlockIdx,
                        bool ForceDirectIndex = false) {
  unsigned SymbolIdx;

  if (EncodeIndexInCU && !ForceDirectIndex)
    SymbolIdx = LGB.nextIdxForBlock(BlockIdx);
  else if (auto E = encoding::consumeVBR8(Data, SymbolIdx))
    return E;

  auto Symbol = LGB.getSymbol(SymbolIdx);
  if (!Symbol)
    return Symbol.takeError();

  unsigned char Bits, Kind;
  unsigned Offset, Addend;
  auto E = encoding::consumeVBR8(Data, Bits);
  if (!E)
    E = encoding::consumeVBR8(Data, Kind);
  if (!E)
    E = encoding::consumeVBR8(Data, Offset);
  if (!E)
    E = encoding::consumeVBR8(Data, Addend);
  if (E)
    return E;

  Parent.addEdge((jitlink::Edge::Kind)Kind, Offset, **Symbol, Addend);

  return Error::success();
}

Expected<EdgeListRef>
EdgeListRef::create(CompileUnitBuilder &CUB,
                    ArrayRef<const jitlink::Edge *> Edges) {
  auto B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  encoding::writeVBR8(Edges.size(), B->Data);
  for (const auto *E : Edges) {
    // EdgeList is too "nested" to encode index in CU.
    if (auto Err = encodeEdge(CUB, B->Data, E, true))
      return Err;
  }

  return get(B->build());
}

Error EdgeListRef::materialize(LinkGraphBuilder &LGB, jitlink::Block &Parent,
                               unsigned BlockIdx) const {
  auto Remaining = getData();

  unsigned EdgeSize;
  auto E = encoding::consumeVBR8(Remaining, EdgeSize);
  if (E)
    return E;

  for (unsigned I = 0; I < EdgeSize; ++I) {
    // EdgeList is too "nested" to encode index in CU.
    if (auto Err = decodeEdge(LGB, Remaining, Parent, BlockIdx, true))
      return Err;
  }
  return Error::success();
}

Expected<BlockRef> BlockRef::create(CompileUnitBuilder &CUB,
                                    const jitlink::Block &Block) {
  Expected<Builder> B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  // Encode Section.
  auto SectionIndex = CUB.getSectionIndex(Block.getSection());
  if (!SectionIndex)
    return SectionIndex.takeError();
  if (EncodeIndexInCU)
    CUB.encodeIndex(*SectionIndex);
  else
    encoding::writeVBR8(*SectionIndex, B->Data);

  // Encode block attributes.
  encoding::writeVBR8(Block.getSize(), B->Data);
  encoding::writeVBR8(Block.getAlignment(), B->Data);
  encoding::writeVBR8(Block.getAlignmentOffset(), B->Data);
  unsigned char Bits = 0;
  Bits |= Block.isZeroFill();
  encoding::writeVBR8(Bits, B->Data);

  // For zerofill block, we can just return here because it has no edges.
  if (Block.isZeroFill())
    return get(B->build());

  // Do a simple sorting based on offset so it has stable ordering for the same
  // block.
  SmallVector<const jitlink::Edge *, 16> Edges;
  Edges.reserve(Block.edges_size());
  for (const auto &E : Block.edges())
    Edges.emplace_back(&E);
  llvm::sort(Edges, compareEdges);

  SmallVector<Fixup, 16> Fixups;
  Fixups.reserve(Edges.size());
  for (const auto *E : Edges) {
    Fixups.emplace_back();
    Fixups.back().Kind = E->getKind();
    Fixups.back().Offset = E->getOffset();
  }

  // Normalize content and put it at the end of the block.
  // FIXME: assume current block alignment is the alignment for padding and just
  // pad every block.
  Optional<Align> TrailingNopsAlignment;
  TrailingNopsAlignment = Align{Block.getAlignment()};
  StringRef Content(Block.getContent().begin(), Block.getSize());
  SmallString<1024> MutableContentStorage;
  if (Error E = helpers::canonicalizeContent(CUB.TT.getArch(), Content,
                                             MutableContentStorage, Fixups,
                                             TrailingNopsAlignment)
                    .moveInto(Content))
    return E;

  // Encode content first with size and data.
  if (UseBlockContentNode) {
    if (auto E = CUB.createAndReferenceContent(Content))
      return E;
  } else {
    encoding::writeVBR8(Content.size(), B->Data);
    B->Data.append(Content);
  }

  // Encode edges.
  if (!NestEdgesInBlock) {
    if (auto E = CUB.createAndReferenceEdges(Edges))
      return E;
    return get(B->build());
  }

  encoding::writeVBR8(Block.edges_size(), B->Data);
  for (const auto *E : Edges) {
    // Nest the Edge in block.
    if (auto Err = encodeEdge(CUB, B->Data, E))
      return Err;
  }

  return get(B->build());
}

Error BlockRef::materializeBlock(LinkGraphBuilder &LGB,
                                 unsigned BlockIdx) const {
  unsigned SectionIdx, Size, Alignment, AlignOffset;
  unsigned char Bits;

  auto Remaining = getData();
  if (EncodeIndexInCU)
    SectionIdx = LGB.nextIdxForBlock(BlockIdx);
  else if (auto E = encoding::consumeVBR8(Remaining, SectionIdx))
    return E;

  auto  E = encoding::consumeVBR8(Remaining, Size);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Alignment);
  if (!E)
    E = encoding::consumeVBR8(Remaining, AlignOffset);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Bits);

  if (E)
    return E;

  auto SectionInfo = LGB.getSectionInfo(SectionIdx);
  if (!SectionInfo)
    return SectionInfo.takeError();

  bool IsZeroFill = Bits & 1U;
  auto Address = getAlignedAddress(*SectionInfo, Size, Alignment, AlignOffset);
  if (IsZeroFill) {
    auto &B = LGB.getLinkGraph()->createZeroFillBlock(
        *SectionInfo->Section, Size, Address, Alignment, AlignOffset);
    if (auto E = LGB.addBlock(BlockIdx, &B))
      return E;

    if (!Remaining.empty())
      return createStringError(inconvertibleErrorCode(),
                               "zero fill block garbage bits");

    return Error::success();
  }

  StringRef ContentData;
  if (UseBlockContentNode) {
    auto ContentRef = LGB.getNode<BlockContentRef>(BlockIdx);
    if (!ContentRef)
      return ContentRef.takeError();
    ContentData = ContentRef->getData();
  } else {
    unsigned ContentSize;
    E = encoding::consumeVBR8(Remaining, ContentSize);
    if (E)
      return E;
    auto Data = consumeDataOfSize(Remaining, ContentSize);
    if (!Data)
      return Data.takeError();
    ContentData = *Data;
  }
  ArrayRef<char> Content(ContentData.data(), ContentData.size());
  auto &B = LGB.getLinkGraph()->createContentBlock(
      *SectionInfo->Section, Content, Address, Alignment, AlignOffset);
  if (auto E = LGB.addBlock(BlockIdx, &B))
    return E;

  // When not embedding, use Remaining to indicate if there is an EdgeList to
  // decode.
  unsigned RemainSize = NestEdgesInBlock ? Remaining.size() : 1;
  LGB.setBlockRemaining(BlockIdx, RemainSize);
  return Error::success();
}

Error BlockRef::materializeEdges(LinkGraphBuilder &LGB,
                                 unsigned BlockIdx) const {
  auto BlockInfo = LGB.getBlockInfo(BlockIdx);
  if (!BlockInfo)
    return BlockInfo.takeError();

  // Nothing remains, no edges.
  if (!BlockInfo->Remaining)
    return Error::success();

  if (!NestEdgesInBlock) {
    auto Edge = LGB.getNode<EdgeListRef>(BlockIdx);
    if (!Edge)
      return Edge.takeError();

    if (auto Err = Edge->materialize(LGB, *BlockInfo->Block, BlockIdx))
      return Err;

    return Error::success();
  }

  auto Remaining = getData().take_back(BlockInfo->Remaining);
  unsigned EdgeSize;
  auto E = encoding::consumeVBR8(Remaining, EdgeSize);
  if (E)
    return E;

  for (unsigned I = 0; I < EdgeSize; ++I) {
    if (auto Err = decodeEdge(LGB, Remaining, *BlockInfo->Block, BlockIdx))
      return Err;
  }

  return Error::success();
}

Expected<BlockRef> BlockRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return BlockRef(*Specific);
}

Expected<BlockContentRef>
BlockContentRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return BlockContentRef(*Specific);
}

Expected<BlockContentRef> BlockContentRef::create(CompileUnitBuilder &CUB,
                                                  StringRef Content) {
  Expected<Builder> B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Content);
  return get(B->build());
}

Expected<SymbolRef> SymbolRef::create(CompileUnitBuilder &CUB,
                                      const jitlink::Symbol &S) {
  Expected<Builder> B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (NameInSymbols) {
    encoding::writeVBR8(S.getName().size(), B->Data);
    B->Data.append(S.getName());
  } else if (auto E = CUB.createAndReferenceName(S.getName()))
    return E;

  // Encode attributes. FIXME: Not optimal encoding.
  encoding::writeVBR8(S.getSize(), B->Data);
  encoding::writeVBR8(S.getOffset(), B->Data);
  unsigned Bits = 0;
  Bits |= (unsigned)S.getScope();        // 2 bits
  Bits |= (unsigned)S.getLinkage() << 2; // 1 bit
  Bits |= (unsigned)S.isDefined() << 3;  // 1 bit
  Bits |= (unsigned)S.isLive() << 4;     // 1 bit
  Bits |= (unsigned)S.isCallable() << 5; // 1 bit
  Bits |= (unsigned)S.isAutoHide() << 6; // 1 bit
  encoding::writeVBR8(Bits, B->Data);

  if (S.isDefined()) {
    auto BlockIndex = CUB.getBlockIndex(S.getBlock());
    if (!BlockIndex)
      return BlockIndex.takeError();
    if (EncodeIndexInCU)
      CUB.encodeIndex(*BlockIndex);
    else
      encoding::writeVBR8(*BlockIndex, B->Data);
  }

  return get(B->build());
}

Error SymbolRef::materialize(LinkGraphBuilder &LGB, unsigned Idx) const {
  StringRef Remaining = getData();
  unsigned Size, Offset, Bits;
  StringRef Name;

  if (NameInSymbols) {
    unsigned NameSize;
    auto E = encoding::consumeVBR8(Remaining, NameSize);
    if (E)
      return E;
    auto NameStr = consumeDataOfSize(Remaining, NameSize);
    if (NameStr)
      return NameStr.takeError();
    Name = *NameStr;
  } else {
    auto NameStr = LGB.nextNode<NameRef>();
    if (!NameStr)
      return NameStr.takeError();
    Name = NameStr->getName();
  }

  auto E = encoding::consumeVBR8(Remaining, Size);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Offset);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Bits);
  if (E)
    return E;

  jitlink::Linkage Linkage = (jitlink::Linkage)((Bits & (1U << 2)) >> 2);
  bool IsDefined = Bits & (1U << 3);
  if (!IsDefined) {
    auto &Symbol = LGB.getLinkGraph()->addExternalSymbol(Name, Size, Linkage);
    if (auto E = LGB.addSymbol(Idx, &Symbol))
      return E;
    return Error::success();
  }

  jitlink::Scope Scope = (jitlink::Scope)(Bits & 3U);
  bool IsLive = Bits & (1U << 4);
  bool IsCallable = Bits & (1U << 5);

  unsigned BlockIdx;
  if (EncodeIndexInCU) {
    auto Idx = LGB.nextIdx();
    if (!Idx)
      return Idx.takeError();
    BlockIdx = *Idx;
  } else if (auto E = encoding::consumeVBR8(Remaining, BlockIdx))
    return E;

  auto BlockInfo = LGB.getBlockInfo(BlockIdx);
  if (!BlockInfo)
    return BlockInfo.takeError();

  auto &Symbol =
      Name.empty()
          ? LGB.getLinkGraph()->addAnonymousSymbol(*BlockInfo->Block, Offset, 0,
                                                   IsCallable, IsLive)
          : LGB.getLinkGraph()->addDefinedSymbol(*BlockInfo->Block, Offset,
                                                 Name, 0, Linkage, Scope,
                                                 IsCallable, IsLive);
  if (auto E = LGB.addSymbol(Idx, &Symbol))
    return E;

  return Error::success();
}

Expected<SymbolRef> SymbolRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return SymbolRef(*Specific);
}

void CompileUnitBuilder::encodeIndex(unsigned Index) {
  LocalIndexStorage.emplace_back(Index);
}

unsigned CompileUnitBuilder::recordNode(const ObjectFormatNodeRef &Ref) {
  // Try emplace the current index into map.
  auto LastIdx = IDs.size();
  auto Result = CASIDMap.try_emplace(Ref.getID(), LastIdx);
  // If insert successful, we need to store the ID into the vector.
  auto Idx = Result.second ? LastIdx : Result.first->getSecond();
  LocalIndexStorage.emplace_back(Idx);
  if (Result.second)
    IDs.emplace_back(Ref.getID());
  return Idx;
}

unsigned CompileUnitBuilder::commitNode(const ObjectFormatNodeRef &Ref) {
  // Try emplace the current index into map.
  auto LastIdx = IDs.size();
  auto Result = CASIDMap.try_emplace(Ref.getID(), LastIdx);
  // If insert successful, we need to store the ID into the vector.
  auto Idx = Result.second ? LastIdx : Result.first->getSecond();
  if (Result.second)
    IDs.emplace_back(Ref.getID());

  Indexes.emplace_back(Idx);
  // Emplace the local ids and clear the local storage.
  Indexes.insert(Indexes.end(), LocalIndexStorage.begin(),
                 LocalIndexStorage.end());
  LocalIndexStorage.clear();
  return Idx;
}

Expected<unsigned> CompileUnitBuilder::getBlockIndex(const jitlink::Block &B) {
  for (unsigned I = 0; I < Blocks.size(); ++I) {
    if (&B == Blocks[I])
      return I;
  }
  return createStringError(inconvertibleErrorCode(),
                           "Block should be in the index array");
}

Expected<unsigned>
CompileUnitBuilder::getSectionIndex(const jitlink::Section &S) {
  for (unsigned I = 0; I < Sections.size(); ++I) {
    if (&S == Sections[I])
      return I;
  }
  return createStringError(inconvertibleErrorCode(),
                           "Section should be in the index array");
}

Expected<unsigned>
CompileUnitBuilder::getSymbolIndex(const jitlink::Symbol &S) {
  for (unsigned I = 0; I < Symbols.size(); ++I) {
    if (&S == Symbols[I])
      return I;
  }
  return createStringError(inconvertibleErrorCode(),
                           "Symbol should be in the index array");
}

Error CompileUnitBuilder::createAndReferenceName(StringRef Name) {
  auto Ref = NameRef::create(*this, Name);
  if (!Ref)
    return Ref.takeError();
  // NameRef is not top level record, always record here.
  recordNode(*Ref);
  return Error::success();
}

Error CompileUnitBuilder::createAndReferenceEdges(
    ArrayRef<const jitlink::Edge *> Edges) {
  auto Ref = EdgeListRef::create(*this, Edges);
  if (!Ref)
    return Ref.takeError();
  // EdgeListRef is not top level record, always record here.
  recordNode(*Ref);
  return Error::success();
}

Error CompileUnitBuilder::createAndReferenceContent(StringRef Content) {
  auto Ref = BlockContentRef::create(*this, Content);
  if (!Ref)
    return Ref.takeError();
  // EdgeListRef is not top level record, always record here.
  recordNode(*Ref);
  return Error::success();
}

Expected<SectionRef>
CompileUnitBuilder::createSection(const jitlink::Section &S) {
  auto Ref = SectionRef::create(*this, S);
  if (!Ref)
    return Ref.takeError();
  Sections.emplace_back(&S);
  return Ref;
}

Expected<BlockRef> CompileUnitBuilder::createBlock(const jitlink::Block &B) {
  // Store the current idx. It is created in order so just add in the end.
  BlockIndexStarts.push_back(Indexes.size());
  auto Block = BlockRef::create(*this, B);
  if (!Block)
    return Block.takeError();
  return *Block;
}

Expected<SymbolRef> CompileUnitBuilder::createSymbol(const jitlink::Symbol &S) {
  auto Ref = SymbolRef::create(*this, S);
  if (!Ref)
    return Ref.takeError();
  Symbols.emplace_back(&S);
  return Ref;
}

Expected<CompileUnitRef>
CompileUnitRef::get(Expected<ObjectFormatNodeRef> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  // Parse the fields stored in the data.
  StringRef Remaining = Specific->getData();
  uint32_t PointerSize;
  uint32_t NormalizedTripleSize;
  uint8_t Endianness;
  Error E = encoding::consumeVBR8(Remaining, PointerSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, NormalizedTripleSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Endianness);
  if (E) {
    consumeError(std::move(E));
    return createStringError(inconvertibleErrorCode(),
                             "corrupt compile unit data");
  }
  if (Endianness != uint8_t(support::endianness::little) &&
      Endianness != uint8_t(support::endianness::big))
    return createStringError(inconvertibleErrorCode(),
                             "corrupt compile unit endianness");

  auto TripleStr = consumeDataOfSize(Remaining, NormalizedTripleSize);
  if (!TripleStr)
    return TripleStr.takeError();

  return CompileUnitRef(*Specific);
}

Expected<CompileUnitRef> CompileUnitRef::create(const ObjectFileSchema &Schema,
                                                const jitlink::LinkGraph &G,
                                                raw_ostream *DebugOS) {
  if (Error E = helpers::checkArch(G.getTargetTriple()))
    return std::move(E);

  CompileUnitBuilder Builder(Schema, G.getTargetTriple(), DebugOS);

  // Visit sections first since they don't have references to each other.
  // Assume section list has a stable ordering.
  for (const auto &S : G.sections()) {
    auto Ref = Builder.createSection(S);
    if (!Ref)
      return Ref.takeError();
    Builder.commitNode(*Ref);
  }
  // Visit symbols. Sort the symbols for stable ordering.
  // FIXME: duplicated code for sorting and comparsion.
  SmallVector<const jitlink::Symbol *, 16> Symbols;
  auto appendSymbols = [&](auto &&NewSymbols) {
    size_t PreviousSize = Symbols.size();
    Symbols.append(NewSymbols.begin(), NewSymbols.end());
    llvm::sort(Symbols.begin() + PreviousSize, Symbols.end(),
               helpers::compareSymbolsByAddress);
  };
  for (const jitlink::Section &Section : G.sections())
    appendSymbols(Section.symbols());
  appendSymbols(G.absolute_symbols());
  appendSymbols(G.external_symbols());

  std::stable_sort(Symbols.begin(), Symbols.end(),
                   helpers::compareSymbolsByLinkageAndSemantics);

  // Visit blocks. Create a ordered list of blocks so it can be index.
  auto appendBlocks = [&](auto &&NewBlocks) {
    size_t PreviousSize = Builder.Blocks.size();
    Builder.Blocks.append(NewBlocks.begin(), NewBlocks.end());
    llvm::sort(Builder.Blocks.begin() + PreviousSize, Builder.Blocks.end(),
               compareBlocksByAddress);
  };
  for (const jitlink::Section &Section : G.sections())
    appendBlocks(Section.blocks());

  // Create sections.
  for (auto *S : Symbols) {
    auto Ref = Builder.createSymbol(*S);
    if (!Ref)
      return Ref.takeError();
    Builder.commitNode(*Ref);
  }

  // Create BlockRefs.
  Builder.BlockIndexStarts.reserve(Builder.Blocks.size());
  for (auto *B : Builder.Blocks) {
    auto Ref = Builder.createBlock(*B);
    if (!Ref)
      return Ref.takeError();
    Builder.commitNode(*Ref);
  }

  auto B = Builder::startRootNode(Schema, KindString);
  if (!B)
    return B.takeError();

  B->IDs.insert(B->IDs.end(), Builder.IDs.begin(), Builder.IDs.end());

  std::string NormalizedTriple = G.getTargetTriple().normalize();
  encoding::writeVBR8(uint32_t(G.getPointerSize()), B->Data);
  encoding::writeVBR8(uint32_t(NormalizedTriple.size()), B->Data);
  encoding::writeVBR8(uint8_t(G.getEndianness()), B->Data);
  B->Data.append(NormalizedTriple);

  // Write size of different entities.
  encoding::writeVBR8(G.sections_size(), B->Data);
  encoding::writeVBR8(Symbols.size(), B->Data);
  encoding::writeVBR8(Builder.Blocks.size(), B->Data);
  // Write a list of all the CASID references.
  encoding::writeVBR8(Builder.Indexes.size(), B->Data);
  for (auto Idx : Builder.Indexes)
    encoding::writeVBR8(Idx, B->Data);
  for (auto Idx : Builder.BlockIndexStarts)
    encoding::writeVBR8(Idx, B->Data);

  return get(B->build());
}

Error CompileUnitRef::materialize(LinkGraphBuilder &LGB) const {
  if (auto E = forEachReference([&](cas::CASID ID) {
        LGB.IDs.emplace_back(ID);
        return Error::success();
      }))
    return E;

  // Parse the fields stored in the data.
  StringRef Remaining = getData();
  uint32_t PointerSize;
  uint32_t NormalizedTripleSize;
  uint8_t Endianness;
  Error E = encoding::consumeVBR8(Remaining, PointerSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, NormalizedTripleSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Endianness);
  if (E) {
    consumeError(std::move(E));
    return createStringError(inconvertibleErrorCode(),
                             "corrupt compile unit data");
  }
  if (Endianness != uint8_t(support::endianness::little) &&
      Endianness != uint8_t(support::endianness::big))
    return createStringError(inconvertibleErrorCode(),
                             "corrupt compile unit endianness");

  auto TripleStr = consumeDataOfSize(Remaining, NormalizedTripleSize);
  if (!TripleStr)
    return TripleStr.takeError();

  Triple TT(*TripleStr);
  unsigned IndexesSize;

  E = encoding::consumeVBR8(Remaining, LGB.SectionsSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, LGB.SymbolsSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, LGB.BlocksSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, IndexesSize);
  if (E)
    return E;

  LGB.Indexes.reserve(IndexesSize);
  for (unsigned I = 0; I < IndexesSize; ++I) {
    unsigned Idx = 0;
    auto Err = encoding::consumeVBR8(Remaining, Idx);
    if (Err)
      return Err;
    LGB.Indexes.emplace_back(Idx);
  }
  LGB.Sections.resize(LGB.SectionsSize);
  LGB.Symbols.resize(LGB.SymbolsSize);
  LGB.Blocks.resize(LGB.BlocksSize);
  for (unsigned I = 0; I < LGB.BlocksSize; ++I) {
    unsigned Idx = 0;
    auto Err = encoding::consumeVBR8(Remaining, Idx);
    if (Err)
      return Err;
    LGB.Blocks[I].BlockIdx = Idx;
  }

  if (!Remaining.empty())
    return createStringError(inconvertibleErrorCode(),
                             "corrupt ending of compile unit");

  LGB.LG.reset(new jitlink::LinkGraph(LGB.Name, TT, PointerSize,
                                      support::endianness(Endianness),
                                      LGB.GetEdgeKindName));

  return Error::success();
}

Expected<std::unique_ptr<jitlink::LinkGraph>> CompileUnitRef::createLinkGraph(
    StringRef Name,
    jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName) {
  LinkGraphBuilder Builder(Name, GetEdgeKindName, *this);
  if (auto E = Builder.materializeCompileUnit())
    return E;

  if (auto E = Builder.materializeSections())
    return E;

  if (auto E = Builder.materializeBlockContents())
    return E;

  if (auto E = Builder.materializeSymbols())
    return E;

  if (auto E = Builder.materializeEdges())
    return E;

  return Builder.takeLinkGraph();
}

Error LinkGraphBuilder::materializeCompileUnit() {
  if (auto E = Root.materialize(*this))
    return E;
  return Error::success();
}

Error LinkGraphBuilder::materializeSections() {
  for (unsigned I = 0; I < SectionsSize; ++I) {
    auto Section = nextNode<SectionRef>();
    if (!Section)
      return Section.takeError();

    if (auto E = Section->materialize(*this, I))
      return E;
  }

  return Error::success();
}

Error LinkGraphBuilder::materializeBlockContents() {
  for (unsigned I = 0; I < BlocksSize; ++I) {
    auto Block = getNode<BlockRef>(I);
    if (!Block)
      return Block.takeError();

    Blocks[I].Ref.emplace(*Block);
    if (auto E = Block->materializeBlock(*this, I))
      return E;
  }

  return Error::success();
}

Error LinkGraphBuilder::materializeSymbols() {
  for (unsigned I = 0; I < SymbolsSize; ++I) {
    auto Symbol = nextNode<SymbolRef>();
    if (!Symbol)
      return Symbol.takeError();

    if (auto E = Symbol->materialize(*this, I))
      return E;
  }

  return Error::success();
}

Error LinkGraphBuilder::materializeEdges() {
  for (unsigned I = 0; I < BlocksSize; ++I) {
    if (auto E = Blocks[I].Ref->materializeEdges(*this, I))
      return E;
  }

  return Error::success();
}

Error LinkGraphBuilder::addSection(unsigned SectionIdx, jitlink::Section *S) {
  if (SectionIdx >= Sections.size())
    return createStringError(inconvertibleErrorCode(),
                             "Section is at the wrong index");
  Sections[SectionIdx].Section = S;
  return Error::success();
}

Error LinkGraphBuilder::addBlock(unsigned BlockIdx, jitlink::Block *B) {
  if (BlockIdx >= Blocks.size())
    return createStringError(inconvertibleErrorCode(),
                             "Block is at the wrong index");
  Blocks[BlockIdx].Block = B;
  return Error::success();
}

void LinkGraphBuilder::setBlockRemaining(unsigned BlockIdx,
                                         unsigned Remaining) {
  Blocks[BlockIdx].Remaining = Remaining;
}

Expected<LinkGraphBuilder::BlockInfo &>
LinkGraphBuilder::getBlockInfo(unsigned BlockIdx) {
  if (BlockIdx >= Blocks.size())
    return createStringError(inconvertibleErrorCode(), "Block out of bound");
  return Blocks[BlockIdx];
}

Error LinkGraphBuilder::addSymbol(unsigned SymbolIdx, jitlink::Symbol *S) {
  if (SymbolIdx >= Symbols.size())
    return createStringError(inconvertibleErrorCode(),
                             "Symbol is at the wrong index");
  Symbols[SymbolIdx] = S;
  return Error::success();
}

Expected<jitlink::Symbol *> LinkGraphBuilder::getSymbol(unsigned SymbolIdx) {
  if (SymbolIdx >= Symbols.size())
    return createStringError(inconvertibleErrorCode(), "Symbol out of bound");
  return Symbols[SymbolIdx];
}

Expected<LinkGraphBuilder::SectionInfo &>
LinkGraphBuilder::getSectionInfo(unsigned SectionIdx) {
  if (SectionIdx >= Sections.size())
    return createStringError(inconvertibleErrorCode(), "Section out of bound");
  return Sections[SectionIdx];
}
