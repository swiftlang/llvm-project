//===- MC/MCCASFlatV1.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/CAS/MCCASFlatV1.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include <memory>

// FIXME: Fix dependency here.
#include "llvm/CASObjectFormats/Encoding.h"

using namespace llvm;
using namespace llvm::mccasformats;
using namespace llvm::mccasformats::flatv1;
using namespace llvm::mccasformats::reader;

using namespace llvm::casobjectformats::encoding;

constexpr StringLiteral MCAssemblerRef::KindString;
constexpr StringLiteral HeaderRef::KindString;
constexpr StringLiteral PaddingRef::KindString;
constexpr StringLiteral RelocationsRef::KindString;
constexpr StringLiteral DataInCodeRef::KindString;
constexpr StringLiteral SymbolTableRef::KindString;

#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  constexpr StringLiteral MCFragmentName##Ref::KindString;
#include "llvm/MC/CAS/MCCASFlatV1.def"

void MCSchema::anchor() {}
char MCSchema::ID = 0;

Expected<cas::NodeProxy>
MCSchema::createFromMCAssemblerImpl(MachOCASWriter &ObjectWriter,
                                    MCAssembler &Asm, const MCAsmLayout &Layout,
                                    raw_ostream *DebugOS) const {
  return MCAssemblerRef::create(*this, ObjectWriter, Asm, Layout, DebugOS);
}

Error MCSchema::serializeObjectFile(cas::NodeProxy RootNode,
                                    raw_ostream &OS) const {
  if (!isRootNode(RootNode))
    return createStringError(inconvertibleErrorCode(), "invalid root node");
  auto Asm = MCAssemblerRef::get(*this, RootNode);
  if (!Asm)
    return Asm.takeError();

  return Asm->materialize(OS);
}

MCSchema::MCSchema(cas::CASDB &CAS) : MCSchema::RTTIExtends(CAS) {
  // Fill the cache immediately to preserve thread-safety.
  if (Error E = fillCache())
    report_fatal_error(std::move(E));
}

Error MCSchema::fillCache() {
  Optional<cas::CASID> RootKindID;
  const unsigned Version = 0; // Bump this to error on old object files.
  if (Expected<cas::NodeProxy> ExpectedRootKind =
          CAS.createNode(None, "mc:flatv1:schema:" + Twine(Version).str()))
    RootKindID = *ExpectedRootKind;
  else
    return ExpectedRootKind.takeError();

  StringRef AllKindStrings[] = {
      PaddingRef::KindString,   MCAssemblerRef::KindString,
      HeaderRef::KindString,     RelocationsRef::KindString,
      DataInCodeRef::KindString, SymbolTableRef::KindString,
#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  MCFragmentName##Ref::KindString,
#include "llvm/MC/CAS/MCCASFlatV1.def"
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

  auto ExpectedTypeID = CAS.createNode(IDs, "mc:flatv1:root");
  if (!ExpectedTypeID)
    return ExpectedTypeID.takeError();
  RootNodeTypeID = *ExpectedTypeID;
  return Error::success();
}

Optional<StringRef> MCSchema::getKindString(const cas::NodeProxy &Node) const {
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

bool MCSchema::isRootNode(const cas::NodeProxy &Node) const {
  if (Node.getNumReferences() < 1)
    return false;
  return Node.getReferenceID(0) == *RootNodeTypeID;
}

bool MCSchema::isNode(const cas::NodeProxy &Node) const {
  // This is a very weak check!
  return bool(getKindString(Node));
}

Expected<MCNodeProxy::Builder>
MCNodeProxy::Builder::startRootNode(const MCSchema &Schema,
                                    StringRef KindString) {
  Builder B(Schema);
  B.IDs.push_back(Schema.getRootNodeTypeID());

  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Error MCNodeProxy::Builder::startNodeImpl(StringRef KindString) {
  Optional<unsigned char> TypeID = Schema->getKindStringID(KindString);
  if (!TypeID)
    return createStringError(inconvertibleErrorCode(),
                             "invalid mc format kind string: " + KindString);
  Data.push_back(*TypeID);
  return Error::success();
}

Expected<MCNodeProxy::Builder>
MCNodeProxy::Builder::startNode(const MCSchema &Schema, StringRef KindString) {
  Builder B(Schema);
  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Expected<MCNodeProxy> MCNodeProxy::Builder::build() {
  return MCNodeProxy::get(*Schema, Schema->CAS.createNode(IDs, Data));
}

StringRef MCNodeProxy::getKindString() const {
  Optional<StringRef> KS = getSchema().getKindString(*this);
  assert(KS && "Expected valid kind string");
  return *KS;
}

Optional<unsigned char> MCSchema::getKindStringID(StringRef KindString) const {
  for (auto &I : KindStrings)
    if (I.second == KindString)
      return I.first;
  return None;
}

Expected<MCNodeProxy> MCNodeProxy::get(const MCSchema &Schema,
                                       Expected<cas::NodeProxy> Ref) {
  if (!Ref)
    return Ref.takeError();
  if (!Schema.isNode(*Ref))
    return createStringError(inconvertibleErrorCode(),
                             "invalid kind-string for node in mc-cas-schema");
  return MCNodeProxy(Schema, *Ref);
}

static Expected<StringRef> consumeDataOfSize(StringRef &Data, unsigned Size) {
  if (Data.size() < Size)
    return createStringError(inconvertibleErrorCode(),
                             "Requested data go beyond the buffer");

  auto Ret = Data.take_front(Size);
  Data = Data.drop_front(Size);

  return Ret;
}

Expected<HeaderRef> HeaderRef::create(MCCASBuilder &MB, StringRef Name) {
  auto B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Name);
  return get(B->build());
}

Expected<HeaderRef> HeaderRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  if (Specific->getNumReferences())
    return createStringError(inconvertibleErrorCode(), "corrupt name");

  return HeaderRef(*Specific);
}

Expected<RelocationsRef> RelocationsRef::create(MCCASBuilder &MB,
                                                StringRef Data) {
  auto B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Data);
  return get(B->build());
}

Expected<RelocationsRef> RelocationsRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return RelocationsRef(*Specific);
}

