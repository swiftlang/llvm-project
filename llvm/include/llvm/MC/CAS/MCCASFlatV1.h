//===- llvm/MC/CAS/MCCASFlatV1.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_CAS_MCCASFLATV1_H
#define LLVM_MC_CAS_MCCASFLATV1_H

#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASID.h"
#include "llvm/MC/CAS/MCCASFormatSchemaBase.h"
#include "llvm/MC/CAS/MCCASReader.h"
#include "llvm/MC/MCAsmLayout.h"

namespace llvm {
namespace mccasformats {
namespace flatv1 {

class MCSchema;
class MCCASBuilder;
class MCCASReader;

// FIXME: Using the same structure from FlatV1 from CASObjectFormat.
class MCNodeProxy : public cas::NodeProxy {
public:
  static Expected<MCNodeProxy> get(const MCSchema &Schema,
                                   Expected<cas::NodeProxy> Ref);
  StringRef getKindString() const;

  /// Return the data skipping the type-id character.
  StringRef getData() const { return cas::NodeProxy::getData().drop_front(); }

  const MCSchema &getSchema() const { return *Schema; }

  bool operator==(const MCNodeProxy &RHS) const {
    return Schema == RHS.Schema && cas::CASID(*this) == cas::CASID(RHS);
  }

  MCNodeProxy() = delete;

protected:
  MCNodeProxy(const MCSchema &Schema, const cas::NodeProxy &Node)
      : cas::NodeProxy(Node), Schema(&Schema) {}

  class Builder {
  public:
    static Expected<Builder> startRootNode(const MCSchema &Schema,
                                           StringRef KindString);
    static Expected<Builder> startNode(const MCSchema &Schema,
                                       StringRef KindString);

    Expected<MCNodeProxy> build();

  private:
    Error startNodeImpl(StringRef KindString);

    Builder(const MCSchema &Schema) : Schema(&Schema) {}
    const MCSchema *Schema;

  public:
    SmallString<256> Data;
    SmallVector<cas::CASID, 16> IDs;
  };

private:
  const MCSchema *Schema;
};

/// Schema for a DAG in a CAS.
class MCSchema final : public RTTIExtends<MCSchema, MCFormatSchemaBase> {
  void anchor() override;

public:
  static char ID;
  Optional<StringRef> getKindString(const cas::NodeProxy &Node) const;
  Optional<unsigned char> getKindStringID(StringRef KindString) const;

  cas::CASID getRootNodeTypeID() const { return *RootNodeTypeID; }

  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  bool isRootNode(const cas::NodeProxy &Node) const override;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  bool isNode(const cas::NodeProxy &Node) const override;

  Expected<cas::NodeProxy>
  createFromMCAssemblerImpl(llvm::MachOCASWriter &ObjectWriter,
                            llvm::MCAssembler &Asm,
                            const llvm::MCAsmLayout &Layout,
                            raw_ostream *DebugOS) const override;

  Error serializeObjectFile(cas::NodeProxy RootNode,
                            raw_ostream &OS) const override;

  MCSchema(cas::CASDB &CAS);

  Expected<MCNodeProxy> createNode(ArrayRef<cas::CASID> IDs,
                                   StringRef Data) const {
    return MCNodeProxy::get(*this, CAS.createNode(IDs, Data));
  }
  Expected<MCNodeProxy> getNode(cas::CASID ID) const {
    return MCNodeProxy::get(*this, CAS.getNode(ID));
  }

private:
  // Two-way map. Should be small enough for linear search from string to
  // index.
  SmallVector<std::pair<unsigned char, StringRef>, 16> KindStrings;

  // Optional as convenience for constructor, which does not return if it can't
  // fill this in.
  Optional<cas::CASID> RootNodeTypeID;

  // Called by constructor. Not thread-safe.
  Error fillCache();
};

/// A type-checked reference to a node of a specific kind.
template <class DerivedT, class FinalT = DerivedT>
class SpecificRef : public MCNodeProxy {
protected:
  static Expected<DerivedT> get(Expected<MCNodeProxy> Ref) {
    if (auto Specific = getSpecific(std::move(Ref)))
      return DerivedT(*Specific);
    else
      return Specific.takeError();
  }

  static Expected<SpecificRef> getSpecific(Expected<MCNodeProxy> Ref) {
    if (!Ref)
      return Ref.takeError();
    if (Ref->getKindString() == FinalT::KindString)
      return SpecificRef(*Ref);
    return createStringError(inconvertibleErrorCode(),
                             "expected MC object '" + FinalT::KindString +
                                 "'");
  }

  static Optional<SpecificRef> Cast(MCNodeProxy Ref) {
    if (Ref.getKindString() == FinalT::KindString)
      return SpecificRef(Ref);
    return None;
  }

