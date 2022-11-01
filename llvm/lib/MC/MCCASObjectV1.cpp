//===- MC/MCCASObjectV1.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/CAS/MCCASObjectV1.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/DebugInfo/DWARF/DWARFCompileUnit.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFDataExtractor.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugAbbrev.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugLine.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/Support/BinaryStreamWriter.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include <memory>

// FIXME: Fix dependency here.
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace llvm::mccasformats;
using namespace llvm::mccasformats::v1;
using namespace llvm::mccasformats::reader;

using namespace llvm::casobjectformats::encoding;

constexpr StringLiteral MCAssemblerRef::KindString;
constexpr StringLiteral PaddingRef::KindString;

#define CASV1_SIMPLE_DATA_REF(RefName, IdentifierName)                         \
  constexpr StringLiteral RefName::KindString;
#define CASV1_SIMPLE_GROUP_REF(RefName, IdentifierName)                        \
  constexpr StringLiteral RefName::KindString;
#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  constexpr StringLiteral MCFragmentName##Ref::KindString;
#include "llvm/MC/CAS/MCCASObjectV1.def"
constexpr StringLiteral DebugInfoSectionRef::KindString;
constexpr StringLiteral DebugInfoCURef::KindString;

constexpr unsigned Dwarf4HeaderSize32Bit = 11;

void MCSchema::anchor() {}
char MCSchema::ID = 0;

cl::opt<unsigned>
    MCDataMergeThreshold("mc-cas-data-merge-threshold",
                         cl::desc("MCDataFragment merge threshold"),
                         cl::init(1024));
cl::opt<bool> SplitStringSections(
    "mc-cas-split-string-sections",
    cl::desc("Split String Sections (SymTable and DebugStr)"), cl::init(true));

enum RelEncodeLoc {
  Atom,
  Section,
  CompileUnit,
};

cl::opt<RelEncodeLoc> RelocLocation(
    "mc-cas-reloc-encode-in", cl::desc("Where to put relocation in encoding"),
    cl::values(clEnumVal(Atom, "In atom"), clEnumVal(Section, "In section"),
               clEnumVal(CompileUnit, "In compile unit")),
    cl::init(Atom));

template <> struct llvm::DenseMapInfo<llvm::dwarf::Form> {
  static llvm::dwarf::Form getEmptyKey() {
    return static_cast<llvm::dwarf::Form>(
        DenseMapInfo<uint16_t>::getEmptyKey());
  }

  static llvm::dwarf::Form getTombstoneKey() {
    return static_cast<llvm::dwarf::Form>(
        DenseMapInfo<uint16_t>::getTombstoneKey());
  }

  static unsigned getHashValue(const llvm::dwarf::Form &OVal) {
    return DenseMapInfo<uint16_t>::getHashValue(OVal);
  }

  static bool isEqual(const llvm::dwarf::Form &LHS,
                      const llvm::dwarf::Form &RHS) {
    return LHS == RHS;
  }
};

DenseMap<unsigned, cas::ObjectRef>
    MCCASBuilder::DebugStringSectionContents::MapOfStringRefs =
        DenseMap<unsigned, cas::ObjectRef>();

/// A DWARFObject implementation that can be used to dwarfdump CAS-formatted
/// debug info.
class InMemoryCASDWARFObject : public DWARFObject {
  ArrayRef<char> DebugAbbrevSection;
  bool IsLittleEndian;

public:
  InMemoryCASDWARFObject(ArrayRef<char> AbbrevContents, bool IsLittleEndian)
      : DebugAbbrevSection(AbbrevContents), IsLittleEndian(IsLittleEndian) {}
  bool isLittleEndian() const override { return IsLittleEndian; }

  StringRef getAbbrevSection() const override {
    return toStringRef(DebugAbbrevSection);
  }

  Optional<RelocAddrEntry> find(const DWARFSection &Sec,
                                uint64_t Pos) const override {
    return {};
  }

  /// This struct represents the Data in one Compile Unit. The DistinctData is
  /// the data that doesn't deduplicate and must be stored separately, the
  /// DebugInfoRefData is the data that is stored in one DebugInfoCURef cas
  /// object and will deduplicate for a link ODR function.
  struct PartitionedDebugInfoSection {
    SmallVector<char, 0> DebugInfoCURefData;
    SmallVector<char, 0> DistinctData;
    SmallVector<cas::ObjectRef, 0> DebugStringRefs;
    constexpr static std::array FormsToPartition{0};
  };

  /// Create a DwarfCompileUnit that represents the compile unit at \p CUOffset
  /// in the debug info section, and iterate over the individual DIEs to
  /// identify and separate the Forms that do not deduplicate in
  /// PartitionedDebugInfoSection::FormsToPartition and those that do
  /// deduplicate. Store both kinds of Forms in their own buffers per compile
  /// unit.
  Expected<PartitionedDebugInfoSection>
  partitionCUData(MCCASBuilder &Builder, ArrayRef<char> DebugInfoData, uint64_t AbbrevOffset,
                  uint64_t CUOffset, DWARFContext *Ctx, ArrayRef<DebugStrRef> DebugStringRefs);
};

struct CUInfo {
  size_t CUSize;
  uint32_t AbbrevOffset;
};
static Expected<CUInfo> getAndSetDebugAbbrevOffsetAndSkip(
    MutableArrayRef<char> CUData, support::endianness Endian,
    Optional<uint32_t> NewOffset = None, bool SkipData = true);
Expected<cas::ObjectProxy>
MCSchema::createFromMCAssemblerImpl(MachOCASWriter &ObjectWriter,
                                    MCAssembler &Asm, const MCAsmLayout &Layout,
                                    raw_ostream *DebugOS) const {
  return MCAssemblerRef::create(*this, ObjectWriter, Asm, Layout, DebugOS);
}

Error MCSchema::serializeObjectFile(cas::ObjectProxy RootNode,
                                    raw_ostream &OS) const {
  if (!isRootNode(RootNode))
    return createStringError(inconvertibleErrorCode(), "invalid root node");
  auto Asm = MCAssemblerRef::get(*this, RootNode.getRef());
  if (!Asm)
    return Asm.takeError();

  return Asm->materialize(OS);
}

// Helper function to load the list of references inside an ObjectProxy.
Expected<SmallVector<cas::ObjectRef>>
loadReferences(const cas::ObjectProxy &Proxy) {
  SmallVector<cas::ObjectRef> Refs;
  if (auto E = Proxy.forEachReference([&](cas::ObjectRef ID) -> Error {
        Refs.push_back(ID);
        return Error::success();
      }))
    return std::move(E);
  return Refs;
}

MCSchema::MCSchema(cas::ObjectStore &CAS) : MCSchema::RTTIExtends(CAS) {
  // Fill the cache immediately to preserve thread-safety.
  if (Error E = fillCache())
    report_fatal_error(std::move(E));
}

Error MCSchema::fillCache() {
  Optional<cas::ObjectRef> RootKindID;
  const unsigned Version = 0; // Bump this to error on old object files.
  if (Expected<cas::ObjectProxy> ExpectedRootKind =
          CAS.createProxy(None, "mc:v1:schema:" + Twine(Version).str()))
    RootKindID = ExpectedRootKind->getRef();
  else
    return ExpectedRootKind.takeError();

  StringRef AllKindStrings[] = {
      PaddingRef::KindString,
      MCAssemblerRef::KindString,
      DebugInfoSectionRef::KindString,
      DebugInfoCURef::KindString,
#define CASV1_SIMPLE_DATA_REF(RefName, IdentifierName) RefName::KindString,
#define CASV1_SIMPLE_GROUP_REF(RefName, IdentifierName) RefName::KindString,
#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  MCFragmentName##Ref::KindString,
#include "llvm/MC/CAS/MCCASObjectV1.def"
  };
  cas::ObjectRef Refs[] = {*RootKindID};
  SmallVector<cas::ObjectRef> IDs = {*RootKindID};
  for (StringRef KS : AllKindStrings) {
    auto ExpectedID = CAS.createProxy(Refs, KS);
    if (!ExpectedID)
      return ExpectedID.takeError();
    IDs.push_back(ExpectedID->getRef());
    KindStrings.push_back(std::make_pair(KindStrings.size(), KS));
    assert(KindStrings.size() < UCHAR_MAX &&
           "Ran out of bits for kind strings");
  }

  auto ExpectedTypeID = CAS.createProxy(IDs, "mc:v1:root");
  if (!ExpectedTypeID)
    return ExpectedTypeID.takeError();
  RootNodeTypeID = ExpectedTypeID->getRef();
  return Error::success();
}

Optional<StringRef>
MCSchema::getKindString(const cas::ObjectProxy &Node) const {
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

bool MCSchema::isRootNode(const cas::ObjectProxy &Node) const {
  if (Node.getNumReferences() < 1)
    return false;
  return Node.getReference(0) == *RootNodeTypeID;
}

bool MCSchema::isNode(const cas::ObjectProxy &Node) const {
  // This is a very weak check!
  return bool(getKindString(Node));
}

Expected<MCObjectProxy::Builder>
MCObjectProxy::Builder::startRootNode(const MCSchema &Schema,
                                      StringRef KindString) {
  Builder B(Schema);
  B.Refs.push_back(Schema.getRootNodeTypeID());

  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Error MCObjectProxy::Builder::startNodeImpl(StringRef KindString) {
  Optional<unsigned char> TypeID = Schema->getKindStringID(KindString);
  if (!TypeID)
    return createStringError(inconvertibleErrorCode(),
                             "invalid mc format kind string: " + KindString);
  Data.push_back(*TypeID);
  return Error::success();
}

Expected<MCObjectProxy::Builder>
MCObjectProxy::Builder::startNode(const MCSchema &Schema,
                                  StringRef KindString) {
  Builder B(Schema);
  if (Error E = B.startNodeImpl(KindString))
    return std::move(E);
  return std::move(B);
}

Expected<MCObjectProxy> MCObjectProxy::Builder::build() {
  return MCObjectProxy::get(*Schema, Schema->CAS.createProxy(Refs, Data));
}

StringRef MCObjectProxy::getKindString() const {
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

Expected<MCObjectProxy> MCObjectProxy::get(const MCSchema &Schema,
                                           Expected<cas::ObjectProxy> Ref) {
  if (!Ref)
    return Ref.takeError();
  if (!Schema.isNode(*Ref))
    return createStringError(inconvertibleErrorCode(),
                             "invalid kind-string for node in mc-cas-schema");
  return MCObjectProxy(Schema, *Ref);
}

static Expected<StringRef> consumeDataOfSize(StringRef &Data, unsigned Size) {
  if (Data.size() < Size)
    return createStringError(inconvertibleErrorCode(),
                             "Requested data go beyond the buffer");

  auto Ret = Data.take_front(Size);
  Data = Data.drop_front(Size);

  return Ret;
}

#define CASV1_SIMPLE_DATA_REF(RefName, IdentifierName)                         \
  Expected<RefName> RefName::create(MCCASBuilder &MB, StringRef Name) {        \
    auto B = Builder::startNode(MB.Schema, KindString);                        \
    if (!B)                                                                    \
      return B.takeError();                                                    \
    B->Data.append(Name);                                                      \
    return get(B->build());                                                    \
  }                                                                            \
  Expected<RefName> RefName::get(Expected<MCObjectProxy> Ref) {                \
    auto Specific = SpecificRefT::getSpecific(std::move(Ref));                 \
    if (!Specific)                                                             \
      return Specific.takeError();                                             \
    return RefName(*Specific);                                                 \
  }
#include "llvm/MC/CAS/MCCASObjectV1.def"

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
    return std::move(E);
  OS.write_zeros(Size);
  return Size;
}

Expected<PaddingRef> PaddingRef::get(Expected<MCObjectProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return PaddingRef(*Specific);
}

static void writeRelocationsAndAddends(
    ArrayRef<MachO::any_relocation_info> Rels,
    ArrayRef<MachObjectWriter::AddendsSizeAndOffset> Addends,
    SmallVectorImpl<char> &Data) {
  writeVBR8(Rels.size(), Data);
  for (unsigned I = 0; I < Rels.size(); I++) {
    // FIXME: Might be better just encode raw data?
    writeVBR8(Rels[I].r_word0, Data);
    writeVBR8(Rels[I].r_word1, Data);
  }
  writeVBR8(Addends.size(), Data);
  for (unsigned I = 0; I < Addends.size(); I++) {
    writeVBR8(Addends[I].Value, Data);
    writeVBR8(Addends[I].Offset, Data);
    writeVBR8(Addends[I].Size, Data);
  }
}

static Error decodeRelocationsAndAddends(MCCASReader &Reader, StringRef Data) {
  uint32_t Size = 0;
  if (auto E = consumeVBR8(Data, Size))
    return E;
  MachO::any_relocation_info Rel;
  for (unsigned I = 0; I < Size; I++) {
    if (auto E = consumeVBR8(Data, Rel.r_word0))
      return E;
    if (auto E = consumeVBR8(Data, Rel.r_word1))
      return E;
    Reader.Relocations.back().push_back(Rel);
  }
  if (auto E = consumeVBR8(Data, Size))
    return E;
  MachObjectWriter::AddendsSizeAndOffset Add;
  for (unsigned I = 0; I < Size; I++) {
    if (auto E = consumeVBR8(Data, Add.Value))
      return E;
    if (auto E = consumeVBR8(Data, Add.Offset))
      return E;
    if (auto E = consumeVBR8(Data, Add.Size))
      return E;
    Reader.Addends.push_back(Add);
  }
  assert(Data.empty() && "Relocations and Addends not encoded propely, still "
                         "some metadata in the block!");
  return Error::success();
}

Error MCObjectProxy::encodeReferences(ArrayRef<cas::ObjectRef> Refs,
                                      SmallVectorImpl<char> &Data,
                                      SmallVectorImpl<cas::ObjectRef> &IDs) {
  DenseMap<cas::ObjectRef, unsigned> RefMap;
  SmallVector<cas::ObjectRef> CompactRefs;
  for (const auto &ID : Refs) {
    auto I = RefMap.try_emplace(ID, CompactRefs.size());
    if (I.second)
      CompactRefs.push_back(ID);
  }

  // Guess the size of the encoding. Made an assumption that VBR8 encoding is
  // 1 byte (the minimal).
  size_t ReferenceSize = Refs.size() * sizeof(void *);
  size_t CompactSize = CompactRefs.size() * sizeof(void *) + Refs.size();
  if (ReferenceSize <= CompactSize) {
    writeVBR8(0, Data);
    IDs.append(Refs.begin(), Refs.end());
    return Error::success();
  }

  writeVBR8(Refs.size(), Data);
  for (const auto &ID : Refs) {
    auto Idx = RefMap.find(ID);
    assert(Idx != RefMap.end() && "ID must be in the map");
    writeVBR8(Idx->second, Data);
  }

  IDs.append(CompactRefs.begin(), CompactRefs.end());
  return Error::success();
}

Expected<SmallVector<cas::ObjectRef>>
MCObjectProxy::decodeReferences(const MCObjectProxy &Node,
                                StringRef &Remaining) {
  Expected<SmallVector<cas::ObjectRef>> MaybeRefs = loadReferences(Node);
  if (!MaybeRefs)
    return MaybeRefs.takeError();

  SmallVector<cas::ObjectRef> Refs = std::move(*MaybeRefs);

  unsigned Size = 0;
  if (auto E = consumeVBR8(Remaining, Size))
    return std::move(E);

  if (!Size)
    return Refs;

  SmallVector<cas::ObjectRef> CompactRefs;
  for (unsigned I = 0; I < Size; ++I) {
    unsigned Idx = 0;
    if (auto E = consumeVBR8(Remaining, Idx))
      return std::move(E);

    if (Idx >= Refs.size())
      return createStringError(inconvertibleErrorCode(), "invalid ref index");

    CompactRefs.push_back(Refs[Idx]);
  }

  return CompactRefs;
}

Expected<DebugInfoCURef> DebugInfoCURef::create(MCCASBuilder &MB,
                                                StringRef Name,
                                                ArrayRef<cas::ObjectRef> Refs) {
  auto B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Refs, B->Data, B->Refs))
    return std::move(E);

  B->Data.append(Name);
  return get(B->build());
}