Expected<DataInCodeRef> DataInCodeRef::create(MCCASBuilder &MB,
                                              StringRef Data) {
  auto B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Data);
  return get(B->build());
}

Expected<DataInCodeRef> DataInCodeRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return DataInCodeRef(*Specific);
}

Expected<SymbolTableRef> SymbolTableRef::create(MCCASBuilder &MB,
                                                StringRef Data) {
  auto B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.append(Data);
  return get(B->build());
}

Expected<SymbolTableRef> SymbolTableRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return SymbolTableRef(*Specific);
}

Expected<PaddingRef> PaddingRef::create(MCCASBuilder &MB, uint64_t Size) {
  // Fake a FT_Fill Fragment that is zero filled.
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  writeVBR8(Size, B->Data);
  return get(B->build());
}

Expected<uint64_t> PaddingRef::materialize(raw_ostream &OS) const {
  StringRef Remaining = getData();
  uint64_t Size;
  if (auto E = consumeVBR8(Remaining, Size))
    return E;
  OS.write_zeros(Size);
  return Size;
}

Expected<PaddingRef> PaddingRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return PaddingRef(*Specific);
}

Expected<MCAlignFragmentRef>
MCAlignFragmentRef::create(MCCASBuilder &MB, const MCAlignFragment &F,
                           unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  uint64_t Count = FragmentSize / F.getValueSize();
  if (F.hasEmitNops()) {
    // Write 0 as size and use backend to emit nop.
    writeVBR8(0, B->Data);
    if (!MB.Asm.getBackend().writeNopData(MB.FragmentOS, Count,
                                          F.getSubtargetInfo()))
      report_fatal_error("unable to write nop sequence of " + Twine(Count) +
                         " bytes");
    B->Data.append(MB.FragmentData);
    return get(B->build());
  }
  writeVBR8(Count, B->Data);
  writeVBR8(F.getValue(), B->Data);
  writeVBR8(F.getValueSize(), B->Data);
  return get(B->build());
}

