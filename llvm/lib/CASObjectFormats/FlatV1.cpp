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
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/Support/EndianStream.h"
#include <memory>

// FIXME: For cl::opt. Should thread through or delete.
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::flatv1;
using namespace llvm::casobjectformats::reader;

constexpr StringLiteral CompileUnitRef::KindString;
constexpr StringLiteral NameRef::KindString;
constexpr StringLiteral SectionRef::KindString;
constexpr StringLiteral BlockRef::KindString;
constexpr StringLiteral SymbolRef::KindString;
constexpr StringLiteral BlockContentRef::KindString;

void ObjectFileSchema::anchor() {}
char ObjectFileSchema::ID = 0;

cl::opt<bool>
    UseIndirectSymbolName("indirect-symbol-name",
                          cl::desc("Encode symbol name into its own node"),
                          cl::init(false));
cl::opt<bool> UseBlockContentNode("use-block-content",
                            cl::desc("Use block-content"),
                            cl::init(false));
cl::opt<bool> InlineSymbols("inline-symbols",
                            cl::desc("Inline symbols in CU"),
                            cl::init(false));

Expected<std::unique_ptr<CASObjectReader>>
ObjectFileSchema::createObjectReader(cas::NodeProxy RootNode) const {
  if (!isRootNode(RootNode))
    return createStringError(inconvertibleErrorCode(), "invalid root node");
  auto CU = CompileUnitRef::get(*this, RootNode);
  if (!CU)
    return CU.takeError();
  auto Reader = std::make_unique<FlatV1ObjectReader>(*CU);
  if (auto E = Reader->materializeCompileUnit())
    return std::move(E);
  return std::move(Reader);
}

Expected<cas::NodeProxy>
ObjectFileSchema::createFromLinkGraphImpl(const jitlink::LinkGraph &G,
                                          raw_ostream *DebugOS) const {
  return CompileUnitRef::create(*this, G, DebugOS);
}

ObjectFileSchema::ObjectFileSchema(cas::CASDB &CAS)
    : ObjectFileSchema::RTTIExtends(CAS) {
  // Fill the cache immediately to preserve thread-safety.
  if (Error E = fillCache())
    report_fatal_error(std::move(E));
}