Expected<DebugInfoCURef> DebugInfoCURef::get(Expected<MCObjectProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();
  return DebugInfoCURef(*Specific);
}

Expected<GroupRef> GroupRef::create(MCCASBuilder &MB,
                                    ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  return get(B->build());
}
/// Struct that represents a fully verified DebugAbbrevSectionRef object loaded
/// from the CAS.
struct LoadedDebugAbbrevSection {
  SmallVector<char, 0> AbbrevContents;
  Optional<PaddingRef> Padding;

  static Expected<LoadedDebugAbbrevSection> load(DebugAbbrevSectionRef Section);

private:
  /// Extracts the information contained in \p Proxy, which must be either an
  /// AbbrevRef or a PaddingNode.
  Error addNode(Expected<MCObjectProxy> Proxy) {
    if (!Proxy)
      return Proxy.takeError();
    if (auto AbbrevRef = DebugAbbrevRef::Cast(*Proxy)) {
      if (Padding.has_value())
        return createStringError(inconvertibleErrorCode(),
                                 "Invalid CURef after PaddingRef");
      append_range(AbbrevContents, AbbrevRef->getData());
    } else if (auto PaddingNode = PaddingRef::Cast(*Proxy)) {
      if (Padding.has_value())
        return createStringError(inconvertibleErrorCode(),
                                 "Invalid multiple PaddingRefs");
      Padding = PaddingNode;
      append_range(AbbrevContents, PaddingNode->getData());
    } else
      return createStringError(inconvertibleErrorCode(),
                               "Invalid node for Debug Info section");
    return Error::success();
  }
};

Expected<LoadedDebugAbbrevSection>
LoadedDebugAbbrevSection::load(DebugAbbrevSectionRef Section) {

  StringRef Remaining = Section.getData();
  auto Refs = MCObjectProxy::decodeReferences(Section, Remaining);
  if (!Refs)
    return Refs.takeError();

  const MCSchema &Schema = Section.getSchema();
  cas::ObjectStore &CAS = Schema.CAS;
  auto loadRef = [&](cas::ObjectRef Ref) {
    return MCObjectProxy::get(Schema, CAS.getProxy(Ref));
  };

  LoadedDebugAbbrevSection LoadedSection;
  for (auto Ref : *Refs) {
    if (auto E = LoadedSection.addNode(loadRef(Ref)))
      return std::move(E);
  }

  return LoadedSection;
}

struct LoadedDebugLineSection {
  SmallVector<uint32_t, 0> SecOffsetVals;
  static Expected<LoadedDebugLineSection> load(DebugLineSectionRef Section);

private:
  /// Extracts the information contained in \p Proxy, which must be either an
  /// StrRef or a PaddingNode.
  Error addNode(Expected<MCObjectProxy> Proxy, unsigned &LineOffset) {
    if (!Proxy)
      return Proxy.takeError();
    SecOffsetVals.push_back(LineOffset);
    if (auto LineRef = DebugLineRef::Cast(*Proxy)) {
      LineOffset += LineRef->getData().size();
    } else if (auto PadRef = PaddingRef::Cast(*Proxy)) {
      return Error::success();
    } else
      return createStringError(inconvertibleErrorCode(),
                               "Invalid node for Debug Line section");
    return Error::success();
  }
};

Expected<LoadedDebugLineSection>
LoadedDebugLineSection::load(DebugLineSectionRef Section) {

  StringRef Remaining = Section.getData();
  auto Refs = MCObjectProxy::decodeReferences(Section, Remaining);
  if (!Refs)
    return Refs.takeError();

  const MCSchema &Schema = Section.getSchema();
  cas::ObjectStore &CAS = Schema.CAS;
  auto loadRef = [&](cas::ObjectRef Ref) {
    return MCObjectProxy::get(Schema, CAS.getProxy(Ref));
  };

  LoadedDebugLineSection LoadedSection;
  // TODO: Add support for 64 bit dwarf format
  unsigned LineOffset = 0;
  // Pop the last line table, the sec_offset is always the offset of the line
  // table.
  Refs->pop_back();
  for (auto Ref : *Refs) {
    if (auto E = LoadedSection.addNode(loadRef(Ref), LineOffset))
      return std::move(E);
  }

  return LoadedSection;
}

struct LoadedDebugStringSection {
  DenseMap<cas::ObjectRef, unsigned> MapOfStringOffsets;

  static Expected<LoadedDebugStringSection> load(DebugStringSectionRef Section);

private:
  /// Extracts the information contained in \p Proxy, which must be either an
  /// StrRef or a PaddingNode.
  Error addNode(Expected<MCObjectProxy> Proxy, unsigned &StringOffset) {
    if (!Proxy)
      return Proxy.takeError();
    if (auto StrRef = DebugStrRef::Cast(*Proxy)) {
      MapOfStringOffsets.try_emplace(StrRef->getRef(), StringOffset);
      StringOffset += StrRef->getData().size() + 1;
    } else
      return createStringError(inconvertibleErrorCode(),
                               "Invalid node for Debug Info section");
    return Error::success();
  }
};

Expected<LoadedDebugStringSection>
LoadedDebugStringSection::load(DebugStringSectionRef Section) {

  StringRef Remaining = Section.getData();
  auto Refs = MCObjectProxy::decodeReferences(Section, Remaining);
  if (!Refs)
    return Refs.takeError();

  const MCSchema &Schema = Section.getSchema();
  cas::ObjectStore &CAS = Schema.CAS;
  auto loadRef = [&](cas::ObjectRef Ref) {
    return MCObjectProxy::get(Schema, CAS.getProxy(Ref));
  };

  LoadedDebugStringSection LoadedSection;
  // TODO: Add support for 64 bit dwarf format
  unsigned StringOffset = 0;
  for (auto Ref : *Refs) {
    if (auto E = LoadedSection.addNode(loadRef(Ref), StringOffset))
      return std::move(E);
  }

  return LoadedSection;
}

struct DebugInfoMaterializationSecs {
  Optional<DebugAbbrevSectionRef> AbbrevSectionRef;
  Optional<DebugLineSectionRef> LineSectionRef;
  Optional<DebugStringSectionRef> StringSectionRef;
};

Expected<DebugInfoMaterializationSecs>
findSectionRef(MCCASReader &Reader, ArrayRef<cas::ObjectRef> Refs) {
  DebugInfoMaterializationSecs DebugRefs;
  for (auto ID : Refs) {
    auto Node = Reader.getObjectProxy(ID);
    if (!Node)
      return Node.takeError();
    if (auto AbbrevSectionRef = DebugAbbrevSectionRef::Cast(*Node))
      DebugRefs.AbbrevSectionRef = AbbrevSectionRef;
    else if (auto LineSectionRef = DebugLineSectionRef::Cast(*Node))
      DebugRefs.LineSectionRef = LineSectionRef;
    else if (auto StringSectionRef = DebugStringSectionRef::Cast(*Node))
      DebugRefs.StringSectionRef = StringSectionRef;
  }
  if (!DebugRefs.AbbrevSectionRef)
    return createStringError(inconvertibleErrorCode(),
                             "Could not find a DebugAbbrevSectionRef");
  if (!DebugRefs.LineSectionRef)
    return createStringError(inconvertibleErrorCode(),
                             "Could not find a DebugLineSectionRef");
  if (!DebugRefs.StringSectionRef)
    return createStringError(inconvertibleErrorCode(),
                             "Could not find a DebugStringSectionRef");
  return DebugRefs;
}

/// This function materializes the DebugInfoSectionRef by iterating over all the
/// sections and getting the contents of the debug abbreviaion section which is
/// passed on to the materialize method belonging to a DebugInfoSectionRef. It
/// returns the number of bytes written to the output or an error.
static Expected<uint64_t>
materializeDebugInfoSection(MCCASReader &Reader, ArrayRef<cas::ObjectRef> Refs,
                            DebugInfoSectionRef &DebugInfoRef) {
  auto DebugRefs = findSectionRef(Reader, Refs);
  if (!DebugRefs)
    return DebugRefs.takeError();
  auto LoadedAbbrev =
      LoadedDebugAbbrevSection::load(*DebugRefs->AbbrevSectionRef);
  if (!LoadedAbbrev)
    return LoadedAbbrev.takeError();
  auto LoadedString =
      LoadedDebugStringSection::load(*DebugRefs->StringSectionRef);
  if (!LoadedString)
    return LoadedString.takeError();

  auto LoadedLine = LoadedDebugLineSection::load(*DebugRefs->LineSectionRef);
  if (!LoadedLine)
    return LoadedLine.takeError();

  return DebugInfoRef.materialize(Reader, LoadedAbbrev->AbbrevContents,
                                  LoadedLine->SecOffsetVals,
                                  LoadedString->MapOfStringOffsets, &Reader.OS);
}