Expected<uint64_t>
MCAlignFragmentRef::materialize(MCCASReader &Reader) const {
  uint64_t Count;
  auto Remaining = getData();
  auto Endian = Reader.isLittleEndian() ? support::little : support::big;
  if (auto E = consumeVBR8(Remaining, Count))
    return E;

  // hasEmitNops.
  if (!Count) {
    Reader.OS << Remaining;
    return Remaining.size();
  }
  int64_t Value;
  unsigned ValueSize;
  if (auto E = consumeVBR8(Remaining, Value))
    return E;
  if (auto E = consumeVBR8(Remaining, ValueSize))
    return E;

  for (uint64_t I = 0; I != Count; ++I) {
    switch (ValueSize) {
    default:
      llvm_unreachable("Invalid size!");
    case 1:
      Reader.OS << char(Value);
      break;
    case 2:
      support::endian::write<uint16_t>(Reader.OS, Value, Endian);
      break;
    case 4:
      support::endian::write<uint32_t>(Reader.OS, Value, Endian);
      break;
    case 8:
      support::endian::write<uint64_t>(Reader.OS, Value, Endian);
      break;
    }
  }
  return Count * ValueSize;
}

Expected<MCBoundaryAlignFragmentRef> MCBoundaryAlignFragmentRef::create(
    MCCASBuilder &MB, const MCBoundaryAlignFragment &F, unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  if (!MB.Asm.getBackend().writeNopData(MB.FragmentOS, FragmentSize,
                                        F.getSubtargetInfo()))
    report_fatal_error("unable to write nop sequence of " +
                       Twine(FragmentSize) + " bytes");
  B->Data.append(MB.FragmentData);
  return get(B->build());
}

Expected<uint64_t>
MCBoundaryAlignFragmentRef::materialize(MCCASReader &Reader) const {
  Reader.OS << getData();
  return getData().size();
}

Expected<MCCVInlineLineTableFragmentRef>
MCCVInlineLineTableFragmentRef::create(MCCASBuilder &MB,
                                       const MCCVInlineLineTableFragment &F,
                                       unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  B->Data.append(F.getContents());
  return get(B->build());
}

Expected<uint64_t>
MCCVInlineLineTableFragmentRef::materialize(MCCASReader &Reader) const {
  Reader.OS << getData();
  return getData().size();
}

Expected<MCDummyFragmentRef>
MCDummyFragmentRef::create(MCCASBuilder &MB, const MCDummyFragment &F,
                           unsigned FragmentSize) {
  llvm_unreachable("Should not have been added");
}

Expected<uint64_t> MCDummyFragmentRef::materialize(MCCASReader &Reader) const {
  llvm_unreachable("Should not have been added");
}

Expected<MCFillFragmentRef> MCFillFragmentRef::create(MCCASBuilder &MB,
                                                      const MCFillFragment &F,
                                                      unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  writeVBR8(FragmentSize, B->Data);
  writeVBR8(F.getValue(), B->Data);
  writeVBR8(F.getValueSize(), B->Data);
  return get(B->build());
}

Expected<uint64_t> MCFillFragmentRef::materialize(MCCASReader &Reader) const {
  StringRef Remaining = getData();
  uint64_t Size;
  uint64_t Value;
  unsigned ValueSize;
  if (auto E = consumeVBR8(Remaining, Size))
    return E;
  if (auto E = consumeVBR8(Remaining, Value))
    return E;
  if (auto E = consumeVBR8(Remaining, ValueSize))
    return E;

  // FIXME: Code duplication from writeFragment.
  const unsigned MaxChunkSize = 16;
  char Data[MaxChunkSize];
  for (unsigned I = 0; I != ValueSize; ++I) {
    unsigned Index =
        Reader.isLittleEndian() == support::little ? I : (ValueSize - I - 1);
    Data[I] = uint8_t(Value >> (Index * 8));
  }
  for (unsigned I = ValueSize; I < MaxChunkSize; ++I)
    Data[I] = Data[I - ValueSize];

  const unsigned NumPerChunk = MaxChunkSize / ValueSize;
  const unsigned ChunkSize = ValueSize * NumPerChunk;

  StringRef Ref(Data, ChunkSize);
  for (uint64_t I = 0, E = Size / ChunkSize; I != E; ++I)
    Reader.OS << Ref;

  unsigned TrailingCount = Size % ChunkSize;
  if (TrailingCount)
    Reader.OS.write(Data, TrailingCount);
  return Size;
}