  SpecificRef(MCNodeProxy Ref) : MCNodeProxy(Ref) {}
};

#define CASV1_SIMPLE_DATA_REF(RefName, IdentifierName)                         \
  class RefName : public SpecificRef<RefName> {                                \
    using SpecificRefT = SpecificRef<RefName>;                                 \
    friend class SpecificRef<RefName>;                                         \
                                                                               \
  public:                                                                      \
    static constexpr StringLiteral KindString = #IdentifierName;               \
    static Expected<RefName> create(MCCASBuilder &MB, StringRef Data);         \
    static Expected<RefName> get(Expected<MCNodeProxy> Ref);                   \
    static Expected<RefName> get(const MCSchema &Schema, cas::CASID ID) {      \
      return get(Schema.getNode(ID));                                          \
    }                                                                          \
    static Optional<RefName> Cast(MCNodeProxy Ref) {                           \
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
    static Expected<MCFragmentName##Ref> get(Expected<MCNodeProxy> Ref) {      \
      auto Specific = SpecificRefT::getSpecific(std::move(Ref));               \
      if (!Specific)                                                           \
        return Specific.takeError();                                           \
      return MCFragmentName##Ref(*Specific);                                   \
    }                                                                          \
    static Expected<MCFragmentName##Ref> get(const MCSchema &Schema,           \
                                             cas::CASID ID) {                  \
      return get(Schema.getNode(ID));                                          \
    }                                                                          \
    static Optional<MCFragmentName##Ref> Cast(MCNodeProxy Ref) {               \
      auto Specific = SpecificRefT::Cast(Ref);                                 \
      if (!Specific)                                                           \
        return None;                                                           \
      return MCFragmentName##Ref(*Specific);                                   \
    }                                                                          \
    Expected<uint64_t> materialize(MCCASReader &Reader) const;                 \
                                                                               \
  private:                                                                     \
    explicit MCFragmentName##Ref(SpecificRefT Ref) : SpecificRefT(Ref) {}      \
  };
#include "llvm/MC/CAS/MCCASFlatV1.def"

class PaddingRef : public SpecificRef<PaddingRef> {
  using SpecificRefT = SpecificRef<PaddingRef>;
  friend class SpecificRef<PaddingRef>;

public:
  static constexpr StringLiteral KindString = "mc:padding";

  static Expected<PaddingRef> create(MCCASBuilder &MB, uint64_t Size);

  static Expected<PaddingRef> get(Expected<MCNodeProxy> Ref);
  static Expected<PaddingRef> get(const MCSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }
  static Optional<PaddingRef> Cast(MCNodeProxy Ref) {
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

  static Expected<MCAssemblerRef> get(Expected<MCNodeProxy> Ref);
  static Expected<MCAssemblerRef> get(const MCSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  static Expected<MCAssemblerRef> create(const MCSchema &Schema,
                                         MachOCASWriter &ObjectWriter,
                                         MCAssembler &Asm,
                                         const MCAsmLayout &Layout,
                                         raw_ostream *DebugOS = nullptr);

  Error materialize(raw_ostream &OS) const;

private:
  MCAssemblerRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class MCCASBuilder {
public:
  cas::CASDB &CAS;
  MachOCASWriter &ObjectWriter;
  const MCSchema &Schema;
  MCAssembler &Asm;
  const MCAsmLayout &Layout;
  raw_ostream *DebugOS;

  MCCASBuilder(const MCSchema &Schema, MachOCASWriter &ObjectWriter,
               MCAssembler &Asm, const MCAsmLayout &Layout,
               raw_ostream *DebugOS)
      : CAS(Schema.CAS), ObjectWriter(ObjectWriter), Schema(Schema), Asm(Asm),
        Layout(Layout), DebugOS(DebugOS), FragmentOS(FragmentData) {}

  Error prepare();
  Error buildMachOHeader();
  Error buildFragments();
  Error buildRelocations();
  Error buildDataInCodeRegion();
  Error buildSymbolTable();

  void addNode(cas::NodeProxy Node);
  Error buildFragment(const MCFragment &F, unsigned FragmentSize);

  // Scratch space
  SmallString<8> FragmentData;
  raw_svector_ostream FragmentOS;
private:
  friend class MCAssemblerRef;

  unsigned SymTableSize = 0;
  DenseMap<cas::CASID, unsigned> CASIDMap;
  std::vector<unsigned> FragmentIDs;
  std::vector<cas::CASID> Fragments;
  SmallVector<cas::CASID, 4> Sections;
};

class MCCASReader {
public:
  raw_ostream &OS;

  MCCASReader(raw_ostream &OS, const Triple &Target, const MCSchema &Schema);
  bool isLittleEndian() const { return Target.isLittleEndian(); }

  Expected<uint64_t> materializeFragment(cas::CASID ID);
private:
  const Triple &Target;
  const MCSchema &Schema;
};

} // namespace flatv1
} // namespace mccasformats
} // namespace llvm

#endif // LLVM_MC_CAS_MCCASFLATV1_H