Expected<uint64_t> GroupRef::materialize(MCCASReader &Reader,
                                         raw_ostream *Stream) const {
  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto Node = Reader.getObjectProxy(ID);
    if (!Node)
      return Node.takeError();
    if (auto F = DebugInfoSectionRef::Cast(*Node)) {
      auto FragmentSize =
          materializeDebugInfoSection(Reader, makeArrayRef(*Refs), *F);
      if (!FragmentSize)
        return FragmentSize.takeError();
      Size += *FragmentSize;
    } else {
      auto FragmentSize = Reader.materializeGroup(ID);
      if (!FragmentSize)
        return FragmentSize.takeError();
      Size += *FragmentSize;
    }
  }

  if (!Remaining.empty())
    return createStringError(inconvertibleErrorCode(),
                             "Group should not have relocations");

  return Size;
}

Expected<SymbolTableRef>
SymbolTableRef::create(MCCASBuilder &MB, ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  return get(B->build());
}

Expected<uint64_t> SymbolTableRef::materialize(MCCASReader &Reader,
                                               raw_ostream *Stream) const {
  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto FragmentSize = Reader.materializeGroup(ID);
    if (!FragmentSize)
      return FragmentSize.takeError();
    Size += *FragmentSize;
  }

  return Size;
}

Expected<SectionRef> SectionRef::create(MCCASBuilder &MB,
                                        ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  writeRelocationsAndAddends(MB.getSectionRelocs(), MB.getSectionAddends(),
                             B->Data);

  return get(B->build());
}

Expected<DebugInfoSectionRef>
DebugInfoSectionRef::create(MCCASBuilder &MB,
                            ArrayRef<cas::ObjectRef> ChildrenNode) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  append_range(B->Refs, ChildrenNode);
  writeRelocationsAndAddends(MB.getSectionRelocs(), MB.getSectionAddends(),
                             B->Data);
  return get(B->build());
}

Expected<DebugAbbrevSectionRef>
DebugAbbrevSectionRef::create(MCCASBuilder &MB,
                              ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  writeRelocationsAndAddends(MB.getSectionRelocs(), MB.getSectionAddends(),
                             B->Data);
  return get(B->build());
}

Expected<DebugLineSectionRef>
DebugLineSectionRef::create(MCCASBuilder &MB,
                            ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  writeRelocationsAndAddends(MB.getSectionRelocs(), MB.getSectionAddends(),
                             B->Data);
  return get(B->build());
}

Expected<DebugStringSectionRef>
DebugStringSectionRef::create(MCCASBuilder &MB,
                              ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  writeRelocationsAndAddends(MB.getSectionRelocs(), MB.getSectionAddends(),
                             B->Data);
  return get(B->build());
}

/// Class that represents a fully verified DebugInfoSectionRef object loaded
/// from the CAS.
struct LoadedDebugInfoSection {
  StringRef DistinctData;
  SmallVector<size_t, 0> AbbrevOffsets;
  SmallVector<SmallVector<cas::ObjectRef>> DebugStringRefs;
  SmallVector<StringRef, 0> CUData;
  Optional<PaddingRef> Padding;
  StringRef RelocationData;

  /// Returns a range of (AbbrevOffset, CUData) pairs.
  auto getOffsetAndCUDataRange() {
    assert(CUData.size() == AbbrevOffsets.size());
    return llvm::zip(AbbrevOffsets, CUData);
  }

  static Expected<LoadedDebugInfoSection> load(DebugInfoSectionRef Section);

private:
  /// Extracts the abbreviation offsets from `OffsetsProxy` and stores them
  /// internally.
  Error fillAbbrevOffsets(Expected<MCObjectProxy> OffsetsProxy) {
    if (!OffsetsProxy)
      return OffsetsProxy.takeError();
    auto OffsetsRef = DebugAbbrevOffsetsRef::Cast(*OffsetsProxy);
    if (!OffsetsRef)
      return createStringError(inconvertibleErrorCode(),
                               "Missing abbreviation offsets node");

    DebugAbbrevOffsetsRefAdaptor OffsetsAdaptor(*OffsetsRef);
    Expected<SmallVector<size_t>> MaybeAbbrevOffsets =
        OffsetsAdaptor.decodeOffsets();
    if (!MaybeAbbrevOffsets)
      return MaybeAbbrevOffsets.takeError();
    AbbrevOffsets = std::move(*MaybeAbbrevOffsets);
    return Error::success();
  }

  /// Extracts the information contained in Proxy, which must be either a CURef
  /// or a PaddingNode.
  Error addNode(Expected<MCObjectProxy> Proxy) {
    if (!Proxy)
      return Proxy.takeError();
    if (auto DistinctDataRef = DebugInfoDistinctDataRef::Cast(*Proxy)) {
      if (CUData.size())
        return createStringError(inconvertibleErrorCode(),
                                 "Invalid DistinctDataRef after CURef");
      DistinctData = DistinctDataRef->getData();
    } else if (auto CURef = DebugInfoCURef::Cast(*Proxy)) {
      if (Padding.has_value())
        return createStringError(inconvertibleErrorCode(),
                                 "Invalid CURef after PaddingRef");

      StringRef Remaining = CURef->getData();
      auto DebugStrRefs = MCObjectProxy::decodeReferences(*Proxy, Remaining);
      if (!DebugStrRefs)
        return DebugStrRefs.takeError();
      DebugStringRefs.resize(DebugStringRefs.size() + 1);
      DebugStringRefs.back().append(*DebugStrRefs);
      CUData.push_back(Remaining);
    } else if (auto PaddingNode = PaddingRef::Cast(*Proxy)) {
      if (Padding.has_value())
        return createStringError(inconvertibleErrorCode(),
                                 "Invalid multiple PaddingRefs");
      Padding = PaddingNode;
    } else
      return createStringError(inconvertibleErrorCode(),
                               "Invalid node for Debug Info section");
    return Error::success();
  }

  /// Ensures there are as many offsets as CU Refs.
  Error checkSizes() {
    if (AbbrevOffsets.size() == CUData.size())
      return Error::success();
    return createStringError(inconvertibleErrorCode(),
                             "Expected as many abbreviation offsets as CURefs");
  }
};

Expected<LoadedDebugInfoSection>
LoadedDebugInfoSection::load(DebugInfoSectionRef Section) {
  Expected<SmallVector<cas::ObjectRef>> MaybeRefs = loadReferences(Section);
  if (!MaybeRefs)
    return MaybeRefs.takeError();

  auto Refs = makeArrayRef(*MaybeRefs);
  if (Refs.empty())
    return createStringError(inconvertibleErrorCode(),
                             "DebugInfoSection must have children nodes");

  const MCSchema &Schema = Section.getSchema();
  cas::ObjectStore &CAS = Schema.CAS;
  auto loadRef = [&](cas::ObjectRef Ref) {
    return MCObjectProxy::get(Schema, CAS.getProxy(Ref));
  };

  LoadedDebugInfoSection LoadedSection;
  if (auto E = LoadedSection.addNode(loadRef(Refs.front())))
    return std::move(E);

  Refs = Refs.drop_front();
  if (auto E = LoadedSection.fillAbbrevOffsets(loadRef(Refs.front())))
    return std::move(E);

  for (auto Ref : Refs.drop_front())
    if (auto E = LoadedSection.addNode(loadRef(Ref)))
      return std::move(E);

  if (auto E = LoadedSection.checkSizes())
    return std::move(E);

  LoadedSection.RelocationData = Section.getData();
  return LoadedSection;
}

static Error applyAddends(MCCASReader &Reader,
                          MutableArrayRef<char> SectionContents) {
  for (unsigned I = 0; I < Reader.Addends.size(); I++) {
    auto Addend = Reader.Addends[I];
    for (unsigned J = 0; J < Addend.Size; J++)
      SectionContents[Addend.Offset + J] = uint8_t(Addend.Value >> (J * 8));
  }
  return Error::success();
}