Expected<MCLEBFragmentRef> MCLEBFragmentRef::create(MCCASBuilder &MB,
                                                    const MCLEBFragment &F,
                                                    unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  B->Data.append(F.getContents());
  return get(B->build());
}

Expected<uint64_t> MCLEBFragmentRef::materialize(MCCASReader &Reader) const {
  Reader.OS << getData();
  return getData().size();
}

Expected<MCNopsFragmentRef> MCNopsFragmentRef::create(MCCASBuilder &MB,
                                                    const MCNopsFragment &F,
                                                    unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  int64_t NumBytes = F.getNumBytes();
  int64_t ControlledNopLength = F.getControlledNopLength();
  int64_t MaximumNopLength =
      MB.Asm.getBackend().getMaximumNopSize(*F.getSubtargetInfo());
  if (ControlledNopLength > MaximumNopLength)
    ControlledNopLength = MaximumNopLength;
  if (!ControlledNopLength)
    ControlledNopLength = MaximumNopLength;
  while (NumBytes) {
    uint64_t NumBytesToEmit = (uint64_t)std::min(NumBytes, ControlledNopLength);
    assert(NumBytesToEmit && "try to emit empty NOP instruction");
    if (!MB.Asm.getBackend().writeNopData(MB.FragmentOS, NumBytesToEmit,
                                          F.getSubtargetInfo())) {
      report_fatal_error("unable to write nop sequence of the remaining " +
                         Twine(NumBytesToEmit) + " bytes");
      break;
    }
    NumBytes -= NumBytesToEmit;
  }
  B->Data.append(MB.FragmentData);
  return get(B->build());
}

Expected<uint64_t> MCNopsFragmentRef::materialize(MCCASReader &Reader) const {
  Reader.OS << getData();
  return getData().size();
}

Expected<MCOrgFragmentRef> MCOrgFragmentRef::create(MCCASBuilder &MB,
                                                    const MCOrgFragment &F,
                                                    unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  writeVBR8(FragmentSize, B->Data);
  writeVBR8((char)F.getValue(), B->Data);
  return get(B->build());
}

Expected<uint64_t> MCOrgFragmentRef::materialize(MCCASReader &Reader) const {
  Reader.OS << getData();
  return getData().size();
}

Expected<MCSymbolIdFragmentRef>
MCSymbolIdFragmentRef::create(MCCASBuilder &MB, const MCSymbolIdFragment &F,
                              unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();
  writeVBR8(F.getSymbol()->getIndex(), B->Data);
  return get(B->build());
}

Expected<uint64_t>
MCSymbolIdFragmentRef::materialize(MCCASReader &Reader) const {
  Reader.OS << getData();
  return getData().size();
}

#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  Expected<MCFragmentName##Ref> MCFragmentName##Ref::create(                   \
      MCCASBuilder &MB, const MCFragmentName &F, unsigned FragmentSize) {      \
    Expected<Builder> B = Builder::startNode(MB.Schema, KindString);           \
    if (!B)                                                                    \
      return B.takeError();                                                    \
    MB.Asm.writeFragmentPadding(MB.FragmentOS, F, FragmentSize);                  \
    ArrayRef<char> Contents = F.getContents();                                 \
    B->Data.append(MB.FragmentData);                                              \
    B->Data.append(Contents.begin(), Contents.end());                          \
    assert(((MB.FragmentData.empty() && Contents.empty()) ||                      \
            (MB.FragmentData.size() + Contents.size() == FragmentSize)) &&        \
           "Size should match");                                               \
    return get(B->build());                                                    \
  }                                                                            \
  Expected<uint64_t> MCFragmentName##Ref::materialize(MCCASReader &Reader)     \
      const {                                                                  \
    Reader.OS << getData();                                                    \
    return getData().size();                                                   \
  }
#define MCFRAGMENT_ENCODED_FRAGMENT_ONLY
#include "llvm/MC/CAS/MCCASFlatV1.def"

Expected<MCAssemblerRef> MCAssemblerRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return MCAssemblerRef(*Specific);
}

