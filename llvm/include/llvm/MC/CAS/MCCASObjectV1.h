//===- llvm/MC/CAS/MCCASObjectV1.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// CAS Schema:
///
/// Debug Line Section
/// The .debug_line section is stored as one CAS object per line table program.
/// All relocations in the line table programs are zeroed out to maximize
/// deduplication.
///
/// DebugLineSectionRef: The DebugLineSectionRef represents the .debug_line
/// section, it contains the relocations for that section as well as any addends
/// and their offsets and sizes, that were zero-ed out for deduplication
/// purposes. A DebugLineSectionRef has edges to one or more DebugLine Ref.
///
/// DebugLineRef: The DebugLineRef represents one function's contribution to the
/// debug_line section, or one line table. Each function has its own line
/// table entry in the debug_line section when compiled with
/// -fcas-friendly-debug-info.
///
/// ┌─────────────────┐
/// │  DebugLineRef   │
/// │  [Line Table 1] │
/// │                 │         ┌──────────────────────┐
/// └──────────▲──────┘         │  DebugLineSectionRef │
///       .    ├────────────────┤                      │
///       .    │                │     [Relocations]    │
///       .    │                │     [Addends]        │
/// ┌──────────▼──────┐         └──────────────────────┘
/// │  DebugLineRef   │
/// │  [Line Table N] │
/// │                 │
/// └─────────────────┘
///
/// Debug String Section
///
/// DebugStringSectionRef: The DebugStringSectionRef represents the debug_str
/// section. This object is empty and has one edge per string. The order of the
/// strings is implicitly encoded as the order of edges.
///
/// DebugStrRef: A DebugStrRef represents one string in the debug_str section,
/// each string gets its own cas object.
///                         ┌────────────────┐
///            ┌────────────┤  DebugString   ├───────────┐
///            │            │  SectionRef    │           │
///            │            └────────────────┘           │
///     ┌──────▼───────┐                         ┌───────▼──────┐
///     | DebugStrRef  │                         │ DebugStrRef  |
///     │ [String 1]   │          ...            │ [String N]   │
///     └──────────────┘                         └──────────────┘
///
///
/// Debug Info Section
///
/// DebugInfoSectionRef: The DebugInfoSectionRef represents the .debug_info
/// section, it contains the relocations for that section as well as any addends
/// and their offsets and sizes, that were zero-ed out for deduplication
/// purposes.
///
/// DistinctDataRef: The DistinctDataRef is one CAS block per translation unit
/// (object file) that contains all the information from all compile units that
/// is not expected to deduplicate.
///
/// AbbrevOffsetRef: The AbbrevOffsetRef is one CAS block per translation unit
/// that contains all the abbreviation offsets of every compile unit in that
/// translation unit.
///
/// DebugInfoCURef: A DebugInfoCURef represents one function's contribution to
/// the debug_info section, i.e. one compile unit. When compiling with
/// -fcas-friendly each function is stored in its own compile unit instead of
/// the whole translation unit being in one compile unit. The DebugInfoCURef
/// contains only the data in the compile unit that is expected to deduplicate.
///
///                                         ┌───────────────────────┐
///    ┌────────────────────────────┐       │  DebugInfoSectionRef  │
///    │ DistinctDataRef            |◄──┬───┤                       │
///    │ [Data that doesn't dedupe] │   │   │                       │
///    │ [FormsToPartition]         │   │   │     [Relocations]     │
///    └────────────────────────────┘   │   │     [Addends]         │
///                                     │   └────┬─────┬────────────┘
///    ┌────────────────────────┐       │        │     │
///    │ AbbrevOffsetRef        |◄──────┘        │     │
///    │ [Abbrev Offset per CU] │                │     │
///    └────────────────────────┘                │     │
///                                              │     │
///    ┌───────────────────────────┐             │     │
///    |  DebugInfoCURef 1         │             │     │
///    │ [CU data that does dedupe]|◄────────────┘     │
///    │                           │                   │
///    └───────────────────────────┘                   │
///                  .                                 |
///                  .                                 │
///                  .                                 │
///    ┌───────────────────────────┐                   │
///    │  DebugInfoCURef N         │                   │
///    │ [CU data that does dedupe]|◄──────────────────┘
///    │                           │
///    └───────────────────────────┘
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_CAS_MCCASOBJECTV1_H
#define LLVM_MC_CAS_MCCASOBJECTV1_H

