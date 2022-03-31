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
constexpr StringLiteral FragmentRef::KindString;
constexpr StringLiteral RelocationsRef::KindString;
constexpr StringLiteral DataInCodeRef::KindString;
constexpr StringLiteral SymbolTableRef::KindString;

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
      FragmentRef::KindString,   MCAssemblerRef::KindString,
      HeaderRef::KindString,     RelocationsRef::KindString,
      DataInCodeRef::KindString, SymbolTableRef::KindString,
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

Expected<FragmentRef> FragmentRef::createZeroFill(MCCASBuilder &MB,
                                                  uint64_t Size) {
  // Fake a FT_Fill Fragment that is zero filled.
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.push_back(uint8_t(MCFragment::FT_Fill));
  writeVBR8(Size, B->Data);
  writeVBR8(0, B->Data);
  writeVBR8(1, B->Data);
  return get(B->build());
}

Expected<FragmentRef> FragmentRef::create(MCCASBuilder &MB, const MCFragment &F,
                                          unsigned FragmentSize) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  B->Data.push_back(uint8_t(F.getKind()));

  SmallString<8> FragmentData;
  raw_svector_ostream FragmentOS(FragmentData);
  if (const MCEncodedFragment *EF = dyn_cast<MCEncodedFragment>(&F))
    MB.Asm.writeFragmentPadding(FragmentOS, *EF, FragmentSize);

  ArrayRef<char> Contents;
  switch (F.getKind()) {
  case MCFragment::FT_Align: {
    const MCAlignFragment &AF = cast<MCAlignFragment>(F);
    uint64_t Count = FragmentSize / AF.getValueSize();
    if (AF.hasEmitNops()) {
      // Write 0 as size and use backend to emit nop.
      writeVBR8(0, B->Data);
      if (!MB.Asm.getBackend().writeNopData(FragmentOS, Count,
                                            AF.getSubtargetInfo()))
        report_fatal_error("unable to write nop sequence of " + Twine(Count) +
                           " bytes");
      break;
    }
    writeVBR8(Count, B->Data);
    writeVBR8(AF.getValue(), B->Data);
    writeVBR8(AF.getValueSize(), B->Data);
    break;
  }
  case MCFragment::FT_Data: {
    Contents = cast<MCDataFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_Relaxable: {
    Contents = cast<MCRelaxableFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_CompactEncodedInst: {
    Contents = cast<MCCompactEncodedInstFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_Fill: {
    const MCFillFragment &FF = cast<MCFillFragment>(F);
    writeVBR8(FragmentSize, B->Data);
    writeVBR8(FF.getValue(), B->Data);
    writeVBR8(FF.getValueSize(), B->Data);
    break;
  }
  case MCFragment::FT_Nops: {
    // TODO: Use backend to write NOPs and store the full content. We can be
    // more efficient to store this information if we know the backend.
    // FIXME: Code duplication with MCAssembler::writeFragment().
    const MCNopsFragment &NF = cast<MCNopsFragment>(F);
    int64_t NumBytes = NF.getNumBytes();
    int64_t ControlledNopLength = NF.getControlledNopLength();
    int64_t MaximumNopLength =
        MB.Asm.getBackend().getMaximumNopSize(*NF.getSubtargetInfo());
    if (ControlledNopLength > MaximumNopLength)
      ControlledNopLength = MaximumNopLength;
    if (!ControlledNopLength)
      ControlledNopLength = MaximumNopLength;
    while (NumBytes) {
      uint64_t NumBytesToEmit =
          (uint64_t)std::min(NumBytes, ControlledNopLength);
      assert(NumBytesToEmit && "try to emit empty NOP instruction");
      if (!MB.Asm.getBackend().writeNopData(FragmentOS, NumBytesToEmit,
                                            NF.getSubtargetInfo())) {
        report_fatal_error("unable to write nop sequence of the remaining " +
                           Twine(NumBytesToEmit) + " bytes");
        break;
      }
      NumBytes -= NumBytesToEmit;
    }
    break;
  }
  case MCFragment::FT_LEB: {
    Contents = cast<MCLEBFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_BoundaryAlign: {
    const MCBoundaryAlignFragment &BF = cast<MCBoundaryAlignFragment>(F);
    if (!MB.Asm.getBackend().writeNopData(FragmentOS, FragmentSize,
                                          BF.getSubtargetInfo()))
      report_fatal_error("unable to write nop sequence of " +
                         Twine(FragmentSize) + " bytes");
    break;
  }
  case MCFragment::FT_SymbolId: {
    const MCSymbolIdFragment &SF = cast<MCSymbolIdFragment>(F);
    writeVBR8(SF.getSymbol()->getIndex(), B->Data);
    break;
  }
  case MCFragment::FT_Org: {
    const MCOrgFragment &OF = cast<MCOrgFragment>(F);
    writeVBR8(FragmentSize, B->Data);
    writeVBR8((char)OF.getValue(), B->Data);
    break;
  }
  case MCFragment::FT_Dwarf: {
    Contents = cast<MCDwarfLineAddrFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_DwarfFrame: {
    Contents = cast<MCDwarfCallFrameFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_CVInlineLines: {
    Contents = cast<MCCVInlineLineTableFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_CVDefRange: {
    Contents = cast<MCCVDefRangeFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_PseudoProbe: {
    Contents = cast<MCPseudoProbeAddrFragment>(F).getContents();
    break;
  }
  case MCFragment::FT_Dummy:
    llvm_unreachable("Should not have been added");
  }

  B->Data.append(FragmentData);
  B->Data.append(Contents.begin(), Contents.end());

  assert(((FragmentData.empty() && Contents.empty()) ||
          (FragmentData.size() + Contents.size() == FragmentSize)) &&
         "Size should match");

  return get(B->build());
}

Expected<uint64_t> FragmentRef::materialize(raw_ostream &OS,
                                            bool IsLittleEndian) const {
  auto FT = (MCFragment::FragmentType)getData()[0];
  auto Remaining = getData().drop_front(1);
  auto Endian = IsLittleEndian ? support::little : support::big;
  switch (FT) {
  case MCFragment::FT_Align: {
    uint64_t Count;
    if (auto E = consumeVBR8(Remaining, Count))
      return E;

    // hasEmitNops.
    if (!Count) {
      OS << Remaining;
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
        OS << char(Value);
        break;
      case 2:
        support::endian::write<uint16_t>(OS, Value, Endian);
        break;
      case 4:
        support::endian::write<uint32_t>(OS, Value, Endian);
        break;
      case 8:
        support::endian::write<uint64_t>(OS, Value, Endian);
        break;
      }
    }
    return Count * ValueSize;
  }
  case MCFragment::FT_Fill: {
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
      unsigned Index = Endian == support::little ? I : (ValueSize - I - 1);
      Data[I] = uint8_t(Value >> (Index * 8));
    }
    for (unsigned I = ValueSize; I < MaxChunkSize; ++I)
      Data[I] = Data[I - ValueSize];

    const unsigned NumPerChunk = MaxChunkSize / ValueSize;
    const unsigned ChunkSize = ValueSize * NumPerChunk;

    StringRef Ref(Data, ChunkSize);
    for (uint64_t I = 0, E = Size / ChunkSize; I != E; ++I)
      OS << Ref;

    unsigned TrailingCount = Size % ChunkSize;
    if (TrailingCount)
      OS.write(Data, TrailingCount);
    return Size;
  }
  case MCFragment::FT_SymbolId: {
    uint32_t Value;
    if (auto E = consumeVBR8(Remaining, Value))
      return E;
    support::endian::write<uint32_t>(OS, Value, Endian);
    return 4;
  }
  case MCFragment::FT_Org: {
    uint64_t Size;
    char Value;
    if (auto E = consumeVBR8(Remaining, Size))
      return E;
    if (auto E = consumeVBR8(Remaining, Value))
      return E;
    for (uint64_t I = 0, E = Size; I != E; ++I)
      OS << char(Value);
    return Size;
  }
  case MCFragment::FT_Data:
  case MCFragment::FT_Relaxable:
  case MCFragment::FT_CompactEncodedInst:
  case MCFragment::FT_Nops:
  case MCFragment::FT_LEB:
  case MCFragment::FT_BoundaryAlign:
  case MCFragment::FT_Dwarf:
  case MCFragment::FT_DwarfFrame:
  case MCFragment::FT_CVInlineLines:
  case MCFragment::FT_CVDefRange:
  case MCFragment::FT_PseudoProbe:
    OS << Remaining;
    return Remaining.size();

  case MCFragment::FT_Dummy:
    llvm_unreachable("Should not have been added");
  }

  llvm_unreachable("Unknown Fragment Type");
}

Expected<FragmentRef> FragmentRef::get(Expected<MCNodeProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return FragmentRef(*Specific);
}

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

  IDs.push_back(Header->getID());
  return Error::success();
}

Error MCCASBuilder::buildFragments() {
  for (const MCSection &Sec : Asm) {
    if (Sec.isVirtualSection())
      continue;
    for (const MCFragment &F : Sec) {
      auto Size = Asm.computeFragmentSize(Layout, F);
      // Don't need to encode the fragment if it doesn't contribute anything.
      if (!Size)
        continue;

      auto Fragment = FragmentRef::create(*this, F, Size);
      if (!Fragment)
        return Fragment.takeError();
      Fragments.push_back(Fragment->getID());
    }

    uint64_t Pad = ObjectWriter.getPaddingSize(&Sec, Layout);
    auto Fill = FragmentRef::createZeroFill(*this, Pad);
    if (!Fill)
      return Fill.takeError();
    Fragments.push_back(Fill->getID());
  }
  return Error::success();
}

Error MCCASBuilder::buildRelocations() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeRelocations(Asm, Layout);
  auto Data = RelocationsRef::create(*this, ObjectWriter.getContent());
  if (!Data)
    return Data.takeError();

  IDs.push_back(Data->getID());
  return Error::success();
}

Error MCCASBuilder::buildDataInCodeRegion() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeDataInCodeRegion(Asm, Layout);
  auto Data = DataInCodeRef::create(*this, ObjectWriter.getContent());
  if (!Data)
    return Data.takeError();

  IDs.push_back(Data->getID());
  return Error::success();
}

Error MCCASBuilder::buildSymbolTable() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeSymbolTable(Asm, Layout);
  auto Sym = SymbolTableRef::create(*this, ObjectWriter.getContent());
  if (!Sym)
    return Sym.takeError();

  IDs.push_back(Sym->getID());
  return Error::success();
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
  B->IDs.insert(B->IDs.end(), Builder.IDs.begin(), Builder.IDs.end());
  // Then put all Fragments.
  B->IDs.insert(B->IDs.end(), Builder.Fragments.begin(),
                Builder.Fragments.end());

  std::string NormalizedTriple = ObjectWriter.Target.normalize();
  writeVBR8(uint32_t(NormalizedTriple.size()), B->Data);
  B->Data.append(NormalizedTriple);

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

  // The first few referenced nodes are speical blocks: Header, Relocations,
  // DataInCode, SymbolTable.
  if (getNumReferences() < 5)
    return createStringError(inconvertibleErrorCode(),
                             "not enough sub-blocks in MCAssemblerRef");

  uint64_t Written = 0;
  // MachOHeader.
  auto HeaderSize = materializeData<HeaderRef>(OS, *this, 1);
  if (!HeaderSize)
    return HeaderSize.takeError();
  Written += *HeaderSize;

  // SectionData.
  for (unsigned Idx = 5; Idx < getNumReferences(); ++Idx) {
    auto Fragment = FragmentRef::get(getSchema(), getReferenceID(Idx));
    if (!Fragment)
      return Fragment.takeError();
    auto SizeOrErr = Fragment->materialize(OS, Target.isLittleEndian());
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

  auto SymOrErr = materializeData<SymbolTableRef>(OS, *this, 4);
  if (!SymOrErr)
    return SymOrErr.takeError();

  return Error::success();
}