Error MCCASBuilder::prepare() {
  ObjectWriter.resetBuffer();
  ObjectWriter.prepareObject(Asm, Layout);
  assert(ObjectWriter.getContent().empty() &&
         "prepare stage writes no content");
  return Error::success();
}

Error MCCASBuilder::buildMachOHeader() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeMachOHeader(Asm, Layout);
  auto Header = HeaderRef::create(*this, ObjectWriter.getContent());
  if (!Header)
    return Header.takeError();

  Sections.push_back(Header->getID());
  return Error::success();
}

Error MCCASBuilder::buildFragment(const MCFragment &F) {
  auto Size = Asm.computeFragmentSize(Layout, F);
  // Don't need to encode the fragment if it doesn't contribute anything.
  if (!Size)
    return Error::success();

  switch (F.getKind()) {
#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  case MCFragment::MCEnumName: {                                               \
    const MCFragmentName &SF = cast<MCFragmentName>(F);                        \
    auto FN = MCFragmentName##Ref::create(*this, SF, Size);                    \
    if (!FN)                                                                   \
      return FN.takeError();                                                   \
    addNode(*FN);                                                              \
    return Error::success();                                                   \
  }
#include "llvm/MC/CAS/MCCASFlatV1.def"
  }
  llvm_unreachable("unknown fragment");
}

Error MCCASBuilder::buildFragments() {
  for (const MCSection &Sec : Asm) {
    if (Sec.isVirtualSection())
      continue;
    for (const MCFragment &F : Sec) {
      FragmentData.clear();
      if (auto Err = buildFragment(F))
        return Err;
    }

    uint64_t Pad = ObjectWriter.getPaddingSize(&Sec, Layout);
    auto Fill = PaddingRef::create(*this, Pad);
    if (!Fill)
      return Fill.takeError();
    addNode(*Fill);
  }
  return Error::success();
}

Error MCCASBuilder::buildRelocations() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeRelocations(Asm, Layout);
  auto Data = RelocationsRef::create(*this, ObjectWriter.getContent());
  if (!Data)
    return Data.takeError();

  Sections.push_back(Data->getID());
  return Error::success();
}

Error MCCASBuilder::buildDataInCodeRegion() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeDataInCodeRegion(Asm, Layout);
  auto Data = DataInCodeRef::create(*this, ObjectWriter.getContent());
  if (!Data)
    return Data.takeError();

  Sections.push_back(Data->getID());
  return Error::success();
}

Error MCCASBuilder::buildSymbolTable() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeSymbolTable(Asm, Layout);
  StringRef S = ObjectWriter.getContent();
  while (!S.empty()) {
    auto SplitSym = S.split('\0');
    auto Sym = SymbolTableRef::create(*this, SplitSym.first);
    if (!Sym)
      return Sym.takeError();

    Sections.push_back(Sym->getID());
    ++SymTableSize;

    S = SplitSym.second;
  }
  return Error::success();
}

void MCCASBuilder::addNode(cas::NodeProxy Node) {
  auto LastIdx = Fragments.size();
  auto Result = CASIDMap.try_emplace(Node.getID(), LastIdx);
  auto Idx = Result.second ? LastIdx : Result.first->getSecond();
  FragmentIDs.push_back(Idx);
  // If insert successful, we need to store the ID into the vector.
  if (Result.second)
    Fragments.push_back(Node.getID());
}

Expected<MCAssemblerRef> MCAssemblerRef::create(const MCSchema &Schema,
                                                MachOCASWriter &ObjectWriter,
                                                MCAssembler &Asm,
                                                const MCAsmLayout &Layout,
                                                raw_ostream *DebugOS) {
  MCCASBuilder Builder(Schema, ObjectWriter, Asm, Layout, DebugOS);

  if (auto E = Builder.prepare())
    return E;

  if (auto E = Builder.buildMachOHeader())
    return E;

  if (auto E = Builder.buildFragments())
    return E;

  ObjectWriter.writeSectionData(Asm, Layout); // For verify mode only.

  if (auto E = Builder.buildRelocations())
    return E;

  if (auto E = Builder.buildDataInCodeRegion())
    return E;

  if (auto E = Builder.buildSymbolTable())
    return E;

  auto B = Builder::startRootNode(Schema, KindString);
  if (!B)
    return B.takeError();

  // Put Header, Relocations, SymbolTable, etc. in the front.
  B->IDs.append(Builder.Sections.begin(), Builder.Sections.end());
  // Then put all Fragments.
  B->IDs.append(Builder.Fragments.begin(), Builder.Fragments.end());

  std::string NormalizedTriple = ObjectWriter.Target.normalize();
  writeVBR8(uint32_t(NormalizedTriple.size()), B->Data);
  B->Data.append(NormalizedTriple);

  writeVBR8(Builder.SymTableSize, B->Data);
  for (unsigned Idx : Builder.FragmentIDs)
    writeVBR8(Idx, B->Data);

  return get(B->build());
}