#include "llvm/BinaryFormat/MachO.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/MC/CAS/MCCASFormatSchemaBase.h"
#include "llvm/MC/CAS/MCCASReader.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/Support/BinaryStreamReader.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/Endian.h"

namespace llvm {

namespace mccasformats {
namespace v1 {

class MCSchema;
class MCCASBuilder;
class MCCASReader;

// FIXME: Using the same structure from ObjectV1 from CASObjectFormat.
class MCObjectProxy : public cas::ObjectProxy {
public:
  static Expected<MCObjectProxy> get(const MCSchema &Schema,
                                     Expected<cas::ObjectProxy> Ref);
  StringRef getKindString() const;

  /// Return the data skipping the type-id character.
  StringRef getData() const { return cas::ObjectProxy::getData().drop_front(); }

  const MCSchema &getSchema() const { return *Schema; }

  bool operator==(const MCObjectProxy &RHS) const {
    return Schema == RHS.Schema && cas::CASID(*this) == cas::CASID(RHS);
  }

  MCObjectProxy() = delete;

  static Error encodeReferences(ArrayRef<cas::ObjectRef> Refs,
                                SmallVectorImpl<char> &Data,
                                SmallVectorImpl<cas::ObjectRef> &IDs);

  static Expected<SmallVector<cas::ObjectRef>>
  decodeReferences(const MCObjectProxy &Node, StringRef &Remaining);

protected:
  MCObjectProxy(const MCSchema &Schema, const cas::ObjectProxy &Node)
      : cas::ObjectProxy(Node), Schema(&Schema) {}

  class Builder {
  public:
    static Expected<Builder> startRootNode(const MCSchema &Schema,
                                           StringRef KindString);
    static Expected<Builder> startNode(const MCSchema &Schema,
                                       StringRef KindString);

    Expected<MCObjectProxy> build();

  private:
    Error startNodeImpl(StringRef KindString);

    Builder(const MCSchema &Schema) : Schema(&Schema) {}
    const MCSchema *Schema;

  public:
    SmallString<256> Data;
    SmallVector<cas::ObjectRef, 16> Refs;
  };

private:
  const MCSchema *Schema;
};

/// Schema for a DAG in a CAS.
class MCSchema final : public RTTIExtends<MCSchema, MCFormatSchemaBase> {
  void anchor() override;

public:
  static char ID;
  Optional<StringRef> getKindString(const cas::ObjectProxy &Node) const;
  Optional<unsigned char> getKindStringID(StringRef KindString) const;

  cas::ObjectRef getRootNodeTypeID() const { return *RootNodeTypeID; }

  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  bool isRootNode(const cas::ObjectProxy &Node) const override;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  bool isNode(const cas::ObjectProxy &Node) const override;

  Expected<cas::ObjectProxy> createFromMCAssemblerImpl(
      llvm::MachOCASWriter &ObjectWriter, llvm::MCAssembler &Asm,
      const llvm::MCAsmLayout &Layout, raw_ostream *DebugOS) const override;

  Error serializeObjectFile(cas::ObjectProxy RootNode,
                            raw_ostream &OS) const override;

  MCSchema(cas::ObjectStore &CAS);

  Expected<MCObjectProxy> create(ArrayRef<cas::ObjectRef> Refs,
                                 StringRef Data) const {
    return MCObjectProxy::get(*this, CAS.createProxy(Refs, Data));
  }
  Expected<MCObjectProxy> get(cas::ObjectRef ID) const {
    return MCObjectProxy::get(*this, CAS.getProxy(ID));
  }

private:
  // Two-way map. Should be small enough for linear search from string to
  // index.
  SmallVector<std::pair<unsigned char, StringRef>, 16> KindStrings;