static Expected<uint64_t> getFormSize(dwarf::Form FormVal, dwarf::FormParams FP,
                                      StringRef CUData, uint64_t CUOffset,
                                      bool IsLittleEndian,
                                      uint8_t AddressSize) {
  uint64_t FormSize = 0;
  bool Indirect = false;
  Error Err = Error::success();
  do {
    Indirect = false;
    switch (FormVal) {
    case dwarf::DW_FORM_addr:
    case dwarf::DW_FORM_ref_addr: {
      FormSize += (FormVal == dwarf::DW_FORM_addr) ? FP.AddrSize
                                                   : FP.getRefAddrByteSize();
      break;
    }
    case dwarf::DW_FORM_exprloc:
    case dwarf::DW_FORM_block: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      CUOffset += DWARFExtractor.getULEB128(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_block1: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      CUOffset += DWARFExtractor.getU8(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_block2: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      CUOffset += DWARFExtractor.getU16(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_block4: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      CUOffset += DWARFExtractor.getU32(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_implicit_const:
    case dwarf::DW_FORM_flag_present: {
      FormSize += 0;
      break;
    }
    case dwarf::DW_FORM_data1:
    case dwarf::DW_FORM_ref1:
    case dwarf::DW_FORM_flag:
    case dwarf::DW_FORM_strx1:
    case dwarf::DW_FORM_addrx1: {
      FormSize += 1;
      break;
    }
    case dwarf::DW_FORM_data2:
    case dwarf::DW_FORM_ref2:
    case dwarf::DW_FORM_strx2:
    case dwarf::DW_FORM_addrx2: {
      FormSize += 2;
      break;
    }
    case dwarf::DW_FORM_strx3: {
      FormSize += 3;
      break;
    }
    case dwarf::DW_FORM_data4:
    case dwarf::DW_FORM_ref4:
    case dwarf::DW_FORM_ref_sup4:
    case dwarf::DW_FORM_strx4:
    case dwarf::DW_FORM_addrx4: {
      FormSize += 4;
      break;
    }
    case dwarf::DW_FORM_ref_sig8:
    case dwarf::DW_FORM_data8:
    case dwarf::DW_FORM_ref8:
    case dwarf::DW_FORM_ref_sup8: {
      FormSize += 8;
      break;
    }
    case dwarf::DW_FORM_data16: {
      FormSize += 16;
      break;
    }
    case dwarf::DW_FORM_sdata: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      DWARFExtractor.getSLEB128(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_udata:
    case dwarf::DW_FORM_ref_udata:
    case dwarf::DW_FORM_rnglistx:
    case dwarf::DW_FORM_loclistx:
    case dwarf::DW_FORM_GNU_addr_index:
    case dwarf::DW_FORM_GNU_str_index:
    case dwarf::DW_FORM_addrx:
    case dwarf::DW_FORM_strx: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      DWARFExtractor.getULEB128(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_LLVM_addrx_offset: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      DWARFExtractor.getULEB128(&CUOffset, &Err);
      FormSize += CUOffset - PrevOffset + 4;
      break;
    }
    case dwarf::DW_FORM_string: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      auto CurrOffset = CUOffset;
      DWARFExtractor.getCStr(&CUOffset, &Err);
      FormSize += CUOffset - CurrOffset;
      break;
    }
    case dwarf::DW_FORM_indirect: {
      DWARFDataExtractor DWARFExtractor(CUData, IsLittleEndian, AddressSize);
      uint64_t PrevOffset = CUOffset;
      FormVal =
          static_cast<dwarf::Form>(DWARFExtractor.getULEB128(&CUOffset, &Err));
      Indirect = true;
      FormSize += CUOffset - PrevOffset;
      break;
    }
    case dwarf::DW_FORM_strp:
    case dwarf::DW_FORM_sec_offset:
    case dwarf::DW_FORM_GNU_ref_alt:
    case dwarf::DW_FORM_GNU_strp_alt:
    case dwarf::DW_FORM_line_strp:
    case dwarf::DW_FORM_strp_sup: {
      FormSize += FP.getDwarfOffsetByteSize();
      break;
    }
    case dwarf::DW_FORM_addrx3:
    case dwarf::DW_FORM_lo_user: {
      llvm_unreachable("usupported form");
      break;
    }
    }
  } while (Indirect && !Err);

  if (Err)
    return std::move(Err);

  return FormSize;
}

static Error
materializeCUDie(DWARFCompileUnit &DCU, MutableArrayRef<char> SectionContents,
                 StringRef CUData, ArrayRef<char> DistinctDataArrayRef, DenseMap<cas::ObjectRef, unsigned> &MapOfStringOffsets, ArrayRef<cas::ObjectRef> DebugStringRefsVec,
                 ArrayRef<uint32_t> SecOffsetVals, unsigned &SecOffsetIndex,
                 uint64_t &CUOffset, uint64_t &DistinctDataOffset,
                 uint64_t &SectionOffset, unsigned &DebugStringIndex) {
  // Copy Abbrev Tag.
  DWARFDataExtractor DWARFExtractor(CUData, DCU.isLittleEndian(),
                                    DCU.getAddressByteSize());
  uint64_t PrevOffset = CUOffset;
  Error Err = Error::success();
  uint64_t AbbrevTag = DWARFExtractor.getULEB128(&CUOffset, &Err);
  if (Err)
    return Err;
  std::memcpy(&SectionContents[SectionOffset], CUData.data() + PrevOffset,
              CUOffset - PrevOffset);
  SectionOffset += CUOffset - PrevOffset;

  // Return if the abbrev tag is 0, this indicates the end of a sequence of
  // children DWARFDies
  if (AbbrevTag == 0)
    return Error::success();

  // Create a empty DWARFDie to extract the attributes.
  DWARFDebugInfoEntry DDIE;
  auto *AbbrevDecl =
      DCU.getAbbreviations()->getAbbreviationDeclaration(AbbrevTag);
  assert(AbbrevDecl && "AbbrevDecl not found!");
  DDIE.setAbbrevDecl(AbbrevDecl);
  DWARFDie CUDie(&DCU, &DDIE);

  // Loop over all attributes in a compile unit die, read the data from the
  // DistinctDataArrayRef or read it from the CUData, depending on the form,
  // write this to a final buffer that represents the compile unit in an object
  // file.
  for (unsigned I = 0; I < AbbrevDecl->getNumAttributes(); I++) {
    auto Form = AbbrevDecl->getFormByIndex(I);
    auto *U = CUDie.getDwarfUnit();
    dwarf::FormParams FP = U->getFormParams();
    bool FormInDistinctDataRef = is_contained(
        InMemoryCASDWARFObject::PartitionedDebugInfoSection::FormsToPartition,
        Form);
    auto FormSize = getFormSize(
        Form, FP,
        FormInDistinctDataRef ? toStringRef(DistinctDataArrayRef) : CUData,
        FormInDistinctDataRef ? DistinctDataOffset : CUOffset,
        DCU.isLittleEndian(), DCU.getAddressByteSize());

    if (!FormSize)
      return FormSize.takeError();
    // Some forms can have the size zero, such as DW_FORM_flag_present, skip the
    // iteration if this is the case.
    if (!*FormSize)
      continue;
    bool DidOverflow = false;
    if (FormInDistinctDataRef ||
        AbbrevDecl->getAttrByIndex(I) == dwarf::DW_AT_name ||
        AbbrevDecl->getAttrByIndex(I) == dwarf::DW_AT_linkage_name) {
      auto Value = SaturatingAdd(*FormSize, DistinctDataOffset, &DidOverflow);
      if (DidOverflow)
        return createStringError(
            inconvertibleErrorCode(),
            "Overflow when adding FormSize and DistinctDataOffset");
      if (Value > DistinctDataArrayRef.size())
        return createStringError(
            inconvertibleErrorCode(),
            "Trying to read out of bounds from DistinctDataArrayRef");
      memcpy(&SectionContents[SectionOffset],
             &DistinctDataArrayRef[DistinctDataOffset], *FormSize);
      DistinctDataOffset += *FormSize;
    } else if (AbbrevDecl->getAttrByIndex(I) == llvm::dwarf::DW_AT_stmt_list) {
      memcpy(&SectionContents[SectionOffset], &SecOffsetVals[SecOffsetIndex],
             *FormSize);
      assert(SecOffsetVals.size() != 0 &&
             "SecOffset SmallVector should have a non-zero length!");
      // If the size of the SecOffsetsVals vector is 1 then we only have one
      // line table, so its offset is the only one we should use for all
      // sec_offsets.
      if (SecOffsetVals.size() > 1)
        SecOffsetIndex++;
    } else if (Form == dwarf::DW_FORM_strp) {
      auto StringOffsetIt =
          MapOfStringOffsets.find(DebugStringRefsVec[DebugStringIndex]);
      if (StringOffsetIt == MapOfStringOffsets.end())
        return createStringError(inconvertibleErrorCode(),
                                 "Could not find DebugStrRef offset in map");
      // TODO: Add support for 64 bit DWARF
      memcpy(&SectionContents[SectionOffset], &StringOffsetIt->second, 4);
      DebugStringIndex++;
    } else {
      auto Value = SaturatingAdd(*FormSize, CUOffset, &DidOverflow);
      if (DidOverflow)
        return createStringError(inconvertibleErrorCode(),
                                 "Overflow when adding FormSize and CUOffset");
      if (Value > CUData.size())
        return createStringError(inconvertibleErrorCode(),
                                 "Trying to read out of bounds from CUData");
      memcpy(&SectionContents[SectionOffset], &CUData.data()[CUOffset],
             *FormSize);
      CUOffset += *FormSize;
    }
    SectionOffset += *FormSize;
  }
  return Error::success();
}

Expected<uint64_t> DebugInfoSectionRef::materialize(
    MCCASReader &Reader, ArrayRef<char> AbbrevSectionContents,
    ArrayRef<uint32_t> SecOffsetVals,
    DenseMap<cas::ObjectRef, unsigned> &MapOfStringOffsets,
    raw_ostream *Stream) const {
  // Start a new section for relocations.
  Reader.Relocations.emplace_back();
  auto LoadedSection = LoadedDebugInfoSection::load(*this);
  if (!LoadedSection)
    return LoadedSection.takeError();

  SmallVector<char, 0> SectionContents;
  raw_svector_ostream SectionStream(SectionContents);
  unsigned Size = 0;
  SmallVector<SmallVector<char>> CUContents;
  for (auto [AbbrevOffset, CUData] : LoadedSection->getOffsetAndCUDataRange()) {
    // Copy the data so that we can modify the abbrev offset prior to printing.
    // FIXME: do this with a zero-copy strategy.
    auto MutableCUData = to_vector(CUData);
    if (auto E = getAndSetDebugAbbrevOffsetAndSkip(
            MutableCUData, Reader.getEndian(), AbbrevOffset,
            /* SkipData */ false);
        !E)
      return E.takeError();
    CUContents.push_back(MutableCUData);
  }

  InMemoryCASDWARFObject CASObj(AbbrevSectionContents,
                                Reader.getEndian() == support::little);
  auto DWARFObj = std::make_unique<InMemoryCASDWARFObject>(CASObj);
  auto DWARFContextHolder = std::make_unique<DWARFContext>(std::move(DWARFObj));
  auto *DWARFCtx = DWARFContextHolder.get();
  DWARFDebugAbbrev Abbrev;
  Abbrev.extract(DataExtractor(toStringRef(AbbrevSectionContents),
                               Reader.getEndian() == support::little, 8));
  uint64_t DistinctDataOffset = 0;
  unsigned SecOffsetIndex = 0;
  assert(CUContents.size() == LoadedSection->DebugStringRefs.size() &&
         "Number of Compile Units and number of DebugStringRef vectors must be "
         "the same!");
  for (auto [CUData, DebugStringRefVec] :
       llvm::zip(CUContents, LoadedSection->DebugStringRefs)) {

    uint64_t OffsetPtr = 0;
    DWARFUnitHeader Header;
    SmallVector<char> SectionData;
    uint64_t CUOffset = 0;
    uint64_t SectionOffset = 0;

    // Copy 11 bytes which represents the 32-bit DWARF Header for DWARF4.
    if (CUData.size() < Dwarf4HeaderSize32Bit)
      return createStringError(
          inconvertibleErrorCode(),
          "CUData is too small, it doesn't even contain a 32-bit DWARF Header");
    SectionData.resize(Dwarf4HeaderSize32Bit);
    memcpy(SectionData.data(), &CUData[0], Dwarf4HeaderSize32Bit);
    SectionOffset += Dwarf4HeaderSize32Bit;
    CUOffset += Dwarf4HeaderSize32Bit;

    DWARFDataExtractor DWARFExtractor(toStringRef(SectionData),
                                      Reader.getEndian() == support::little, 8);
    uint64_t Offset = 0;
    DWARFDataExtractor::Cursor C(Offset);
    auto HeaderLength = DWARFExtractor.getRelocatedValue(C, 4);
    auto DwarfVersion = DWARFExtractor.getRelocatedValue(C, 2);

    // TODO: Add support for DWARF 5
    if (DwarfVersion != 4)
      return createStringError(inconvertibleErrorCode(),
                               "Only DWARF 4 is supported right now");

    // TODO: Add support for 64-bit DWARF Format
    if (HeaderLength == 0xffffffff)
      return createStringError(inconvertibleErrorCode(),
                               "64-bit DWARF Format not yet supported");
    if (HeaderLength < 0xfffffff0)
      // Add 4 bytes to the size of the SectionData because the DWARF Header
      // length doesn't count the 4 bytes it takes to store itself.
      SectionData.resize(HeaderLength + 4);
    else
      llvm_unreachable("Unknown DWARF Format");
    if (!C)
      return C.takeError();

    DWARFSection Section = {toStringRef(SectionData), 0 /*Address*/};
    Header.extract(*DWARFCtx,
                   DWARFDataExtractor(CASObj, Section,
                                      Reader.getEndian() == support::little, 8),
                   &OffsetPtr, DWARFSectionKind::DW_SECT_INFO);
    DWARFUnitVector UV;
    DWARFCompileUnit DCU(*DWARFCtx, Section, Header, &Abbrev,
                         &CASObj.getRangesSection(), &CASObj.getLocSection(),
                         CASObj.getStrSection(), CASObj.getStrOffsetsSection(),
                         &CASObj.getAddrSection(), CASObj.getLocSection(),
                         Reader.getEndian() == support::little, false, UV);
    
    unsigned DebugStringIndex = 0;
    while (SectionOffset < SectionData.size()) {
      auto Err = materializeCUDie(
          DCU, SectionData, toStringRef(CUData),
          arrayRefFromStringRef<char>(LoadedSection->DistinctData), MapOfStringOffsets, DebugStringRefVec, 
          SecOffsetVals, SecOffsetIndex, CUOffset, DistinctDataOffset,
          SectionOffset, DebugStringIndex);
      if (Err)
        return std::move(Err);
    }
    SectionContents.append(SectionData.begin(), SectionData.end());
    Size += SectionData.size();
  }

  assert(DistinctDataOffset == LoadedSection->DistinctData.size() &&
         "Mismatched materialization of the CAS");

  if (Optional<PaddingRef> Padding = LoadedSection->Padding) {
    auto PaddingSize = Padding->materialize(SectionStream);
    if (!PaddingSize)
      return PaddingSize.takeError();
    Size += *PaddingSize;
  }

  if (auto E =
          decodeRelocationsAndAddends(Reader, LoadedSection->RelocationData))
    return std::move(E);

  if (auto E = applyAddends(Reader, SectionContents))
    return std::move(E);

  Reader.Addends.clear();
  *Stream << SectionContents;

  return Size;
}

Expected<uint64_t> SectionRef::materialize(MCCASReader &Reader,
                                           raw_ostream *Stream) const {
  // Start a new section for relocations.
  Reader.Relocations.emplace_back();
  SmallVector<char, 0> SectionContents;
  raw_svector_ostream SectionStream(SectionContents);

  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto FragmentSize = Reader.materializeSection(ID, &SectionStream);
    if (!FragmentSize)
      return FragmentSize.takeError();
    Size += *FragmentSize;
  }

  if (auto E = decodeRelocationsAndAddends(Reader, Remaining))
    return std::move(E);

  if (auto E = applyAddends(Reader, SectionContents))
    return std::move(E);

  Reader.Addends.clear();
  Reader.OS << SectionContents;

  return Size;
}

Expected<uint64_t>
DebugAbbrevSectionRef::materialize(MCCASReader &Reader,
                                   raw_ostream *Stream) const {
  // Start a new section for relocations.
  Reader.Relocations.emplace_back();
  SmallVector<char, 0> SectionContents;
  raw_svector_ostream SectionStream(SectionContents);

  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto FragmentSize = Reader.materializeSection(ID, &SectionStream);
    if (!FragmentSize)
      return FragmentSize.takeError();
    Size += *FragmentSize;
  }

  if (auto E = decodeRelocationsAndAddends(Reader, Remaining))
    return std::move(E);

  if (auto E = applyAddends(Reader, SectionContents))
    return std::move(E);

  Reader.Addends.clear();
  Reader.OS << SectionContents;

  return Size;
}

Expected<uint64_t> DebugLineSectionRef::materialize(MCCASReader &Reader,
                                                    raw_ostream *Stream) const {
// Start a new section for relocations.
  Reader.Relocations.emplace_back();
  SmallVector<char, 0> SectionContents;
  raw_svector_ostream SectionStream(SectionContents);

  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto FragmentSize = Reader.materializeSection(ID, &SectionStream);
    if (!FragmentSize)
      return FragmentSize.takeError();
    Size += *FragmentSize;
  }

  if (auto E = decodeRelocationsAndAddends(Reader, Remaining))
    return std::move(E);

  if (auto E = applyAddends(Reader, SectionContents))
    return std::move(E);

  Reader.Addends.clear();
  Reader.OS << SectionContents;

  return Size;
}

Expected<uint64_t>
DebugStringSectionRef::materialize(MCCASReader &Reader,
                                   raw_ostream *Stream) const {
  // Start a new section for relocations.
  Reader.Relocations.emplace_back();
  SmallVector<char, 0> SectionContents;
  raw_svector_ostream SectionStream(SectionContents);

  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto FragmentSize = Reader.materializeSection(ID, &SectionStream);
    if (!FragmentSize)
      return FragmentSize.takeError();
    Size += *FragmentSize;
  }

  if (auto E = decodeRelocationsAndAddends(Reader, Remaining))
    return std::move(E);

  if (auto E = applyAddends(Reader, SectionContents))
    return std::move(E);

  Reader.Addends.clear();
  Reader.OS << SectionContents;

  return Size;
}