template <typename T>
static Expected<uint64_t>
materializeData(raw_ostream &OS, const MCAssemblerRef &Asm, unsigned Idx) {
  auto Ref = T::get(Asm.getSchema(), Asm.getReferenceID(Idx));
  if (!Ref)
    return Ref.takeError();
  OS << Ref->getData();
  return Ref->getData().size();
}

Error MCAssemblerRef::materialize(raw_ostream &OS) const {
  // Read the triple first.
  StringRef Remaining = getData();
  uint32_t NormalizedTripleSize;
  if (auto E = consumeVBR8(Remaining, NormalizedTripleSize))
    return E;
  auto TripleStr = consumeDataOfSize(Remaining, NormalizedTripleSize);
  if (!TripleStr)
    return TripleStr.takeError();
  Triple Target(*TripleStr);

  MCCASReader Reader(OS, Target, getSchema());
  // The first few referenced nodes are speical blocks: Header, Relocations,
  // DataInCode, SymbolTable.
  if (getNumReferences() < 4)
    return createStringError(inconvertibleErrorCode(),
                             "not enough sub-blocks in MCAssemblerRef");
  unsigned SymTableSize;
  if (auto E = consumeVBR8(Remaining, SymTableSize))
    return E;

  uint64_t Written = 0;
  // MachOHeader.
  auto HeaderSize = materializeData<HeaderRef>(OS, *this, 1);
  if (!HeaderSize)
    return HeaderSize.takeError();
  Written += *HeaderSize;

  // SectionData.
  while (!Remaining.empty()) {
    unsigned FragID;
    if (auto E = consumeVBR8(Remaining, FragID))
      return E;
    auto SizeOrErr =
        Reader.materializeFragment(getReferenceID(FragID + SymTableSize + 4));
    if (!SizeOrErr)
      return SizeOrErr.takeError();
    Written += *SizeOrErr;
  }

  // Add padding to pointer size.
  auto SectionDataPad =
      offsetToAlignment(Written, Target.isArch64Bit() ? Align(8) : Align(4));
  OS.write_zeros(SectionDataPad);

  auto RelocSizeOrErr = materializeData<RelocationsRef>(OS, *this, 2);
  if (!RelocSizeOrErr)
    return RelocSizeOrErr.takeError();

  auto DCOrErr = materializeData<DataInCodeRef>(OS, *this, 3);
  if (!DCOrErr)
    return DCOrErr.takeError();

  for (unsigned Idx = 0; Idx < SymTableSize; ++ Idx) {
    auto SymOrErr = materializeData<SymbolTableRef>(OS, *this, Idx + 4);
    if (!SymOrErr)
      return SymOrErr.takeError();
    OS.write_zeros(1);
  }

  return Error::success();
}

MCCASReader::MCCASReader(raw_ostream &OS, const Triple &Target,
                         const MCSchema &Schema)
    : OS(OS), Target(Target), Schema(Schema) {}

Expected<uint64_t> MCCASReader::materializeFragment(cas::CASID ID) {
  auto Node = MCNodeProxy::get(Schema, Schema.CAS.getNode(ID));
  if (!Node)
    return Node.takeError();

#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  if (auto F = MCFragmentName##Ref::Cast(*Node))                               \
    return F->materialize(*this);
#include "llvm/MC/CAS/MCCASFlatV1.def"
  if (auto F = PaddingRef::Cast(*Node))
    return F->materialize(OS);

  llvm_unreachable("unknown fragment");
}