  // Optional as convenience for constructor, which does not return if it can't
  // fill this in.
  Optional<cas::ObjectRef> RootNodeTypeID;

  // Called by constructor. Not thread-safe.
  Error fillCache();
};

/// A type-checked reference to a node of a specific kind.
template <class DerivedT, class FinalT = DerivedT>
class SpecificRef : public MCObjectProxy {
protected:
  static Expected<DerivedT> get(Expected<MCObjectProxy> Ref) {
    if (auto Specific = getSpecific(std::move(Ref)))
      return DerivedT(*Specific);
    else
      return Specific.takeError();
  }

  static Expected<SpecificRef> getSpecific(Expected<MCObjectProxy> Ref) {
    if (!Ref)
      return Ref.takeError();
    if (Ref->getKindString() == FinalT::KindString)
      return SpecificRef(*Ref);
    return createStringError(inconvertibleErrorCode(),
                             "expected MC object '" + FinalT::KindString + "'");
  }

  static Optional<SpecificRef> Cast(MCObjectProxy Ref) {
    if (Ref.getKindString() == FinalT::KindString)
      return SpecificRef(Ref);
    return None;
  }

  SpecificRef(MCObjectProxy Ref) : MCObjectProxy(Ref) {}
};

#define CASV1_SIMPLE_DATA_REF(RefName, IdentifierName)                         \
  class RefName : public SpecificRef<RefName> {                                \
    using SpecificRefT = SpecificRef<RefName>;                                 \
    friend class SpecificRef<RefName>;                                         \
                                                                               \
  public:                                                                      \
    static constexpr StringLiteral KindString = #IdentifierName;               \
    static Expected<RefName> create(MCCASBuilder &MB, StringRef Data);         \
    static Expected<RefName> get(Expected<MCObjectProxy> Ref);                 \
    static Expected<RefName> get(const MCSchema &Schema, cas::ObjectRef ID) {  \
      return get(Schema.get(ID));                                              \
    }                                                                          \
    static Optional<RefName> Cast(MCObjectProxy Ref) {                         \
      auto Specific = SpecificRefT::Cast(Ref);                                 \
      if (!Specific)                                                           \
        return None;                                                           \
      return RefName(*Specific);                                               \
    }                                                                          \
    Expected<uint64_t> materialize(raw_ostream &OS) const {                    \
      OS << getData();                                                         \
      return getData().size();                                                 \
    }                                                                          \
                                                                               \
  private:                                                                     \
    explicit RefName(SpecificRefT Ref) : SpecificRefT(Ref) {}                  \
  };

#define CASV1_SIMPLE_GROUP_REF(RefName, IdentifierName)                        \
  class RefName : public SpecificRef<RefName> {                                \
    using SpecificRefT = SpecificRef<RefName>;                                 \
    friend class SpecificRef<RefName>;                                         \
                                                                               \
  public:                                                                      \
    static constexpr StringLiteral KindString = #IdentifierName;               \
    static Expected<RefName> create(MCCASBuilder &MB,                          \
                                    ArrayRef<cas::ObjectRef> IDs);             \
    static Expected<RefName> get(Expected<MCObjectProxy> Ref) {                \
      auto Specific = SpecificRefT::getSpecific(std::move(Ref));               \
      if (!Specific)                                                           \
        return Specific.takeError();                                           \
      return RefName(*Specific);                                               \
    }                                                                          \
    static Expected<RefName> get(const MCSchema &Schema, cas::ObjectRef ID) {  \
      return get(Schema.get(ID));                                              \
    }                                                                          \
    static Optional<RefName> Cast(MCObjectProxy Ref) {                         \
      auto Specific = SpecificRefT::Cast(Ref);                                 \
      if (!Specific)                                                           \
        return None;                                                           \
      return RefName(*Specific);                                               \
    }                                                                          \
    Expected<uint64_t> materialize(MCCASReader &Reader,                        \
                                   raw_ostream *Stream = nullptr) const;       \
                                                                               \
  private:                                                                     \
    explicit RefName(SpecificRefT Ref) : SpecificRefT(Ref) {}                  \
  };

#define MCFRAGMENT_NODE_REF(MCFragmentName, MCEnumName, MCEnumIdentifier)      \
  class MCFragmentName##Ref : public SpecificRef<MCFragmentName##Ref> {        \
    using SpecificRefT = SpecificRef<MCFragmentName##Ref>;                     \
    friend class SpecificRef<MCFragmentName##Ref>;                             \
                                                                               \
  public:                                                                      \
    static constexpr StringLiteral KindString = #MCEnumIdentifier;             \
    static Expected<MCFragmentName##Ref>                                       \
    create(MCCASBuilder &MB, const MCFragmentName &Fragment,                   \
           unsigned FragmentSize);                                             \
    static Expected<MCFragmentName##Ref> get(Expected<MCObjectProxy> Ref) {    \
      auto Specific = SpecificRefT::getSpecific(std::move(Ref));               \
      if (!Specific)                                                           \
        return Specific.takeError();                                           \
      return MCFragmentName##Ref(*Specific);                                   \
    }                                                                          \
    static Expected<MCFragmentName##Ref> get(const MCSchema &Schema,           \
                                             cas::ObjectRef ID) {              \
      return get(Schema.get(ID));                                              \
    }                                                                          \
    static Optional<MCFragmentName##Ref> Cast(MCObjectProxy Ref) {             \
      auto Specific = SpecificRefT::Cast(Ref);                                 \
      if (!Specific)                                                           \
        return None;                                                           \
      return MCFragmentName##Ref(*Specific);                                   \
    }                                                                          \
    Expected<uint64_t> materialize(MCCASReader &Reader,                        \
                                   raw_ostream *Stream) const;                 \
                                                                               \
  private:                                                                     \
    explicit MCFragmentName##Ref(SpecificRefT Ref) : SpecificRefT(Ref) {}      \
  };