Expected<AtomRef> AtomRef::create(MCCASBuilder &MB,
                                  ArrayRef<cas::ObjectRef> Fragments) {
  Expected<Builder> B = Builder::startNode(MB.Schema, KindString);
  if (!B)
    return B.takeError();

  if (auto E = encodeReferences(Fragments, B->Data, B->Refs))
    return std::move(E);

  writeRelocationsAndAddends(MB.getAtomRelocs(), MB.getAtomAddends(), B->Data);

  return get(B->build());
}

Expected<uint64_t> AtomRef::materialize(MCCASReader &Reader,
                                        raw_ostream *Stream) const {
  unsigned Size = 0;
  StringRef Remaining = getData();
  auto Refs = decodeReferences(*this, Remaining);
  if (!Refs)
    return Refs.takeError();

  for (auto ID : *Refs) {
    auto FragmentSize = Reader.materializeAtom(ID, Stream);
    if (!FragmentSize)
      return FragmentSize.takeError();
    Size += *FragmentSize;
  }

  if (auto E = decodeRelocationsAndAddends(Reader, Remaining))
    return std::move(E);

  return Size;
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

Expected<uint64_t> MCAlignFragmentRef::materialize(MCCASReader &Reader,
                                                   raw_ostream *Stream) const {
  uint64_t Count;
  auto Remaining = getData();
  auto Endian = Reader.getEndian();
  if (auto E = consumeVBR8(Remaining, Count))
    return std::move(E);

  // hasEmitNops.
  if (!Count) {
    *Stream << Remaining;
    return Remaining.size();
  }
  int64_t Value;
  unsigned ValueSize;
  if (auto E = consumeVBR8(Remaining, Value))
    return std::move(E);
  if (auto E = consumeVBR8(Remaining, ValueSize))
    return std::move(E);

  for (uint64_t I = 0; I != Count; ++I) {
    switch (ValueSize) {
    default:
      llvm_unreachable("Invalid size!");
    case 1:
      *Stream << char(Value);
      break;
    case 2:
      support::endian::write<uint16_t>(*Stream, Value, Endian);
      break;
    case 4:
      support::endian::write<uint32_t>(*Stream, Value, Endian);
      break;
    case 8:
      support::endian::write<uint64_t>(*Stream, Value, Endian);
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
MCBoundaryAlignFragmentRef::materialize(MCCASReader &Reader,
                                        raw_ostream *Stream) const {
  *Stream << getData();
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
MCCVInlineLineTableFragmentRef::materialize(MCCASReader &Reader,
                                            raw_ostream *Stream) const {
  *Stream << getData();
  return getData().size();
}

Expected<MCDummyFragmentRef>
MCDummyFragmentRef::create(MCCASBuilder &MB, const MCDummyFragment &F,
                           unsigned FragmentSize) {
  llvm_unreachable("Should not have been added");
}

Expected<uint64_t> MCDummyFragmentRef::materialize(MCCASReader &Reader,
                                                   raw_ostream *Stream) const {
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

Expected<uint64_t> MCFillFragmentRef::materialize(MCCASReader &Reader,
                                                  raw_ostream *Stream) const {
  StringRef Remaining = getData();
  uint64_t Size;
  uint64_t Value;
  unsigned ValueSize;
  if (auto E = consumeVBR8(Remaining, Size))
    return std::move(E);
  if (auto E = consumeVBR8(Remaining, Value))
    return std::move(E);
  if (auto E = consumeVBR8(Remaining, ValueSize))
    return std::move(E);

  // FIXME: Code duplication from writeFragment.
  const unsigned MaxChunkSize = 16;
  char Data[MaxChunkSize];
  for (unsigned I = 0; I != ValueSize; ++I) {
    unsigned Index =
        Reader.getEndian() == support::little ? I : (ValueSize - I - 1);
    Data[I] = uint8_t(Value >> (Index * 8));
  }
  for (unsigned I = ValueSize; I < MaxChunkSize; ++I)
    Data[I] = Data[I - ValueSize];

  const unsigned NumPerChunk = MaxChunkSize / ValueSize;
  const unsigned ChunkSize = ValueSize * NumPerChunk;

  StringRef Ref(Data, ChunkSize);
  for (uint64_t I = 0, E = Size / ChunkSize; I != E; ++I)
    *Stream << Ref;

  unsigned TrailingCount = Size % ChunkSize;
  if (TrailingCount)
    Stream->write(Data, TrailingCount);
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

Expected<uint64_t> MCLEBFragmentRef::materialize(MCCASReader &Reader,
                                                 raw_ostream *Stream) const {
  *Stream << getData();
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

Expected<uint64_t> MCNopsFragmentRef::materialize(MCCASReader &Reader,
                                                  raw_ostream *Stream) const {
  *Stream << getData();
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

Expected<uint64_t> MCOrgFragmentRef::materialize(MCCASReader &Reader,
                                                 raw_ostream *Stream) const {
  *Stream << getData();
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
MCSymbolIdFragmentRef::materialize(MCCASReader &Reader,
                                   raw_ostream *Stream) const {
  *Stream << getData();
  return getData().size();
}

#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  Expected<MCFragmentName##Ref> MCFragmentName##Ref::create(                   \
      MCCASBuilder &MB, const MCFragmentName &F, unsigned FragmentSize) {      \
    Expected<Builder> B = Builder::startNode(MB.Schema, KindString);           \
    if (!B)                                                                    \
      return B.takeError();                                                    \
    MB.Asm.writeFragmentPadding(MB.FragmentOS, F, FragmentSize);               \
    ArrayRef<char> Contents = F.getContents();                                 \
    B->Data.append(MB.FragmentData);                                           \
    B->Data.append(Contents.begin(), Contents.end());                          \
    assert(((MB.FragmentData.empty() && Contents.empty()) ||                   \
            (MB.FragmentData.size() + Contents.size() == FragmentSize)) &&     \
           "Size should match");                                               \
    return get(B->build());                                                    \
  }                                                                            \
  Expected<uint64_t> MCFragmentName##Ref::materialize(                         \
      MCCASReader &Reader, raw_ostream *Stream) const {                        \
    *Stream << getData();                                                      \
    return getData().size();                                                   \
  }
#define MCFRAGMENT_ENCODED_FRAGMENT_ONLY
#include "llvm/MC/CAS/MCCASObjectV1.def"

Expected<MCAssemblerRef> MCAssemblerRef::get(Expected<MCObjectProxy> Ref) {
  auto Specific = SpecificRefT::getSpecific(std::move(Ref));
  if (!Specific)
    return Specific.takeError();

  return MCAssemblerRef(*Specific);
}

DwarfSectionsCache mccasformats::v1::getDwarfSections(MCAssembler &Asm) {
  return DwarfSectionsCache{
      Asm.getContext().getObjectFileInfo()->getDwarfInfoSection(),
      Asm.getContext().getObjectFileInfo()->getDwarfLineSection(),
      Asm.getContext().getObjectFileInfo()->getDwarfStrSection(),
      Asm.getContext().getObjectFileInfo()->getDwarfAbbrevSection()};
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

  addNode(*Header);
  return Error::success();
}

Error MCCASBuilder::buildFragment(const MCFragment &F, unsigned Size) {
  FragmentData.clear();
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
#include "llvm/MC/CAS/MCCASObjectV1.def"
  }
  llvm_unreachable("unknown fragment");
}

class MCDataFragmentMerger {
public:
  MCDataFragmentMerger(MCCASBuilder &Builder, const MCSection *Sec)
      : Builder(Builder) {}
  ~MCDataFragmentMerger() { assert(MergeCandidates.empty() && "Not flushed"); }

  Error tryMerge(const MCFragment &F, unsigned Size);
  Error flush() { return emitMergedFragments(); }

private:
  Error emitMergedFragments();
  void reset();

  MCCASBuilder &Builder;
  unsigned CurrentSize = 0;
  std::vector<std::pair<const MCFragment *, unsigned>> MergeCandidates;
};

Error MCDataFragmentMerger::tryMerge(const MCFragment &F, unsigned Size) {
  bool IsSameAtom = Builder.getCurrentAtom() == F.getAtom();
  bool Oversized = CurrentSize + Size > MCDataMergeThreshold;
  // TODO: Try merge align fragment?
  bool IsMergeableFragment =
      isa<MCEncodedFragment>(F) || isa<MCAlignFragment>(F);
  // If not the same atom, flush merge candidate and return false.
  if (!IsSameAtom || !IsMergeableFragment || Oversized) {
    if (auto E = emitMergedFragments())
      return E;

    // If it is a new Atom, start a new sub-section.
    if (!IsSameAtom) {
      if (auto E = Builder.finalizeAtom())
        return E;
      Builder.startAtom(F.getAtom());
    }
  }

  // Emit none Data segments.
  if (!IsMergeableFragment) {
    if (auto E = Builder.buildFragment(F, Size))
      return E;

    return Error::success();
  }

  // Add the fragment to the merge candidate.
  CurrentSize += Size;
  MergeCandidates.emplace_back(&F, Size);

  return Error::success();
}

static Error writeAlignFragment(MCCASBuilder &Builder,
                                const MCAlignFragment &AF, raw_ostream &OS,
                                unsigned FragmentSize) {
  uint64_t Count = FragmentSize / AF.getValueSize();
  if (AF.hasEmitNops()) {
    if (!Builder.Asm.getBackend().writeNopData(OS, Count,
                                               AF.getSubtargetInfo()))
      return createStringError(inconvertibleErrorCode(),
                               "unable to write nop sequence of " +
                                   Twine(Count) + " bytes");
    return Error::success();
  }
  auto Endian = Builder.ObjectWriter.Target.isLittleEndian() ? support::little
                                                             : support::big;
  for (uint64_t I = 0; I != Count; ++I) {
    switch (AF.getValueSize()) {
    default:
      llvm_unreachable("Invalid size!");
    case 1:
      OS << char(AF.getValue());
      break;
    case 2:
      support::endian::write<uint16_t>(OS, AF.getValue(), Endian);
      break;
    case 4:
      support::endian::write<uint32_t>(OS, AF.getValue(), Endian);
      break;
    case 8:
      support::endian::write<uint64_t>(OS, AF.getValue(), Endian);
      break;
    }
  }
  return Error::success();
}

Error MCDataFragmentMerger::emitMergedFragments() {
  if (MergeCandidates.empty())
    return Error::success();

  // Use normal node to store the node.
  if (MergeCandidates.size() == 1) {
    auto E = Builder.buildFragment(*MergeCandidates.front().first,
                                   MergeCandidates.front().second);
    reset();
    return E;
  }
  SmallString<8> FragmentData;
  raw_svector_ostream FragmentOS(FragmentData);
  for (auto &F : MergeCandidates) {
    switch (F.first->getKind()) {
#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  case MCFragment::MCEnumName: {                                               \
    const MCFragmentName *SF = cast<MCFragmentName>(F.first);                  \
    Builder.Asm.writeFragmentPadding(FragmentOS, *SF, F.second);               \
    FragmentData.append(SF->getContents());                                    \
    break;                                                                     \
  }
#define MCFRAGMENT_ENCODED_FRAGMENT_ONLY
#include "llvm/MC/CAS/MCCASObjectV1.def"
    case MCFragment::FT_Align: {
      const MCAlignFragment *AF = cast<MCAlignFragment>(F.first);
      if (auto E = writeAlignFragment(Builder, *AF, FragmentOS, F.second))
        return E;
      break;
    }
    default:
      llvm_unreachable("other framgents should not be added");
    }
  }

  auto FN = MergedFragmentRef::create(Builder, FragmentData);
  if (!FN)
    return FN.takeError();
  Builder.addNode(*FN);

  // Clear state.
  reset();
  return Error::success();
}

void MCDataFragmentMerger::reset() {
  CurrentSize = 0;
  MergeCandidates.clear();
}

Error MCCASBuilder::createPaddingRef(const MCSection *Sec) {
  uint64_t Pad = ObjectWriter.getPaddingSize(Sec, Layout);
  auto Fill = PaddingRef::create(*this, Pad);
  if (!Fill)
    return Fill.takeError();
  addNode(*Fill);
  return Error::success();
}

Error MCCASBuilder::createStringSection(
    StringRef S, std::function<Error(StringRef, unsigned)> CreateFn) {
  assert(S.endswith("\0") && "String sections are null terminated");
  unsigned DebugStringOffset = 0;
  if (!SplitStringSections)
    return CreateFn(S, DebugStringOffset);

  while (!S.empty()) {
    auto SplitSym = S.split('\0');
    if (auto E = CreateFn(SplitSym.first, DebugStringOffset))
      return E;
    DebugStringOffset += SplitSym.first.size() + 1;

    S = SplitSym.second;
  }
  return Error::success();
}

/// Reads and returns the length field of a dwarf header contained in Reader,
/// assuming Reader is positioned at the beginning of the header. The Reader's
/// state is advanced to the first byte after the header.
static Expected<size_t> getSizeFromDwarfHeader(BinaryStreamReader &Reader) {
  // From DWARF 5 section 7.4:
  // In the 32-bit DWARF format, an initial length field [...] is an unsigned
  // 4-byte integer (which must be less than 0xfffffff0);
  uint32_t Word1;
  if (auto E = Reader.readInteger(Word1))
    return std::move(E);

  // TODO: handle 64-bit DWARF format.
  if (Word1 >= 0xfffffff0)
    return createStringError(inconvertibleErrorCode(),
                             "DWARF input is not in the 32-bit format");

  return Word1;
}

Expected<size_t>
mccasformats::v1::getSizeFromDwarfHeaderAndSkip(BinaryStreamReader &Reader) {
  Expected<size_t> Size = getSizeFromDwarfHeader(Reader);
  if (!Size)
    return Size.takeError();
  if (auto E = Reader.skip(*Size))
    return std::move(E);
  return Size;
}

/// Returns the Abbreviation Offset field of a Dwarf Compilation Unit (CU)
/// contained in CUData, as well as the total number of bytes taken by the CU.
/// Note: this is different from the length field of the Dwarf header, which
/// does not account for the header size.
static Expected<CUInfo>
getAndSetDebugAbbrevOffsetAndSkip(MutableArrayRef<char> CUData,
                                  support::endianness Endian,
                                  Optional<uint32_t> NewOffset, bool SkipData) {
  BinaryStreamReader Reader(toStringRef(CUData), Endian);
  Expected<size_t> Size = getSizeFromDwarfHeader(Reader);
  if (!Size)
    return Size.takeError();

  size_t AfterSizeOffset = Reader.getOffset();

  // 2-byte Dwarf version identifier.
  uint16_t DwarfVersion;
  if (auto E = Reader.readInteger(DwarfVersion))
    return std::move(E);

  // TODO: Dwarf 5 has a different order for the next fields.
  if (DwarfVersion != 4)
    return createStringError(inconvertibleErrorCode(),
                             "Expected Dwarf 4 input");

  // TODO: Handle Dwarf 64 format, which uses 8 bytes.
  size_t AbbrevPosition = Reader.getOffset();
  uint32_t AbbrevOffset;
  if (auto E = Reader.readInteger(AbbrevOffset))
    return std::move(E);

  if (NewOffset.has_value()) {
    // FIXME: safe but ugly cast. Similar to: llvm::arrayRefFromStringRef.
    auto UnsignedData = makeMutableArrayRef(
        reinterpret_cast<uint8_t *>(CUData.data()), CUData.size());
    BinaryStreamWriter Writer(UnsignedData, Endian);
    Writer.setOffset(AbbrevPosition);
    if (auto E = Writer.writeInteger(*NewOffset))
      return std::move(E);
  }

  // Do not skip the CU length when materializing the debug info section, the CU
  // length will be larger than the CUData length.
  if (SkipData) {
    Reader.setOffset(AfterSizeOffset);
    if (auto E = Reader.skip(*Size))
      return std::move(E);
  }
  return CUInfo{Reader.getOffset(), AbbrevOffset};
}

/// Given a list of MCFragments, return a vector with the concatenation of their
/// data contents.
/// If any fragment is not an MCDataFragment, or the fragment is an
/// MCDwarfLineAddrFragment and the section containing that fragment is not a
/// debug_line section, an error is returned.
Expected<SmallVector<char, 0>> MCCASBuilder::mergeMCFragmentContents(
    const MCSection::FragmentListType &FragmentList, bool IsDebugLineSection) {
  SmallVector<char, 0> mergedData;
  for (const MCFragment &Fragment : FragmentList) {
    if (const auto *DataFragment = dyn_cast<MCDataFragment>(&Fragment))
      llvm::append_range(mergedData, DataFragment->getContents());
    else if (const auto *DwarfLineAddrFrag =
                 dyn_cast<MCDwarfLineAddrFragment>(&Fragment))
      if (IsDebugLineSection)
        llvm::append_range(mergedData, DwarfLineAddrFrag->getContents());
      else
        return createStringError(
            inconvertibleErrorCode(),
            "Invalid MCDwarfLineAddrFragment in a non debug line section");
    else
      return createStringError(inconvertibleErrorCode(),
                               "Invalid fragment type");
  }
  return mergedData;
}

Expected<MCCASBuilder::CUSplit>
MCCASBuilder::splitDebugInfoSectionData(MutableArrayRef<char> DebugInfoData) {
  CUSplit Split;
  // CU splitting loop.
  while (!DebugInfoData.empty()) {
    Expected<CUInfo> Info = getAndSetDebugAbbrevOffsetAndSkip(
        DebugInfoData, Asm.getBackend().Endian, /*NewOffset*/ 0);
    if (!Info)
      return Info.takeError();
    Split.SplitCUData.push_back(DebugInfoData.take_front(Info->CUSize));
    Split.AbbrevOffsets.push_back(Info->AbbrevOffset);
    DebugInfoData = DebugInfoData.drop_front(Info->CUSize);
  }

  return Split;
}

Error MCCASBuilder::createDebugInfoSection(
    ArrayRef<DebugInfoCURef> CURefs, DebugAbbrevOffsetsRef AbbrevOffsetsRef,
    DebugInfoDistinctDataRef DebugDistinctDataRef) {
  if (CURefs.empty())
    return Error::success();

  startSection(DwarfSections.DebugInfo);
  addNode(DebugDistinctDataRef);
  addNode(AbbrevOffsetsRef);
  for (auto CURef : CURefs)
    addNode(CURef);
  if (auto E = createPaddingRef(DwarfSections.DebugInfo))
    return E;
  return finalizeSection<DebugInfoSectionRef>();
}

Error MCCASBuilder::createDebugAbbrevSection(
    ArrayRef<DebugAbbrevRef> AbbrevRefs) {
  startSection(DwarfSections.Abbrev);
  for (auto AbbrevRef : AbbrevRefs)
    addNode(AbbrevRef);
  if (auto E = createPaddingRef(DwarfSections.Abbrev))
    return E;
  return finalizeSection<DebugAbbrevSectionRef>();
}

Expected<SmallVector<DebugAbbrevRef>>
MCCASBuilder::splitAbbrevSection(ArrayRef<size_t> AbbrevOffsetVec,
                                 ArrayRef<char> FullAbbrevData) {
  // If we are here, the Abbrev section _must_ exist.
  if (!DwarfSections.Abbrev)
    return createStringError(inconvertibleErrorCode(),
                             "Missing __debug_abbrev section");

  // Sorted container of offsets such that, for any iterator `it`, the interval
  // [*it, *(it+1)] describes where to find one Abbreviation contribution.
  SmallVector<size_t> AbbrevOffsets(AbbrevOffsetVec);
  AbbrevOffsets.push_back(FullAbbrevData.size());
  sort(AbbrevOffsets);
  AbbrevOffsets.erase(unique(AbbrevOffsets, std::equal_to<size_t>()),
                      AbbrevOffsets.end());

  size_t StartOffset = *AbbrevOffsets.begin();
  if (StartOffset != 0)
    return createStringError(
        inconvertibleErrorCode(),
        "Expected one of the abbreviation offsets to be zero");

  SmallVector<DebugAbbrevRef> AbbrevRefs;
  for (size_t EndOffset : drop_begin(AbbrevOffsets)) {
    StringRef AbbrevData =
        toStringRef(FullAbbrevData).slice(StartOffset, EndOffset);
    auto AbbrevRef = DebugAbbrevRef::create(*this, AbbrevData);
    if (!AbbrevRef)
      return AbbrevRef.takeError();
    AbbrevRefs.push_back(*AbbrevRef);
    StartOffset = EndOffset;
  }

  return AbbrevRefs;
}

Expected<SmallVector<size_t>> DebugAbbrevOffsetsRefAdaptor::decodeOffsets() {
  SmallVector<size_t>  DecodedOffsets;

  for (StringRef Data = Ref.getData(); !Data.empty();) {
    size_t Offset;
    if (auto E = consumeVBR8(Data, Offset))
      return std::move(E);
    DecodedOffsets.push_back(Offset);
  }

  return DecodedOffsets;
}

SmallVector<char>
DebugAbbrevOffsetsRefAdaptor::encodeOffsets(ArrayRef<size_t> Offsets) {
  SmallVector<char> EncodedOffsets;
  for (auto Offset : Offsets)
    writeVBR8(Offset, EncodedOffsets);
  return EncodedOffsets;
}

static void partitionCUDie(MCCASBuilder &Builder,
    InMemoryCASDWARFObject::PartitionedDebugInfoSection &PartitionedData,
    DWARFDie &CUDie, ArrayRef<char> DebugInfoData, uint64_t &CUOffset,
    bool IsLittleEndian, uint8_t AddressSize) {

  // Copy Abbrev Tag
  DWARFDataExtractor DWARFExtractor(toStringRef(DebugInfoData), IsLittleEndian,
                                    AddressSize);
  auto PrevOffset = CUOffset;
  DWARFExtractor.getULEB128(&CUOffset);
  auto Size = CUOffset - PrevOffset;
  append_range(PartitionedData.DebugInfoCURefData,
               DebugInfoData.slice(PrevOffset, Size));
  for (const DWARFAttribute &AttrValue : CUDie.attributes()) {
    if (is_contained(InMemoryCASDWARFObject::PartitionedDebugInfoSection::
                         FormsToPartition,
                     AttrValue.Value.getForm()) ||
        AttrValue.Attr == llvm::dwarf::DW_AT_name ||
        AttrValue.Attr == llvm::dwarf::DW_AT_linkage_name) {
      append_range(PartitionedData.DistinctData,
                   DebugInfoData.slice(AttrValue.Offset, AttrValue.ByteSize));
    } else if (AttrValue.Value.getForm() == llvm::dwarf::DW_FORM_strp) {
      // TODO: Add support for 64-bit DWARF
      uint32_t StringOffset;
      memcpy(&StringOffset, &DebugInfoData[AttrValue.Offset], 4);
      auto StringRef = Builder.getObjectRefFromStringMap(StringOffset);
      if (!StringRef)
        llvm_unreachable("Strp value not found in debug string section!");
      PartitionedData.DebugStringRefs.push_back(*StringRef);
    } else if (AttrValue.Attr != llvm::dwarf::DW_AT_stmt_list)
      append_range(PartitionedData.DebugInfoCURefData,
                   DebugInfoData.slice(AttrValue.Offset, AttrValue.ByteSize));
    CUOffset += AttrValue.ByteSize;
  }

  DWARFDie Child = CUDie.getFirstChild();
  while (Child && Child.getAbbreviationDeclarationPtr()) {
    partitionCUDie(Builder, PartitionedData, Child, DebugInfoData, CUOffset,
                   IsLittleEndian, AddressSize);
    Child = Child.getSibling();
  }
  if (Child && !Child.getAbbreviationDeclarationPtr()) {
    PartitionedData.DebugInfoCURefData.push_back(DebugInfoData[CUOffset]);
    CUOffset++;
  }
}

Expected<InMemoryCASDWARFObject::PartitionedDebugInfoSection>
InMemoryCASDWARFObject::partitionCUData(MCCASBuilder &Builder, ArrayRef<char> DebugInfoData,
                                        uint64_t AbbrevOffset,
                                        uint64_t CUOffset, DWARFContext *Ctx, ArrayRef<DebugStrRef> DebugStringRefs) {

  StringRef AbbrevSectionContribution =
      getAbbrevSection().drop_front(AbbrevOffset);
  DWARFDebugAbbrev Abbrev;
  Abbrev.extract(DataExtractor(AbbrevSectionContribution, isLittleEndian(), 8));
  uint64_t OffsetPtr = 0;
  DWARFUnitHeader Header;
  DWARFSection Section = {toStringRef(DebugInfoData), 0 /*Address*/};
  Header.extract(*Ctx, DWARFDataExtractor(*this, Section, isLittleEndian(), 8),
                 &OffsetPtr, DWARFSectionKind::DW_SECT_INFO);

  DWARFUnitVector UV;
  DWARFCompileUnit DCU(*Ctx, Section, Header, &Abbrev, &getRangesSection(),
                       &getLocSection(), getStrSection(),
                       getStrOffsetsSection(), &getAddrSection(),
                       getLocSection(), isLittleEndian(), false, UV);

  InMemoryCASDWARFObject::PartitionedDebugInfoSection PartitionedData;
  if (DWARFDie CUDie = DCU.getUnitDIE(false)) {
    // Copy 11 bytes which represents the 32-bit DWARF Header for DWARF4.
    if (DebugInfoData.size() < Dwarf4HeaderSize32Bit)
      return createStringError(inconvertibleErrorCode(),
                               "DebugInfoData is too small, it doesn't even "
                               "contain a 32-bit DWARF Header");
    append_range(PartitionedData.DebugInfoCURefData,
                 DebugInfoData.take_front(Dwarf4HeaderSize32Bit));
    CUOffset += Dwarf4HeaderSize32Bit;

    DWARFDataExtractor DWARFExtractor(
        toStringRef(PartitionedData.DebugInfoCURefData), IsLittleEndian, 8);
    uint64_t Offset = 0;
    DWARFDataExtractor::Cursor C(Offset);
    auto HeaderLength = DWARFExtractor.getRelocatedValue(C, 4);
    auto DwarfVersion = DWARFExtractor.getRelocatedValue(C, 2);

    // TODO: Add support for DWARF 5
    if (DwarfVersion != 4)
      return createStringError(inconvertibleErrorCode(),
                               "Only DWARF 4 is supported right now");

    // TODO: Add support for 64-bit DWARF Format
    if (HeaderLength == 0xffffffff)
      return createStringError(inconvertibleErrorCode(),
                               "64-bit DWARF Format not yet supported");
    if (HeaderLength >= 0xfffffff0)
      llvm_unreachable("Unknown DWARF Format");

    if (!C)
      return C.takeError();

    partitionCUDie(Builder, PartitionedData, CUDie, DebugInfoData, CUOffset,
                   IsLittleEndian, DCU.getAddressByteSize());
  }
  return PartitionedData;
}

Expected<AbbrevAndDebugSplit> MCCASBuilder::splitDebugInfoAndAbbrevSections(
    ArrayRef<DebugStrRef> DebugStringRefs) {
  if (!DwarfSections.DebugInfo)
    return AbbrevAndDebugSplit{};

  const MCSection::FragmentListType &FragmentList =
      DwarfSections.DebugInfo->getFragmentList();
  Expected<SmallVector<char, 0>> DebugInfoData =
      mergeMCFragmentContents(FragmentList);
  if (!DebugInfoData)
    return DebugInfoData.takeError();

  Expected<CUSplit> SplitInfo = splitDebugInfoSectionData(*DebugInfoData);
  if (!SplitInfo)
    return SplitInfo.takeError();

  const MCSection::FragmentListType &AbbrevFragmentList =
      DwarfSections.Abbrev->getFragmentList();

  Expected<SmallVector<char, 0>> FullAbbrevData =
      mergeMCFragmentContents(AbbrevFragmentList);
  if (!FullAbbrevData)
    return FullAbbrevData.takeError();

  InMemoryCASDWARFObject CASObj(
      *FullAbbrevData, Asm.getBackend().Endian == support::endianness::little);
  auto DWARFObj = std::make_unique<InMemoryCASDWARFObject>(CASObj);
  auto DWARFContextHolder = std::make_unique<DWARFContext>(std::move(DWARFObj));
  auto *DWARFCtx = DWARFContextHolder.get();

  Expected<SmallVector<DebugAbbrevRef>> AbbrevRefs =
      splitAbbrevSection(SplitInfo->AbbrevOffsets, *FullAbbrevData);
  if (!AbbrevRefs)
    return AbbrevRefs.takeError();

  SmallVector<DebugInfoCURef> CURefs;
  SmallVector<char> DistinctData;
  for (auto [CUData, AbbrevOffset] :
       llvm::zip(SplitInfo->SplitCUData, SplitInfo->AbbrevOffsets)) {
    uint64_t CUOffset = 0;
    auto PartitionedData =
        CASObj.partitionCUData(*this, CUData, AbbrevOffset, CUOffset, DWARFCtx, DebugStringRefs);
    if (!PartitionedData)
      return PartitionedData.takeError();
    DistinctData.append(PartitionedData->DistinctData.begin(),
                        PartitionedData->DistinctData.end());
    auto DbgInfoRef = DebugInfoCURef::create(
        *this, toStringRef(PartitionedData->DebugInfoCURefData),
        PartitionedData->DebugStringRefs);
    if (!DbgInfoRef)
      return DbgInfoRef.takeError();
    CURefs.push_back(*DbgInfoRef);
  }

  SmallVector<char> EncodedOffsets =
      DebugAbbrevOffsetsRefAdaptor::encodeOffsets(SplitInfo->AbbrevOffsets);
  auto AbbrevOffsetsRef =
      DebugAbbrevOffsetsRef::create(*this, toStringRef(EncodedOffsets));
  if (!AbbrevOffsetsRef)
    return AbbrevOffsetsRef.takeError();

  auto DebugDistinctDataRef =
      DebugInfoDistinctDataRef::create(*this, toStringRef(DistinctData));
  if (!DebugDistinctDataRef)
    return DebugDistinctDataRef.takeError();

  return AbbrevAndDebugSplit{std::move(CURefs), std::move(*AbbrevRefs),
                             *AbbrevOffsetsRef, *DebugDistinctDataRef};
}

Error MCCASBuilder::createLineSection() {
  if (!DwarfSections.Line)
    return Error::success();

  Expected<SmallVector<char, 0>> DebugLineData =
      mergeMCFragmentContents(DwarfSections.Line->getFragmentList(), true);
  if (!DebugLineData)
    return DebugLineData.takeError();

  startSection(DwarfSections.Line);

  StringRef DebugLineStrRef(DebugLineData->data(), DebugLineData->size());
  BinaryStreamReader SectionReader(DebugLineStrRef, Asm.getBackend().Endian);
  // Iterate over the line section contents and split it up into individual cas
  // blocks that represent one line table per function.
  while (!SectionReader.empty()) {
    uint64_t SectionStartOffset = SectionReader.getOffset();
    auto Length = getSizeFromDwarfHeaderAndSkip(SectionReader);
    if (!Length)
      return Length.takeError();
    StringRef LineTableData = DebugLineStrRef.substr(
        SectionStartOffset, SectionReader.getOffset() - SectionStartOffset);
    auto DbgLineRef = DebugLineRef::create(*this, LineTableData);
    if (!DbgLineRef)
      return DbgLineRef.takeError();
    addNode(*DbgLineRef);
  }
  if (auto E = createPaddingRef(DwarfSections.Line))
    return E;
  return finalizeSection<DebugLineSectionRef>();
}

Error MCCASBuilder::createDebugStrSection(
    ArrayRef<DebugStrRef> DebugStringRefs) {

  startSection(DwarfSections.Str);
  for (auto DebugStringRef : DebugStringRefs)
    addNode(DebugStringRef);
  return finalizeSection<DebugStringSectionRef>();
}

static void createAddendVector(
    SmallVector<MachObjectWriter::AddendsSizeAndOffset> &Dest,
    const std::vector<MachObjectWriter::AddendsSizeAndOffset> &Source,
    const MCFragment *F, const MCAsmLayout &Layout) {
  // Encode the fragment offset with the Addend offset to get the proper fixup
  // offset in that section.
  for (auto Addend : Source)
    Dest.push_back(
        {Addend.Value,
         static_cast<uint32_t>(Addend.Offset + Layout.getFragmentOffset(F)),
         Addend.Size});
}

Expected<MCCASBuilder::DebugStringSectionContents>
MCCASBuilder::createDebugStringRefs() {
  if (!DwarfSections.Str || !DwarfSections.Str->getFragmentList().size())
    return DebugStringSectionContents{};

  assert(DwarfSections.Str->getFragmentList().size() == 1 &&
         "One fragment in debug str section");

  DebugStringSectionContents DebugStringContents;
  ArrayRef<char> DebugStrData =
      cast<MCDataFragment>(*DwarfSections.Str->begin()).getContents();
  StringRef S(DebugStrData.data(), DebugStrData.size());
  if (auto E = createStringSection(
          S, [&](StringRef S, unsigned DebugStringOffset) -> Error {
            auto Sym = DebugStrRef::create(*this, S);
            if (!Sym)
              return Sym.takeError();
            DebugStringContents.DebugStringRefs.push_back(*Sym);
            DebugStringSectionContents::MapOfStringRefs.try_emplace(
                DebugStringOffset, Sym->getRef());
            return Error::success();
          }))
    return E;
  return DebugStringContents;
}

Error MCCASBuilder::buildFragments() {
  startGroup();

  auto DebugStringContents = createDebugStringRefs();
  if (!DebugStringContents)
    return DebugStringContents.takeError();

  Expected<AbbrevAndDebugSplit> AbbrevAndCURefs =
      splitDebugInfoAndAbbrevSections(DebugStringContents->DebugStringRefs);
  if (!AbbrevAndCURefs)
    return AbbrevAndCURefs.takeError();

  for (const MCSection &Sec : Asm) {
    if (Sec.isVirtualSection() || Sec.getFragmentList().empty())
      continue;

    // Handle Debug Info sections separately.
    if (&Sec == DwarfSections.DebugInfo) {
      if (auto E = createDebugInfoSection(
              AbbrevAndCURefs->CURefs, *AbbrevAndCURefs->AbbrevOffsetsRef,
              *AbbrevAndCURefs->DebugDistinctDataRef))
        return E;
      continue;
    }

    // Handle Debug Abbrev sections separately.
    if (&Sec == DwarfSections.Abbrev) {
      if (auto E = createDebugAbbrevSection(AbbrevAndCURefs->AbbrevRefs))
        return E;
      continue;
    }

    // Handle Debug Line sections separately.
    if (&Sec == DwarfSections.Line) {
      if (auto E = createLineSection())
        return E;
      continue;
    }

    // Handle Debug Str sections separately.
    if (&Sec == DwarfSections.Str) {
      if (auto E = createDebugStrSection(DebugStringContents->DebugStringRefs))
        return E;
      continue;
    }

    // Start Subsection for one section.
    startSection(&Sec);

    // Start subsection for first Atom.
    startAtom(Sec.getFragmentList().front().getAtom());

    MCDataFragmentMerger Merger(*this, &Sec);
    for (const MCFragment &F : Sec) {
      if (RelocLocation == Atom) {
        auto Relocs = RelMap.find(&F);
        if (Relocs != RelMap.end())
          AtomRelocs.append(Relocs->second.begin(), Relocs->second.end());
        createAddendVector(AtomAddends, ObjectWriter.getAddends()[&F], &F,
                           Layout);
      } else
        createAddendVector(SectionAddends, ObjectWriter.getAddends()[&F], &F,
                           Layout);

      auto Size = Asm.computeFragmentSize(Layout, F);
      // Don't need to encode the fragment if it doesn't contribute anything.
      if (!Size)
        continue;

      if (auto E = Merger.tryMerge(F, Size))
        return E;
    }
    if (auto E = Merger.flush())
      return E;

    // End last subsection for late Atom.
    if (auto E = finalizeAtom())
      return E;

    if (auto E = createPaddingRef(&Sec))
      return E;

    if (auto E = finalizeSection())
      return E;
  }
  return finalizeGroup();
}

Error MCCASBuilder::buildRelocations() {
  ObjectWriter.resetBuffer();
  if (ObjectWriter.Mode == CASBackendMode::Verify ||
      RelocLocation == CompileUnit)
    ObjectWriter.writeRelocations(Asm, Layout);

  if (RelocLocation == CompileUnit) {
    auto Relocs = RelocationsRef::create(*this, ObjectWriter.getContent());
    if (!Relocs)
      return Relocs.takeError();

    addNode(*Relocs);
  }

  return Error::success();
}

Error MCCASBuilder::buildDataInCodeRegion() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeDataInCodeRegion(Asm, Layout);
  auto Data = DataInCodeRef::create(*this, ObjectWriter.getContent());
  if (!Data)
    return Data.takeError();

  addNode(*Data);
  return Error::success();
}

Error MCCASBuilder::buildSymbolTable() {
  ObjectWriter.resetBuffer();
  ObjectWriter.writeSymbolTable(Asm, Layout);
  StringRef S = ObjectWriter.getContent();
  std::vector<cas::ObjectRef> CStrings;
  if (auto E = createStringSection(
          S, [&](StringRef S, unsigned DebugStringOffset) -> Error {
            auto Sym = CStringRef::create(*this, S);
            if (!Sym)
              return Sym.takeError();
            CStrings.push_back(Sym->getRef());
            return Error::success();
          }))
    return E;

  auto Ref = SymbolTableRef::create(*this, CStrings);
  if (!Ref)
    return Ref.takeError();
  addNode(*Ref);

  return Error::success();
}

void MCCASBuilder::startGroup() {
  assert(GroupContext.empty() && "GroupContext is not empty");
  CurrentContext = &GroupContext;
}

Error MCCASBuilder::finalizeGroup() {
  auto Ref = GroupRef::create(*this, GroupContext);
  if (!Ref)
    return Ref.takeError();
  GroupContext.clear();
  CurrentContext = &Sections;
  addNode(*Ref);
  return Error::success();
}

void MCCASBuilder::startSection(const MCSection *Sec) {
  assert(SectionContext.empty() && !CurrentSection && RelMap.empty() &&
         SectionRelocs.empty() && "SectionContext is not empty");

  CurrentSection = Sec;
  CurrentContext = &SectionContext;

  if (RelocLocation == Atom) {
    // Build a map for lookup.
    for (auto R : ObjectWriter.getRelocations()[Sec]) {
      // For the Dwarf Sections, just append the relocations to the
      // SectionRelocs. No Atoms are considered for this section.
      if (R.F && Sec != DwarfSections.Line && Sec != DwarfSections.DebugInfo &&
          Sec != DwarfSections.Abbrev)
        RelMap[R.F].push_back(R.MRE);
      else
        // If the fragment is nullptr, it should a section with only relocation
        // in section. Encode in section.
        // DebugInfo sections are also encoded in a single section.
        SectionRelocs.push_back(R.MRE);
    }
  }

  if (Sec == DwarfSections.Line || Sec == DwarfSections.DebugInfo) {
    for (auto &Frag : *Sec) {
      createAddendVector(SectionAddends, ObjectWriter.getAddends()[&Frag],
                         &Frag, Layout);
    }
  }

  if (RelocLocation == Section) {
    for (auto R : ObjectWriter.getRelocations()[Sec])
      SectionRelocs.push_back(R.MRE);
  }
}

template <typename SectionRefTy>
Error MCCASBuilder::finalizeSection() {
  auto Ref = SectionRefTy::create(*this, SectionContext);
  if (!Ref)
    return Ref.takeError();

  SectionContext.clear();
  SectionRelocs.clear();
  SectionAddends.clear();
  RelMap.clear();
  CurrentSection = nullptr;
  CurrentContext = &GroupContext;
  addNode(*Ref);

  return Error::success();
}

void MCCASBuilder::startAtom(const MCSymbol *Atom) {
  assert(AtomContext.empty() && AtomRelocs.empty() && !CurrentAtom &&
         "AtomContext is not empty");

  CurrentAtom = Atom;
  CurrentContext = &AtomContext;
}

Error MCCASBuilder::finalizeAtom() {
  auto Ref = AtomRef::create(*this, AtomContext);
  if (!Ref)
    return Ref.takeError();

  AtomContext.clear();
  AtomRelocs.clear();
  AtomAddends.clear();
  CurrentAtom = nullptr;
  CurrentContext = &SectionContext;
  addNode(*Ref);

  return Error::success();
}

void MCCASBuilder::addNode(cas::ObjectProxy Node) {
  CurrentContext->push_back(Node.getRef());
}

Expected<MCAssemblerRef> MCAssemblerRef::create(const MCSchema &Schema,
                                                MachOCASWriter &ObjectWriter,
                                                MCAssembler &Asm,
                                                const MCAsmLayout &Layout,
                                                raw_ostream *DebugOS) {
  MCCASBuilder Builder(Schema, ObjectWriter, Asm, Layout, DebugOS);

  if (auto E = Builder.prepare())
    return std::move(E);

  if (auto E = Builder.buildMachOHeader())
    return std::move(E);

  if (auto E = Builder.buildFragments())
    return std::move(E);

  // Only need to do this for verify mode so we compare the output byte by
  // byte.
  if (ObjectWriter.Mode == CASBackendMode::Verify) {
    ObjectWriter.applyAddends(Asm, Layout);
    ObjectWriter.writeSectionData(Asm, Layout);
  }

  if (auto E = Builder.buildRelocations())
    return std::move(E);

  if (auto E = Builder.buildDataInCodeRegion())
    return std::move(E);

  if (auto E = Builder.buildSymbolTable())
    return std::move(E);

  auto B = Builder::startRootNode(Schema, KindString);
  if (!B)
    return B.takeError();

  // Put Header, Relocations, SymbolTable, etc. in the front.
  B->Refs.append(Builder.Sections.begin(), Builder.Sections.end());

  std::string NormalizedTriple = ObjectWriter.Target.normalize();
  writeVBR8(uint32_t(NormalizedTriple.size()), B->Data);
  B->Data.append(NormalizedTriple);

  return get(B->build());
}

template <typename T>
static Expected<T> findSectionFromAsm(const MCAssemblerRef &Asm) {
  for (unsigned I = 1; I < Asm.getNumReferences(); ++I) {
    auto Node = MCObjectProxy::get(
        Asm.getSchema(), Asm.getSchema().CAS.getProxy(Asm.getReference(I)));
    if (!Node)
      return Node.takeError();
    if (auto Ref = T::Cast(*Node))
      return *Ref;
  }

  return createStringError(inconvertibleErrorCode(),
                           "cannot locate the requested section");
}

template <typename T>
static Expected<uint64_t> materializeData(raw_ostream &OS,
                                          const MCAssemblerRef &Asm) {
  auto Node = findSectionFromAsm<T>(Asm);
  if (!Node)
    return Node.takeError();

  return Node->materialize(OS);
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
  uint64_t Written = 0;
  // MachOHeader.
  auto HeaderSize = materializeData<HeaderRef>(OS, *this);
  if (!HeaderSize)
    return HeaderSize.takeError();
  Written += *HeaderSize;

  // SectionData.
  auto SectionDataRef = findSectionFromAsm<GroupRef>(*this);
  if (!SectionDataRef)
    return SectionDataRef.takeError();
  auto SectionDataSize = SectionDataRef->materialize(Reader);
  if (!SectionDataSize)
    return SectionDataSize.takeError();
  Written += *SectionDataSize;

  // Add padding to pointer size.
  auto SectionDataPad =
      offsetToAlignment(Written, Target.isArch64Bit() ? Align(8) : Align(4));
  OS.write_zeros(SectionDataPad);

  for (auto &Sec : Reader.Relocations) {
    for (auto &Entry : llvm::reverse(Sec)) {
      support::endian::write<uint32_t>(OS, Entry.r_word0, Reader.getEndian());
      support::endian::write<uint32_t>(OS, Entry.r_word1, Reader.getEndian());
    }
  }

  if (auto Relocations = findSectionFromAsm<RelocationsRef>(*this)) {
    auto RelocSize = Relocations->materialize(OS);
    if (!RelocSize)
      return RelocSize.takeError();
  } else
    consumeError(Relocations.takeError()); // Relocations can be missing.

  auto DCOrErr = materializeData<DataInCodeRef>(OS, *this);
  if (!DCOrErr)
    return DCOrErr.takeError();

  auto SymTableRef = findSectionFromAsm<SymbolTableRef>(*this);
  if (!SymTableRef)
    return SymTableRef.takeError();
  auto SymbolTableSize = SymTableRef->materialize(Reader);
  if (!SymbolTableSize)
    return SymbolTableSize.takeError();

  return Error::success();
}

MCCASReader::MCCASReader(raw_ostream &OS, const Triple &Target,
                         const MCSchema &Schema)
    : OS(OS), Target(Target), Schema(Schema) {}

Expected<uint64_t> MCCASReader::materializeGroup(cas::ObjectRef ID) {
  auto Node = MCObjectProxy::get(Schema, Schema.CAS.getProxy(ID));
  if (!Node)
    return Node.takeError();

  // Group can have sections, symbol table strs.
  if (auto F = SectionRef::Cast(*Node))
    return F->materialize(*this);
  if (auto F = DebugStringSectionRef::Cast(*Node))
    return F->materialize(*this);
  if (auto F = CStringRef::Cast(*Node)) {
    auto Size = F->materialize(OS);
    if (!Size)
      return Size.takeError();
    // Write null between strings.
    OS.write_zeros(1);
    return *Size + 1;
  }
  if (auto F = DebugInfoSectionRef::Cast(*Node))
    return createStringError(
        inconvertibleErrorCode(),
        "DebugInfoSectionRef should not be materialized here!");
  if (auto F = DebugLineSectionRef::Cast(*Node))
    return F->materialize(*this);
  if (auto F = DebugAbbrevSectionRef::Cast(*Node))
    return F->materialize(*this);
  return createStringError(inconvertibleErrorCode(),
                           "unsupported CAS node for group");
}

Expected<uint64_t> MCCASReader::materializeSection(cas::ObjectRef ID,
                                                   raw_ostream *Stream) {
  auto Node = MCObjectProxy::get(Schema, Schema.CAS.getProxy(ID));
  if (!Node)
    return Node.takeError();

  // Section can have atoms, padding, debug_strs.
  if (auto F = AtomRef::Cast(*Node))
    return F->materialize(*this, Stream);
  if (auto F = PaddingRef::Cast(*Node))
    return F->materialize(*Stream);
  if (auto F = DebugStrRef::Cast(*Node)) {
    auto Size = F->materialize(*Stream);
    if (!Size)
      return Size.takeError();
    // Write null between strings.
    Stream->write_zeros(1);
    return *Size + 1;
  }
  if (auto F = DebugLineRef::Cast(*Node))
    return F->materialize(*Stream);
  if (auto F = DebugAbbrevRef::Cast(*Node))
    return F->materialize(*Stream);
  return createStringError(inconvertibleErrorCode(),
                           "unsupported CAS node for atom");
}

Expected<uint64_t> MCCASReader::materializeAtom(cas::ObjectRef ID,
                                                raw_ostream *Stream) {
  auto Node = MCObjectProxy::get(Schema, Schema.CAS.getProxy(ID));
  if (!Node)
    return Node.takeError();

#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  if (auto F = MCFragmentName##Ref::Cast(*Node))                               \
    return F->materialize(*this, Stream);
#include "llvm/MC/CAS/MCCASObjectV1.def"
  if (auto F = PaddingRef::Cast(*Node))
    return F->materialize(*Stream);
  if (auto F = MergedFragmentRef::Cast(*Node))
    return F->materialize(*Stream);

  return createStringError(inconvertibleErrorCode(),
                           "unsupported CAS node for fragment");
}