Error ObjectFileSchema::fillCache() {
  Optional<cas::CASID> RootKindID;
  const unsigned Version = 0; // Bump this to error on old object files.
  if (Expected<cas::NodeProxy> ExpectedRootKind =
          CAS.createNode(None, "cas.o:flatv1:schema:" + Twine(Version).str()))
    RootKindID = *ExpectedRootKind;
  else
    return ExpectedRootKind.takeError();

  StringRef AllKindStrings[] = {
      BlockRef::KindString,  CompileUnitRef::KindString,
      NameRef::KindString,   SectionRef::KindString,
      SymbolRef::KindString, BlockContentRef::KindString,
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
ObjectFileSchema::getKindString(const cas::NodeProxy &Node) const {
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

bool ObjectFileSchema::isRootNode(const cas::NodeProxy &Node) const {
  if (Node.getNumReferences() < 1)
    return false;
  return Node.getReferenceID(0) == *RootNodeTypeID;
}

bool ObjectFileSchema::isNode(const cas::NodeProxy &Node) const {
  // This is a very weak check!
  return bool(getKindString(Node));
}

Expected<ObjectFormatNodeProxy::Builder>
ObjectFormatNodeProxy::Builder::startRootNode(const ObjectFileSchema &Schema,
                                              StringRef KindString) {
  Builder B(Schema);
  B.IDs.push_back(Schema.getRootNodeTypeID());

  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Error ObjectFormatNodeProxy::Builder::startNodeImpl(StringRef KindString) {
  Optional<unsigned char> TypeID = Schema->getKindStringID(KindString);
  if (!TypeID)
    return createStringError(inconvertibleErrorCode(),
                             "invalid object format kind string: " +
                                 KindString);
  Data.push_back(*TypeID);
  return Error::success();
}

Expected<ObjectFormatNodeProxy::Builder>
ObjectFormatNodeProxy::Builder::startNode(const ObjectFileSchema &Schema,
                                          StringRef KindString) {
  Builder B(Schema);
  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Expected<ObjectFormatNodeProxy> ObjectFormatNodeProxy::Builder::build() {
  return ObjectFormatNodeProxy::get(*Schema, Schema->CAS.createNode(IDs, Data));
}

StringRef ObjectFormatNodeProxy::getKindString() const {
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

  auto LAddr = LHS->getAddress();
  auto RAddr = RHS->getAddress();
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

Expected<ObjectFormatNodeProxy>
ObjectFormatNodeProxy::get(const ObjectFileSchema &Schema,
                           Expected<cas::NodeProxy> Ref) {
  if (!Ref)
    return Ref.takeError();
  if (!Schema.isNode(*Ref))
    return createStringError(
        inconvertibleErrorCode(),
        "invalid kind-string for node in object-file-schema");
  return ObjectFormatNodeProxy(Schema, *Ref);
}

Expected<NameRef> NameRef::create(CompileUnitBuilder &CUB, StringRef Name) {
  auto B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Name);
  return get(B->build());
}

Expected<NameRef> NameRef::get(Expected<ObjectFormatNodeProxy> Ref) {
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

  B->Data.push_back(uint8_t(data::encodeProtectionFlags(S.getMemProt())));

  B->Data.append(S.getName());

  return get(B->build());
}

Expected<CASSection> SectionRef::materialize() const {
  CASSection Info;
  Info.Prot = decodeProtectionFlags((data::SectionProtectionFlags)getData()[0]);
  Info.Name = getData().drop_front(1);
  return Info;
}

Expected<SectionRef> SectionRef::get(Expected<ObjectFormatNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return SectionRef(*Specific);
}

static Error encodeEdge(CompileUnitBuilder &CUB, SmallVectorImpl<char> &Data,
                        const jitlink::Edge *E) {

  auto SymbolIndex = CUB.getSymbolIndex(E->getTarget());
  if (!SymbolIndex)
    return SymbolIndex.takeError();

  unsigned IdxAndHasAddend = *SymbolIndex << 1 | (E->getAddend() != 0);
  CUB.encodeIndex(IdxAndHasAddend);

  if (E->getAddend() != 0)
    encoding::writeVBR8(E->getAddend(), Data);
  return Error::success();
}

static Error decodeFixup(const FlatV1ObjectReader &Reader, StringRef &Data,
                         unsigned BlockIdx, unsigned &NextOffset,
                         const data::Fixup &Fixup,
                         function_ref<Error(const CASBlockFixup &)> Callback) {
  auto ExpSymbolIdx = Reader.getBlockDataIndex(BlockIdx, NextOffset++);
  if (!ExpSymbolIdx)
    return ExpSymbolIdx.takeError();
  unsigned SymbolIdx = *ExpSymbolIdx;

  bool HasAddend = SymbolIdx & 1U;
  SymbolIdx = SymbolIdx >> 1;

  int64_t Addend = 0;
  if (HasAddend) {
    if (auto E = encoding::consumeVBR8(Data, Addend))
      return E;
  }

  auto Target = CASSymbolRef{SymbolIdx};
  CASBlockFixup CASFixup{Fixup.Offset, Fixup.Kind, Addend, std::move(Target)};
  return Callback(CASFixup);
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
  CUB.encodeIndex(*SectionIndex);

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

  // Create BlockData.
  SmallString<1024> MutableContentStorage;
  Optional<StringRef> Content;
  size_t BlockSize = Block.getSize();
  if (!Block.isZeroFill()) {
    // Normalize content and put it at the end of the block.
    // FIXME: assume current block alignment is the alignment for padding and
    // just pad every block.
    Optional<Align> TrailingNopsAlignment;
    TrailingNopsAlignment = Align{Block.getAlignment()};
    StringRef BlockContent(Block.getContent().begin(), Block.getSize());
    if (Error E = helpers::canonicalizeContent(CUB.TT.getArch(), BlockContent,
                                               MutableContentStorage, Fixups,
                                               TrailingNopsAlignment)
                      .moveInto(Content))
      return std::move(E);
    BlockSize = Content->size();
  }

  SmallString<1024> EncodeContent;
  data::BlockData::encode(BlockSize, Block.getAlignment(),
                          Block.getAlignmentOffset(), Content, Fixups,
                          EncodeContent);

  StringRef BlockData(EncodeContent);
  // Encode content first with size and data.
  if (UseBlockContentNode) {
    if (auto E = CUB.createAndReferenceContent(BlockData))
      return std::move(E);
  } else {
    encoding::writeVBR8(BlockData.size(), B->Data);
    B->Data.append(BlockData);
  }

  for (const auto *E : Edges) {
    // Nest the Edge in block.
    if (auto Err = encodeEdge(CUB, B->Data, E))
      return std::move(Err);
  }

  return get(B->build());
}

template <typename Fn>
static Error decodeBlock(const FlatV1ObjectReader &Reader, unsigned BlockIdx,
                         StringRef BlockData, Fn Callback) {
  unsigned NextOffs = 1;

  auto Remaining = BlockData;
  auto SectionIdx = Reader.getBlockDataIndex(BlockIdx, NextOffs++);
  if (!SectionIdx)
    return SectionIdx.takeError();

  StringRef ContentData;
  if (Reader.hasBlockContentNodes()) {
    auto ContentRef =
        Reader.getBlockNode<BlockContentRef>(BlockIdx, NextOffs++);
    if (!ContentRef)
      return ContentRef.takeError();
    ContentData = ContentRef->getData();
  } else {
    unsigned ContentSize;
    if (auto E = encoding::consumeVBR8(Remaining, ContentSize))
      return E;
    auto Data = consumeDataOfSize(Remaining, ContentSize);
    if (!Data)
      return Data.takeError();
    ContentData = *Data;
  }

  return Callback(*SectionIdx, NextOffs, ContentData, Remaining);
}

Expected<CASBlock> BlockRef::materializeBlock(const FlatV1ObjectReader &Reader,
                                              unsigned BlockIdx) const {
  Optional<CASBlock> Info;
  Error E =
      decodeBlock(Reader, BlockIdx, getData(),
                  [&](unsigned SectionIdx, unsigned NextOffset,
                      StringRef ContentData, StringRef Remaining) -> Error {
                    data::BlockData Block(ContentData);

                    auto SectionRef = CASSectionRef{SectionIdx};
                    Info =
                        CASBlock(Block.getSize(), Block.getAlignment(),
                                 Block.getAlignmentOffset(), Block.getContent(),
                                 std::move(SectionRef), this->getID());
                    return Error::success();
                  });
  if (E)
    return std::move(E);
  return *Info;
}

Error BlockRef::materializeFixups(
    const FlatV1ObjectReader &Reader, unsigned BlockIdx,
    function_ref<Error(const CASBlockFixup &)> Callback) const {
  return decodeBlock(
      Reader, BlockIdx, getData(),
      [&](unsigned SectionIdx, unsigned NextOffset, StringRef ContentData,
          StringRef Remaining) -> Error {
        data::BlockData Block(ContentData);

        for (const data::Fixup &Fixup : Block.getFixups()) {
          if (Error Err = decodeFixup(Reader, Remaining, BlockIdx, NextOffset,
                                      Fixup, std::move(Callback)))
            return Err;
        }
        return Error::success();
      });
}

Expected<BlockRef> BlockRef::get(Expected<ObjectFormatNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return BlockRef(*Specific);
}

Expected<BlockContentRef>
BlockContentRef::get(Expected<ObjectFormatNodeProxy> Ref) {
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

static Error encodeSymbol(CompileUnitBuilder &CUB, SmallVectorImpl<char> &Data,
                          const jitlink::Symbol &S) {
  if (!UseIndirectSymbolName) {
    encoding::writeVBR8(S.getName().size(), Data);
    Data.append(S.getName().begin(), S.getName().end());
  } else if (auto E = CUB.createAndReferenceName(S.getName()))
    return E;

  // Encode attributes. FIXME: Not optimal encoding.
  encoding::writeVBR8(S.getSize(), Data);
  encoding::writeVBR8(S.getOffset(), Data);
  unsigned Bits = 0;
  Bits |= (unsigned)S.getScope();        // 2 bits
  Bits |= (unsigned)S.getLinkage() << 2; // 1 bit
  Bits |= (unsigned)S.isDefined() << 3;  // 1 bit
  Bits |= (unsigned)S.isLive() << 4;     // 1 bit
  Bits |= (unsigned)S.isCallable() << 5; // 1 bit
  Bits |= (unsigned)S.isAutoHide() << 6; // 1 bit
  encoding::writeVBR8(Bits, Data);

  if (S.isDefined()) {
    auto BlockIndex = CUB.getBlockIndex(S.getBlock());
    if (!BlockIndex)
      return BlockIndex.takeError();
    CUB.encodeIndex(*BlockIndex);
  }
  return Error::success();
}

static Expected<CASSymbol> decodeSymbol(const FlatV1ObjectReader &Reader,
                                        StringRef &Data, unsigned Idx) {
  unsigned NextOffs = 1;

  unsigned Size, Offset, Bits;
  StringRef Name;

  if (!Reader.hasIndirectSymbolNames()) {
    unsigned NameSize;
    auto E = encoding::consumeVBR8(Data, NameSize);
    if (E)
      return std::move(E);
    auto NameStr = consumeDataOfSize(Data, NameSize);
    if (!NameStr)
      return NameStr.takeError();
    Name = *NameStr;
  } else {
    auto NameStr = Reader.getSymbolNode<NameRef>(Idx, NextOffs++);
    if (!NameStr)
      return NameStr.takeError();
    Name = NameStr->getName();
  }

  auto E = encoding::consumeVBR8(Data, Size);
  if (!E)
    E = encoding::consumeVBR8(Data, Offset);
  if (!E)
    E = encoding::consumeVBR8(Data, Bits);
  if (E)
    return std::move(E);

  jitlink::Linkage Linkage = (jitlink::Linkage)((Bits & (1U << 2)) >> 2);
  bool IsDefined = Bits & (1U << 3);
  if (!IsDefined) {
    CASSymbol Info{Name,  Offset, Linkage, jitlink::Scope::Default,
                   false, false,  false,   None};
    return Info;
  }

  jitlink::Scope Scope = (jitlink::Scope)(Bits & 3U);
  bool IsLive = Bits & (1U << 4);
  bool IsCallable = Bits & (1U << 5);
  bool IsAutoHide = Bits & (1U << 6);

  auto BlockIdx = Reader.getSymbolDataIndex(Idx, NextOffs++);
  if (!BlockIdx)
    return BlockIdx.takeError();
  auto BlockRef = CASBlockRef{*BlockIdx};
  CASSymbol Info{Name,   Offset,     Linkage,    Scope,
                 IsLive, IsCallable, IsAutoHide, std::move(BlockRef)};
  return Info;
}

Expected<SymbolRef> SymbolRef::create(CompileUnitBuilder &CUB,
                                      const jitlink::Symbol &S) {
  Expected<Builder> B = Builder::startNode(CUB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeSymbol(CUB, B->Data, S))
    return std::move(E);

  return get(B->build());
}

Expected<CASSymbol> SymbolRef::materialize(const FlatV1ObjectReader &Reader,
                                           unsigned Idx) const {
  auto Data = getData();
  return decodeSymbol(Reader, Data, Idx);
}

Expected<SymbolRef> SymbolRef::get(Expected<ObjectFormatNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return SymbolRef(*Specific);
}

void CompileUnitBuilder::encodeIndex(unsigned Index) {
  LocalIndexStorage.emplace_back(Index);
}

unsigned CompileUnitBuilder::recordNode(const ObjectFormatNodeProxy &Ref) {
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

unsigned CompileUnitBuilder::commitNode(const ObjectFormatNodeProxy &Ref) {
  // Try emplace the current index into map.
  auto LastIdx = IDs.size();
  auto Result = CASIDMap.try_emplace(Ref.getID(), LastIdx);
  // If insert successful, we need to store the ID into the vector.
  auto Idx = Result.second ? LastIdx : Result.first->getSecond();
  if (Result.second)
    IDs.emplace_back(Ref.getID());

  Indexes.emplace_back(Idx);
  pushNodes();
  return Idx;
}

void CompileUnitBuilder::pushNodes() {
  // Emplace the local ids and clear the local storage.
  Indexes.insert(Indexes.end(), LocalIndexStorage.begin(),
                 LocalIndexStorage.end());
  LocalIndexStorage.clear();
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

Error CompileUnitBuilder::createAndReferenceContent(StringRef Content) {
  auto Ref = BlockContentRef::create(*this, Content);
  if (!Ref)
    return Ref.takeError();
  // EdgeListRef is not top level record, always record here.
  recordNode(*Ref);
  return Error::success();
}

Error CompileUnitBuilder::createSection(const jitlink::Section &S) {
  auto Ref = SectionRef::create(*this, S);
  if (!Ref)
    return Ref.takeError();
  Sections.emplace_back(&S);
  commitNode(*Ref);
  return Error::success();
}

Error CompileUnitBuilder::createBlock(const jitlink::Block &B) {
  // Store the current idx. It is created in order so just add in the end.
  BlockIndexStarts.push_back(Indexes.size());
  auto Block = BlockRef::create(*this, B);
  if (!Block)
    return Block.takeError();
  commitNode(*Block);
  return Error::success();
}

Error CompileUnitBuilder::createSymbol(const jitlink::Symbol &S) {
  if (InlineSymbols) {
    if (auto E = encodeSymbol(*this, InlineBuffer, S))
      return E;
    Symbols.emplace_back(&S);
    pushNodes();
    return Error::success();
  }
  auto Ref = SymbolRef::create(*this, S);
  if (!Ref)
    return Ref.takeError();
  Symbols.emplace_back(&S);
  commitNode(*Ref);
  return Error::success();
}

Expected<CompileUnitRef>
CompileUnitRef::get(Expected<ObjectFormatNodeProxy> Ref) {
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
    if (auto E = Builder.createSection(S))
      return std::move(E);
  }
  // Visit symbols. Sort the symbols for stable ordering.
  // FIXME: duplicated code for sorting and comparsion.
  SmallVector<const jitlink::Symbol *, 16> Symbols;
  auto compareSymbol = [](const jitlink::Symbol *LHS,
                          const jitlink::Symbol *RHS) {
    return helpers::compareSymbolsByAddressAnd(
        LHS, RHS, [](const jitlink::Symbol *LHS, const jitlink::Symbol *RHS) {
          return LHS->getName().compare(RHS->getName()) < 0;
        });
  };
  auto appendSymbols = [&](auto &&NewSymbols) {
    size_t PreviousSize = Symbols.size();
    Symbols.append(NewSymbols.begin(), NewSymbols.end());
    llvm::sort(Symbols.begin() + PreviousSize, Symbols.end(), compareSymbol);
  };
  for (const jitlink::Section &Section : G.sections())
    appendSymbols(Section.symbols());
  unsigned DefinedSymbolsSize = Symbols.size();
  appendSymbols(G.absolute_symbols());
  appendSymbols(G.external_symbols());

  std::stable_sort(Symbols.begin() + DefinedSymbolsSize, Symbols.end(),
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

#ifndef NDEBUG
  for (auto *S : makeArrayRef(Symbols).slice(0, DefinedSymbolsSize))
    assert(S->isDefined());
  for (auto *S : makeArrayRef(Symbols).slice(DefinedSymbolsSize))
    assert(!S->isDefined());
#endif

  // Create sections.
  for (auto *S : Symbols) {
    if (auto E = Builder.createSymbol(*S))
      return std::move(E);
  }

  // Create BlockRefs.
  Builder.BlockIndexStarts.reserve(Builder.Blocks.size());
  for (auto *B : Builder.Blocks) {
    if (auto E = Builder.createBlock(*B))
      return std::move(E);
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

  // Write the options that are used for serialization.
  encoding::writeVBR8(bool(UseIndirectSymbolName), B->Data);
  encoding::writeVBR8(bool(UseBlockContentNode), B->Data);
  encoding::writeVBR8(bool(InlineSymbols), B->Data);

  // Write size of different entities.
  encoding::writeVBR8(G.sections_size(), B->Data);
  encoding::writeVBR8(Symbols.size(), B->Data);
  encoding::writeVBR8(Builder.Blocks.size(), B->Data);
  encoding::writeVBR8(DefinedSymbolsSize, B->Data);
  // Write a list of all the CASID references.
  encoding::writeVBR8(Builder.Indexes.size(), B->Data);
  for (auto Idx : Builder.Indexes)
    encoding::writeVBR8(Idx, B->Data);
  for (auto Idx : Builder.BlockIndexStarts)
    encoding::writeVBR8(Idx, B->Data);

  // Inlined symbols if set.
  if (InlineSymbols)
    B->Data.append(Builder.InlineBuffer);

  return get(B->build());
}

Error CompileUnitRef::materialize(FlatV1ObjectReader &Reader) const {
  if (auto E = forEachReferenceID([&](cas::CASID ID) {
        Reader.IDs.emplace_back(ID);
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

  Reader.TT = Triple(*TripleStr);

  E = encoding::consumeVBR8(Remaining, Reader.HasIndirectSymbolNames);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Reader.HasBlockContentNodes);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Reader.HasInlinedSymbols);
  if (E)
    return E;

  unsigned IndexesSize;

  E = encoding::consumeVBR8(Remaining, Reader.SectionsSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Reader.SymbolsSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Reader.BlocksSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, Reader.DefinedSymbolsSize);
  if (!E)
    E = encoding::consumeVBR8(Remaining, IndexesSize);
  if (E)
    return E;

  Reader.Indexes.reserve(IndexesSize);
  for (unsigned I = 0; I < IndexesSize; ++I) {
    unsigned Idx = 0;
    auto Err = encoding::consumeVBR8(Remaining, Idx);
    if (Err)
      return Err;
    Reader.Indexes.emplace_back(Idx);
  }
  Reader.BlockIdxs.resize(Reader.BlocksSize);
  for (unsigned I = 0; I < Reader.BlocksSize; ++I) {
    unsigned Idx = 0;
    auto Err = encoding::consumeVBR8(Remaining, Idx);
    if (Err)
      return Err;
    Reader.BlockIdxs[I] = Idx;
  }

  if (Reader.HasInlinedSymbols)
    Reader.InlineBuffer = Remaining;
  else if (!Remaining.empty())
    return createStringError(inconvertibleErrorCode(),
                             "corrupt ending of compile unit");

  return Error::success();
}

Error FlatV1ObjectReader::materializeCompileUnit() {
  if (auto E = Root.materialize(*this))
    return E;
  return Error::success();
}

Error FlatV1ObjectReader::forEachSymbol(
    function_ref<Error(CASSymbolRef, CASSymbol)> Callback) const {
  for (unsigned I = 0; I < SymbolsSize; ++I) {
    auto SymbolRef = CASSymbolRef{I};
    auto Symbol = materialize(SymbolRef);
    if (!Symbol)
      return Symbol.takeError();
    if (Error E = Callback(std::move(SymbolRef), *Symbol))
      return E;
  }

  return Error::success();
}

Expected<CASSection> FlatV1ObjectReader::materialize(CASSectionRef Ref) const {
  auto Node = this->getSectionNode(Ref.Idx);
  if (!Node)
    return Node.takeError();
  return Node->materialize();
}

Expected<CASBlock> FlatV1ObjectReader::materialize(CASBlockRef Ref) const {
  auto Node = this->getBlockNode<BlockRef>(Ref.Idx, 0);
  if (!Node)
    return Node.takeError();
  return Node->materializeBlock(*this, Ref.Idx);
}

Expected<CASSymbol> FlatV1ObjectReader::materialize(CASSymbolRef Ref) const {
  if (this->hasInlinedSymbols()) {
    // FIXME: Need efficient random index visitation for \p InlineSymbols.
    return createStringError(inconvertibleErrorCode(),
                             "'InlineSymbols' not supported");
  }
  auto Node = this->getSymbolNode<SymbolRef>(Ref.Idx, 0);
  if (!Node)
    return Node.takeError();
  return Node->materialize(*this, Ref.Idx);
}

Error FlatV1ObjectReader::materializeFixups(
    CASBlockRef Ref,
    function_ref<Error(const CASBlockFixup &)> Callback) const {
  auto Node = this->getBlockNode<BlockRef>(Ref.Idx, 0);
  if (!Node)
    return Node.takeError();
  return Node->materializeFixups(*this, Ref.Idx, std::move(Callback));
}
