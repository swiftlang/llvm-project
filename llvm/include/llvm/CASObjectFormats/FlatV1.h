//===- llvm/CASObjectFormats/FlatV1.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_FLATV1_H
#define LLVM_CASOBJECTFORMATS_FLATV1_H

#include "llvm/ADT/StringMap.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/CASObjectFormats/SchemaBase.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

namespace llvm {
namespace casobjectformats {
namespace flatv1 {

using data::Fixup;

class ObjectFileSchema;
class CompileUnitBuilder;
class LinkGraphBuilder;

/// FIXME: This is a copy from NestedV1 implementation. We should unify that.
class ObjectFormatNodeRef : public cas::NodeRef {
public:
  static Expected<ObjectFormatNodeRef> get(const ObjectFileSchema &Schema,
                                           Expected<cas::NodeRef> Ref);
  StringRef getKindString() const;

  /// Return the data skipping the type-id character.
  StringRef getData() const { return cas::NodeRef::getData().drop_front(); }

  const ObjectFileSchema &getSchema() const { return *Schema; }

  bool operator==(const ObjectFormatNodeRef &RHS) const {
    return Schema == RHS.Schema && cas::CASID(*this) == cas::CASID(RHS);
  }

  ObjectFormatNodeRef() = delete;

protected:
  ObjectFormatNodeRef(const ObjectFileSchema &Schema, const cas::NodeRef &Node)
      : cas::NodeRef(Node), Schema(&Schema) {}

  class Builder {
  public:
    static Expected<Builder> startRootNode(const ObjectFileSchema &Schema,
                                           StringRef KindString);
    static Expected<Builder> startNode(const ObjectFileSchema &Schema,
                                       StringRef KindString);

    Expected<ObjectFormatNodeRef> build();

  private:
    Error startNodeImpl(StringRef KindString);

    Builder(const ObjectFileSchema &Schema) : Schema(&Schema) {}
    const ObjectFileSchema *Schema;

  public:
    SmallString<256> Data;
    SmallVector<cas::CASID, 16> IDs;
  };

private:
  const ObjectFileSchema *Schema;
};

/// Schema for a DAG in a CAS.
///
/// The root nodes in the schema are entry points. Currently, that's just \a
/// CompileUnitRef. To recognize that the root node is part of the schema, the
/// first reference is used as a kind of type-id.
///
/// Sub-objects in the schema don't need to spend a reference on a type-id.
/// Instead, the first byte is stolen from \a getData().
///
/// The root node type-id is structured as:
class ObjectFileSchema final : public SchemaBase {
  void anchor() override;

public:
  Optional<StringRef> getKindString(const cas::NodeRef &Node) const;
  Optional<unsigned char> getKindStringID(StringRef KindString) const;

  cas::CASID getRootNodeTypeID() const { return *RootNodeTypeID; }

  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  bool isRootNode(const cas::NodeRef &Node) const override;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  bool isNode(const cas::NodeRef &Node) const override;

  Expected<cas::NodeRef>
  createFromLinkGraphImpl(const jitlink::LinkGraph &G,
                          raw_ostream *DebugOS) const override;

  Expected<std::unique_ptr<jitlink::LinkGraph>> createLinkGraphImpl(
      cas::NodeRef RootNode, StringRef Name,
      jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName,
      raw_ostream *DebugOS) const override;

  ObjectFileSchema(cas::CASDB &CAS);