#include "llvm/MC/CAS/MCCASObjectV1.def"

class PaddingRef : public SpecificRef<PaddingRef> {
  using SpecificRefT = SpecificRef<PaddingRef>;
  friend class SpecificRef<PaddingRef>;

public:
  static constexpr StringLiteral KindString = "mc:padding";

  static Expected<PaddingRef> create(MCCASBuilder &MB, uint64_t Size);

  static Expected<PaddingRef> get(Expected<MCObjectProxy> Ref);
  static Expected<PaddingRef> get(const MCSchema &Schema, cas::ObjectRef ID) {
    return get(Schema.get(ID));
  }
  static Optional<PaddingRef> Cast(MCObjectProxy Ref) {
    auto Specific = SpecificRefT::Cast(Ref);
    if (!Specific)
      return None;
    return PaddingRef(*Specific);
  }

  Expected<uint64_t> materialize(raw_ostream &OS) const;

private:
  explicit PaddingRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class MCAssemblerRef : public SpecificRef<MCAssemblerRef> {
  using SpecificRefT = SpecificRef<MCAssemblerRef>;
  friend class SpecificRef<MCAssemblerRef>;

public:
  static constexpr StringLiteral KindString = "mc:assembler";

  static Expected<MCAssemblerRef> get(Expected<MCObjectProxy> Ref);
  static Expected<MCAssemblerRef> get(const MCSchema &Schema,
                                      cas::ObjectRef ID) {
    return get(Schema.get(ID));
  }

  static Expected<MCAssemblerRef>
  create(const MCSchema &Schema, MachOCASWriter &ObjectWriter, MCAssembler &Asm,
         const MCAsmLayout &Layout, raw_ostream *DebugOS = nullptr);

  Error materialize(raw_ostream &OS) const;

  static Optional<MCAssemblerRef> Cast(MCObjectProxy Ref) {
    auto Specific = SpecificRefT::Cast(Ref);
    if (!Specific)
      return None;
    return MCAssemblerRef(*Specific);
  }

private:
  MCAssemblerRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class DebugInfoCURef : public SpecificRef<DebugInfoCURef> {
  using SpecificRefT = SpecificRef<DebugInfoCURef>;
  friend class SpecificRef<DebugInfoCURef>;

public:
  static constexpr StringLiteral KindString = "mc:debug_info_cu";
  static Expected<DebugInfoCURef> create(MCCASBuilder &MB, StringRef Data,
                                         ArrayRef<cas::ObjectRef> Refs);
  static Expected<DebugInfoCURef> get(Expected<MCObjectProxy> Ref);
  static Expected<DebugInfoCURef> get(const MCSchema &Schema,
                                      cas::ObjectRef ID) {
    return get(Schema.get(ID));
  }
  static Optional<DebugInfoCURef> Cast(MCObjectProxy Ref) {
    auto Specific = SpecificRefT::Cast(Ref);
    if (!Specific)
      return None;
    return DebugInfoCURef(*Specific);
  }
  Expected<uint64_t> materialize(raw_ostream &OS) const {
    OS << getData();
    return getData().size();
  }

private:
  explicit DebugInfoCURef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

struct DwarfSectionsCache {
  MCSection *DebugInfo;
  MCSection *Line;
  MCSection *Str;
  MCSection *Abbrev;
};
/// This struct represents the result of properly converting all the information
/// in the debug info section into cas objects. CURefs are the cas objects for
/// the compile units, AbbrevRefs are the cas objects for their abbreviation
/// contributions, AbbrevOffsetsRef is a cas object that contains the
/// abbreviation offset for every compile unit, and DebugDistinctDataRef is a
/// cas object contains the information from every compile unit that will not
/// deduplicate.
struct AbbrevAndDebugSplit {
  SmallVector<DebugInfoCURef> CURefs;
  SmallVector<DebugAbbrevRef> AbbrevRefs;
  Optional<DebugAbbrevOffsetsRef> AbbrevOffsetsRef;
  Optional<DebugInfoDistinctDataRef> DebugDistinctDataRef;
};

/// Helper class to allow reusing the logic of encoding/decoding Abbreviation
/// Offsets.
struct DebugAbbrevOffsetsRefAdaptor {
  DebugAbbrevOffsetsRefAdaptor(DebugAbbrevOffsetsRef Ref) : Ref(Ref) {}

  /// Decode the offsets inside the CAS object and return them.
  Expected<SmallVector<size_t>> decodeOffsets();

  /// Encode the `Offsets` vector into data suitable for creating a
  /// DebugAbbrevRef.
  static SmallVector<char> encodeOffsets(ArrayRef<size_t> Offsets);

private:
  DebugAbbrevOffsetsRef Ref;
};

/// Queries `Asm` for all dwarf sections and returns an object with (possibly
/// null) pointers to them.
DwarfSectionsCache getDwarfSections(MCAssembler &Asm);

/// Reads and returns the length field of a dwarf header contained in Reader,
/// assuming Reader is positioned at the beginning of the header. The Reader's
/// state is advanced to the first byte after the section.
Expected<size_t> getSizeFromDwarfHeaderAndSkip(BinaryStreamReader &Reader);

class MCCASBuilder {
public:
  cas::ObjectStore &CAS;
  MachOCASWriter &ObjectWriter;
  const MCSchema &Schema;
  MCAssembler &Asm;
  const MCAsmLayout &Layout;
  raw_ostream *DebugOS;

  MCCASBuilder(const MCSchema &Schema, MachOCASWriter &ObjectWriter,
               MCAssembler &Asm, const MCAsmLayout &Layout,
               raw_ostream *DebugOS)
      : CAS(Schema.CAS), ObjectWriter(ObjectWriter), Schema(Schema), Asm(Asm),
        Layout(Layout), DebugOS(DebugOS), FragmentOS(FragmentData),
        CurrentContext(&Sections), DwarfSections(getDwarfSections(Asm)) {}

  Error prepare();
  Error buildMachOHeader();
  Error buildFragments();
  Error buildRelocations();
  Error buildDataInCodeRegion();
  Error buildSymbolTable();

  void startGroup();
  Error finalizeGroup();

  void startSection(const MCSection *Sec);
  template <typename SectionRefTy = SectionRef> Error finalizeSection();

  void startAtom(const MCSymbol *Atom);
  Error finalizeAtom();

  void addNode(cas::ObjectProxy Node);
  const MCSymbol *getCurrentAtom() const { return CurrentAtom; }

  Error buildFragment(const MCFragment &F, unsigned FragmentSize);

  ArrayRef<MachO::any_relocation_info> getSectionRelocs() const {
    return SectionRelocs;
  }
  ArrayRef<MachO::any_relocation_info> getAtomRelocs() const {
    return AtomRelocs;
  }
  ArrayRef<MachObjectWriter::AddendsSizeAndOffset> getSectionAddends() const {
    return SectionAddends;
  }
  ArrayRef<MachObjectWriter::AddendsSizeAndOffset> getAtomAddends() const {
    return AtomAddends;
  }

  Optional<cas::ObjectRef>
  getObjectRefFromStringMap(unsigned StringOffset) const {
    auto StringRefIt =
        DebugStringSectionContents::MapOfStringRefs.find(StringOffset);
    if (StringRefIt != DebugStringSectionContents::MapOfStringRefs.end())
      return StringRefIt->getSecond();
    return None;
  }

  // Scratch space
  SmallString<8> FragmentData;
  raw_svector_ostream FragmentOS;

private:
  friend class MCAssemblerRef;

  Expected<SmallVector<char, 0>>
  mergeMCFragmentContents(const MCSection::FragmentListType &FragmentList,
                          bool IsDebugLineSection = false);

  // Helper functions.
  Error createStringSection(StringRef S,
                            std::function<Error(StringRef, unsigned)> CreateFn);

  // If a DWARF Line Section exists, create a DebugLineRef CAS object per
  // function contribution to the line table.
  Error createLineSection();

  /// If a DWARF Debug Info section exists, create a DebugInfoCURef CAS object
  /// for each compile unit (CU) inside the section, and a DebugAbbrevRef CAS
  /// object for the corresponding abbreviation section.
  /// A pair of vectors with the CAS objects is returned.
  /// The CAS objects appear in the same order as in the object file.
  /// If the section doesn't exist, an empty container is returned.
  Expected<AbbrevAndDebugSplit>
  splitDebugInfoAndAbbrevSections(ArrayRef<DebugStrRef> DebugStringRefs);

  /// If CURefs is non-empty, create a SectionRef CAS object with edges to all
  /// CURefs. Otherwise, no objects are created and `success` is returned.
  Error createDebugInfoSection(ArrayRef<DebugInfoCURef> CURefs,
                               DebugAbbrevOffsetsRef AbbrevOffsetsRef,
                               DebugInfoDistinctDataRef DebugDistinctDataRef);

  /// If AbbrevRefs is non-empty, create a SectionRef CAS object with edges to all
  /// AbbrevRefs. Otherwise, no objects are created and `success` is returned.
  Error createDebugAbbrevSection(ArrayRef<DebugAbbrevRef> AbbrevRefs);

  /// Split the Dwarf Abbrev section using `AbbrevOffsets` (possibly unsorted)
  /// as the split points for the section, creating one DebugAbbrevRef per
  /// _unique_ offset in the input.
  /// Returns a sequence of DebugAbbrevRefs, sorted by the order in which they
  /// should appear in the object file.
  Expected<SmallVector<DebugAbbrevRef>>
  splitAbbrevSection(ArrayRef<size_t> AbbrevOffsets,
                     ArrayRef<char> FullAbbrevData);

  struct DebugStringSectionContents {
    SmallVector<DebugStrRef, 0> DebugStringRefs;
    static DenseMap<unsigned, cas::ObjectRef> MapOfStringRefs;
  };

  Expected<DebugStringSectionContents> createDebugStringRefs();

  struct CUSplit {
    SmallVector<MutableArrayRef<char>> SplitCUData;
    SmallVector<size_t> AbbrevOffsets;
  };
  /// Split the data of the __debug_info section it into multiple pieces, one
  /// per Compile Unit(CU) and return them. The abbreviation offset for each CU
  /// is also returned.
  Expected<CUSplit>
  splitDebugInfoSectionData(MutableArrayRef<char> DebugInfoData);

  // If a DWARF String section exists, create a DebugStrRef CAS object per
  // string in the section.
  Error createDebugStrSection(ArrayRef<DebugStrRef> DebugStringRefs);

  /// If there is any padding between one section and the next, create a
  /// PaddingRef CAS object to represent the bytes of Padding between the two
  /// sections.
  Error createPaddingRef(const MCSection *Sec);

  const MCSection *CurrentSection = nullptr;
  const MCSymbol *CurrentAtom = nullptr;

  SmallVector<cas::ObjectRef> Sections, GroupContext, SectionContext,
      AtomContext;
  SmallVector<cas::ObjectRef> *CurrentContext;

  SmallVector<MachO::any_relocation_info> AtomRelocs;
  SmallVector<MachO::any_relocation_info> SectionRelocs;
  SmallVector<MachObjectWriter::AddendsSizeAndOffset> AtomAddends;
  SmallVector<MachObjectWriter::AddendsSizeAndOffset> SectionAddends;
  DenseMap<const MCFragment *, std::vector<MachO::any_relocation_info>> RelMap;

  DwarfSectionsCache DwarfSections;
};

class MCCASReader {
public:
  raw_ostream &OS;

  std::vector<std::vector<MachO::any_relocation_info>> Relocations;
  std::vector<MachObjectWriter::AddendsSizeAndOffset> Addends;

  MCCASReader(raw_ostream &OS, const Triple &Target, const MCSchema &Schema);
  support::endianness getEndian() {
    return Target.isLittleEndian() ? support::little : support::big;
  }

  Expected<MCObjectProxy> getObjectProxy(cas::ObjectRef ID) {
    auto Node = MCObjectProxy::get(Schema, Schema.CAS.getProxy(ID));
    if (!Node)
      return Node.takeError();
    return Node;
  }

  Triple::ArchType getArch() { return Target.getArch(); }

  Expected<uint64_t> materializeGroup(cas::ObjectRef ID);
  Expected<uint64_t> materializeSection(cas::ObjectRef ID, raw_ostream *Stream);
  Expected<uint64_t> materializeAtom(cas::ObjectRef ID, raw_ostream *Stream);

private:
  const Triple &Target;
  const MCSchema &Schema;
};

class DebugInfoSectionRef : public SpecificRef<DebugInfoSectionRef> {
  using SpecificRefT = SpecificRef<DebugInfoSectionRef>;
  friend class SpecificRef<DebugInfoSectionRef>;

public:
  static constexpr StringLiteral KindString = "mc:debug_info_section";
  static Expected<DebugInfoSectionRef> create(MCCASBuilder &MB,
                                              ArrayRef<cas::ObjectRef> IDs);
  static Expected<DebugInfoSectionRef> get(Expected<MCObjectProxy> Ref) {
    auto Specific = SpecificRefT::getSpecific(std::move(Ref));
    if (!Specific)
      return Specific.takeError();
    return DebugInfoSectionRef(*Specific);
  }
  static Expected<DebugInfoSectionRef> get(const MCSchema &Schema,
                                           cas::ObjectRef ID) {
    return get(Schema.get(ID));
  }
  static Optional<DebugInfoSectionRef> Cast(MCObjectProxy Ref) {
    auto Specific = SpecificRefT::Cast(Ref);
    if (!Specific)
      return None;
    return DebugInfoSectionRef(*Specific);
  }
  
  Expected<uint64_t>
  materialize(MCCASReader &Reader, ArrayRef<char> AbbrevSectionContents, ArrayRef<uint32_t> SecOffsetVals,
              DenseMap<cas::ObjectRef, unsigned> &MapOfStringOffsets,
              raw_ostream *Stream = nullptr) const;

private:
  explicit DebugInfoSectionRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

} // namespace v1
} // namespace mccasformats
} // namespace llvm

#endif // LLVM_MC_CAS_MCCASOBJECTV1_H