  Expected<ObjectFormatNodeRef> createNode(ArrayRef<cas::CASID> IDs,
                                           StringRef Data) const {
    return ObjectFormatNodeRef::get(*this, CAS.createNode(IDs, Data));
  }
  Expected<ObjectFormatNodeRef> getNode(cas::CASID ID) const {
    return ObjectFormatNodeRef::get(*this, CAS.getNode(ID));
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
class SpecificRef : public ObjectFormatNodeRef {
protected:
  static Expected<DerivedT> get(Expected<ObjectFormatNodeRef> Ref) {
    if (auto Specific = getSpecific(std::move(Ref)))
      return DerivedT(*Specific);
    else
      return Specific.takeError();
  }

  static Expected<SpecificRef> getSpecific(Expected<ObjectFormatNodeRef> Ref) {
    if (!Ref)
      return Ref.takeError();
    if (Ref->getKindString() == FinalT::KindString)
      return SpecificRef(*Ref);
    return createStringError(inconvertibleErrorCode(),
                             "expected object kind '" + FinalT::KindString +
                                 "'");
  }

  SpecificRef(ObjectFormatNodeRef Ref) : ObjectFormatNodeRef(Ref) {}
};

class NameRef : public SpecificRef<NameRef> {
  using SpecificRefT = SpecificRef<NameRef>;
  friend class SpecificRef<NameRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:name";

  StringRef getName() const { return getData(); }

  static Expected<NameRef> create(CompileUnitBuilder &CUB, StringRef Name);
  static Expected<NameRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<NameRef> get(const ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

private:
  explicit NameRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class SectionRef : public SpecificRef<SectionRef> {
  using SpecificRefT = SpecificRef<SectionRef>;
  friend class SpecificRef<SectionRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:section";

  static Expected<SectionRef> create(CompileUnitBuilder &CUB,
                                     const jitlink::Section &S);
  static Expected<SectionRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SectionRef> get(const ObjectFileSchema &Schema,
                                  cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  Error materialize(LinkGraphBuilder &LGB, unsigned SectionIdx) const;

private:
  explicit SectionRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class BlockRef : public SpecificRef<BlockRef> {
  using SpecificRefT = SpecificRef<BlockRef>;
  friend class SpecificRef<BlockRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:block";

  static Expected<BlockRef> create(CompileUnitBuilder &CUB,
                                   const jitlink::Block &Block);

  static Expected<BlockRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<BlockRef> get(const ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  Error materializeBlock(LinkGraphBuilder &LGB, unsigned BlockIdx) const;
  Error materializeEdges(LinkGraphBuilder &LGB, unsigned BlockIdx) const;

private:
  explicit BlockRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class BlockContentRef : public SpecificRef<BlockContentRef> {
  using SpecificRefT = SpecificRef<BlockContentRef>;
  friend class SpecificRef<BlockContentRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:block-content";

  static Expected<BlockContentRef> create(CompileUnitBuilder &CUB,
                                          StringRef Content);

  static Expected<BlockContentRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<BlockContentRef> get(const ObjectFileSchema &Schema,
                                       cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  Error materialize(LinkGraphBuilder &LGB, unsigned BlockIdx) const;

private:
  explicit BlockContentRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class SymbolRef : public SpecificRef<SymbolRef> {
  using SpecificRefT = SpecificRef<SymbolRef>;
  friend class SpecificRef<SymbolRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:symbol";

  static Expected<SymbolRef> create(CompileUnitBuilder &CUB,
                                    const jitlink::Symbol &S);

  static Expected<SymbolRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SymbolRef> get(const ObjectFileSchema &Schema,
                                 cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  Error materialize(LinkGraphBuilder &LGB, unsigned SymbolIdx) const;

private:
  explicit SymbolRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// An object file / compile unit.
///
/// Note: This wrapper eagerly parses target triple, pointer size, and
/// endianness.
///
class CompileUnitRef : public SpecificRef<CompileUnitRef> {
  using SpecificRefT = SpecificRef<CompileUnitRef>;
  friend class SpecificRef<CompileUnitRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:compile-unit";

  /// Eagerly parse the full compile unit to create a LinkGraph.
  ///
  /// Maybe \a LinkGraph isn't really the right interface. Building one forces
  /// us to eagerly parse the full object file, defeating some of the point of
  /// this format.
  ///
  /// Ideally we'd have some sort of LazyLinkGraph that can answer questions
  /// and/or build itself up on demand.
  Expected<std::unique_ptr<jitlink::LinkGraph>>
  createLinkGraph(StringRef Name,
                  jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName);

  static Expected<CompileUnitRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<CompileUnitRef> get(const ObjectFileSchema &Schema,
                                      cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  static Expected<CompileUnitRef> create(const ObjectFileSchema &Schema,
                                         const jitlink::LinkGraph &G,
                                         raw_ostream *DebugOS = nullptr);

  Error materialize(LinkGraphBuilder &LGB) const;

private:
  CompileUnitRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

class CompileUnitBuilder {
public:
  cas::CASDB &CAS;
  const ObjectFileSchema &Schema;
  Triple TT;
  raw_ostream *DebugOS;

  CompileUnitBuilder(const ObjectFileSchema &Schema, Triple Target,
                     raw_ostream *DebugOS)
      : CAS(Schema.CAS), Schema(Schema), TT(Target), DebugOS(DebugOS),
        TargetInfoPool(TargetInfoAlloc) {}

  Error createAndReferenceName(StringRef S);
  Error createAndReferenceContent(StringRef Content);

  Error createSection(const jitlink::Section &S);
  Error createBlock(const jitlink::Block &B);
  Error createSymbol(const jitlink::Symbol &S);

  // Lookup functions. The result must exist in the cache already.
  Expected<unsigned> getSectionIndex(const jitlink::Section &S);
  Expected<unsigned> getBlockIndex(const jitlink::Block &B);
  Expected<unsigned> getSymbolIndex(const jitlink::Symbol &S);

  // Encode the index into compile unit.
  void encodeIndex(unsigned Index);

  // Encode a target info list and return the offset.
  Expected<size_t> encodeTargetInfo(ArrayRef<const jitlink::Edge *> Edges);

private:
  friend class CompileUnitRef;

  unsigned recordNode(const ObjectFormatNodeRef &Ref);
  unsigned commitNode(const ObjectFormatNodeRef &Ref);
  void pushNodes();

  // Cache all the CASID created.
  DenseMap<cas::CASID, unsigned> CASIDMap;
  std::vector<cas::CASID> IDs;

  // Index Storage.
  std::vector<unsigned> Indexes;
  SmallVector<unsigned, 16> LocalIndexStorage;

  // Lookup cache.
  SmallVector<const jitlink::Section *, 16> Sections;
  SmallVector<const jitlink::Symbol *, 16> Symbols;
  SmallVector<const jitlink::Block *, 16> Blocks;
  std::vector<unsigned> BlockIndexStarts;

  // TargetInfoList storage.
  SmallString<256> FlatTargetInfo;
  BumpPtrAllocator TargetInfoAlloc;
  StringMap<uint32_t, BumpPtrAllocator> TargetInfoPool;

  // Temp storage.
  SmallString<256> InlineBuffer;
};

class LinkGraphBuilder {
public:
  LinkGraphBuilder(StringRef Name,
                   jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName,
                   CompileUnitRef Root)
      : Name(Name.str()), GetEdgeKindName(GetEdgeKindName), Root(Root) {}

  std::unique_ptr<jitlink::LinkGraph> takeLinkGraph() { return std::move(LG); }

  Error materializeCompileUnit();
  Error materializeSections();
  Error materializeBlockContents();
  Error materializeSymbols();
  Error materializeEdges();

  Expected<unsigned> nextIdx() {
    if (CurrentIdx >= Indexes.size())
      return createStringError(inconvertibleErrorCode(), "Index out of bound");

    return Indexes[CurrentIdx++];
  }

  unsigned nextIdxForBlock(unsigned BlockIndex) {
    return Indexes[Blocks[BlockIndex].BlockIdx++];
  }

  Expected<cas::CASID> nextID() {
    auto Idx = nextIdx();
    if (!Idx)
      return Idx.takeError();

    // First ID is the RootTypeID
    auto IDIndex = *Idx + 1;
    if (IDIndex >= IDs.size())
      return createStringError(inconvertibleErrorCode(),
                               "ID Index out of bound");

    return IDs[IDIndex];
  }

  template <typename RefT> Expected<RefT> nextNode() {
    auto ID = nextID();
    if (!ID)
      return ID.takeError();
    return RefT::get(Root.getSchema(), *ID);
  }

  template <typename RefT> Expected<RefT> getNode(unsigned Idx) {
    auto IDIndex = nextIdxForBlock(Idx) + 1;
    if (IDIndex >= IDs.size())
      return createStringError(inconvertibleErrorCode(),
                               "ID Index out of bound");
    return RefT::get(Root.getSchema(), IDs[IDIndex]);
  }

  jitlink::LinkGraph *getLinkGraph() const { return LG.get(); }

  struct SectionInfo {
    jitlink::Section *Section;
    uint64_t Size = 0;
    uint64_t Alignment = 1;
  };

  Error addSection(unsigned SectionIdx, jitlink::Section *S);
  Expected<SectionInfo &> getSectionInfo(unsigned SectionIdx);

  struct BlockInfo {
    jitlink::Block *Block;
    Optional<BlockRef> Ref;
    Optional<data::FixupList> Fixups;
    unsigned BlockIdx = 0;
  };

  Expected<BlockInfo &> getBlockInfo(unsigned BlockIdx);

  Error addSymbol(unsigned SymbolIdx, jitlink::Symbol *S);
  Expected<jitlink::Symbol *> getSymbol(unsigned SymbolIdx);

  /// Returns a list starting from \p Start, but that iterates past the end of
  /// the edges.
  data::TargetInfoList getTargetInfoFrom(unsigned Start);

private:
  friend class CompileUnitRef;
  std::string Name;
  jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName;
  CompileUnitRef Root;

  std::unique_ptr<jitlink::LinkGraph> LG;
  unsigned CurrentIdx = 0;
  std::vector<cas::CASID> IDs;
  std::vector<unsigned> Indexes;

  unsigned SectionsSize;
  unsigned SymbolsSize;
  unsigned BlocksSize;

  // Target info.
  StringRef FlatTargetInfo;

  // Lookup Cache.
  SmallVector<SectionInfo, 16> Sections;
  SmallVector<BlockInfo, 16> Blocks;
  SmallVector<jitlink::Symbol *> Symbols;

  StringRef InlineBuffer;
};

} // namespace flatv1
} // namespace casobjectformats
} // namespace llvm

#endif // LLVM_CASOBJECTFORMATS_FLATV1_H
