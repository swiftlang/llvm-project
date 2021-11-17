//===- llvm/CASObjectFormats/NestedV1.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_NESTEDV1_H
#define LLVM_CASOBJECTFORMATS_NESTEDV1_H

#include "llvm/CAS/CASDB.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/CASObjectFormats/SchemaBase.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

namespace llvm {
namespace casobjectformats {
namespace nestedv1 {

using data::Fixup;
using data::FixupList;
using data::TargetInfo;
using data::TargetInfoList;

class ObjectFileSchema;

/// An object in "cas.o:", using the first CAS reference as a type-id. This
/// type-id is useful for error checking and collecting statistics.
///
/// FIXME: Consider using the first 8B (or 1B!) of the data for the type-id
/// instead... or, drop the type-id entirely except when it's needed to
/// distinguish the type of a referenced object. (Note that dropping the
/// type-id would break \a getKindString().)
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

  static Expected<NameRef> create(const ObjectFileSchema &Schema,
                                  StringRef Name);
  static Expected<NameRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<NameRef> get(const ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

private:
  explicit NameRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// A section.
///
/// Note: the name is stored separately since in some formats there are
/// collisions between symbol names and section names.
///
/// FIXME: Should the section name be split up in two? Maybe optionally?
/// The question is really about what happens with COMDATs (and whether this
/// format cares about optimizing for COMDATs). If COMDAT section names are
/// equal to symbol names, then this is already giving us deduplication. If
/// section names are some segment recommendation with the symbol name
/// appended, maybe we should split it up... or store the segment name part
/// inline... or something.
///
/// FIXME: Should the section object (optionally) embed its name directly?
/// This would not capitalize on redundancy between symbol names and section
/// names, maybe that doesn't matter in practice. Or maybe it only matters
/// sometimes. Not clear.
///
/// FIXME: Should the section object (optionally) be embedded in its users?
class SectionRef : public SpecificRef<SectionRef> {
  using SpecificRefT = SpecificRef<SectionRef>;
  friend class SpecificRef<SectionRef>;

public:
  // FIXME: Support "huge" bit?
  static constexpr StringLiteral KindString = "cas.o:section";

  cas::CASID getNameID() const { return getReference(0); }
  Expected<NameRef> getName() const {
    return NameRef::get(getSchema(), getNameID());
  }
  jitlink::MemProt getMemProt() const;

  static Expected<SectionRef> create(const ObjectFileSchema &Schema,
                                     NameRef SectionName,
                                     jitlink::MemProt MemProt);
  static Expected<SectionRef> create(const ObjectFileSchema &Schema,
                                     const jitlink::Section &S);

  static Expected<SectionRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SectionRef> get(const ObjectFileSchema &Schema,
                                  cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

private:
  explicit SectionRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// Raw content of a block.
///
/// - A leaf if it's zero-fill.
/// - Points at a blob for the content if it's not zero-fill.
/// - Directly stores size, alignment, and alignment offset.
///
/// TODO: More carefully evaluate trade-offs of storing the content out-of-line
/// vs. inline. It'd be easy enough to store it directly after the fields that
/// are here.
class BlockDataRef : public SpecificRef<BlockDataRef> {
  using SpecificRefT = SpecificRef<BlockDataRef>;
  friend class SpecificRef<BlockDataRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:block-data";

  bool isZeroFill() const { return Data.isZeroFill(); }
  uint64_t getSize() const { return Data.getSize(); }
  uint64_t getAlignment() const { return Data.getAlignment(); }
  uint64_t getAlignmentOffset() const { return Data.getAlignmentOffset(); }
  FixupList getFixups() const { return Data.getFixups(); }
  Optional<StringRef> getContent() const { return Data.getContent(); }
  Optional<ArrayRef<char>> getContentArray() const {
    return Data.getContentArray();
  }

  static Expected<BlockDataRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<BlockDataRef> get(const ObjectFileSchema &Schema,
                                    cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  static Expected<BlockDataRef> createZeroFill(const ObjectFileSchema &Schema,
                                               uint64_t Size,
                                               uint64_t Alignment,
                                               uint64_t AlignmentOffset,
                                               ArrayRef<Fixup> Fixups);

  static Expected<BlockDataRef> createContent(const ObjectFileSchema &Schema,
                                              StringRef Content,
                                              uint64_t Alignment,
                                              uint64_t AlignmentOffset,
                                              ArrayRef<Fixup> Fixups);

  static Expected<BlockDataRef> create(const ObjectFileSchema &Schema,
                                       const jitlink::Block &Block,
                                       ArrayRef<Fixup> Fixups);

private:
  data::BlockData Data;
  explicit BlockDataRef(SpecificRefT Ref, data::BlockData Data)
      : SpecificRefT(Ref), Data(Data) {}

  static Expected<BlockDataRef> createImpl(const ObjectFileSchema &Schema,
                                           Optional<StringRef> Content,
                                           uint64_t Size, uint64_t Alignment,
                                           uint64_t AlignmentOffset,
                                           ArrayRef<Fixup> Fixups);
};

/// A variant of SymbolRef and IndirectSymbolRef. The kind is cached.
class SymbolRef;
class TargetRef {
public:
  enum Kind {
    Symbol,
    IndirectSymbol,
  };

  Kind getKind() const { return K; }
  const ObjectFileSchema &getSchema() const { return *Schema; }
  cas::CASID getID() const { return ID; }
  operator cas::CASID() const { return getID(); }

  Expected<Optional<NameRef>> getName() const;

  /// Get the name, which may be empty.
  Expected<StringRef> getNameString() const {
    if (auto Name = getName())
      return *Name ? (*Name)->getName() : "";
    else
      return Name.takeError();
  }

  /// Get a \a TargetRef. If \c Kind is specified, returns an error on
  /// mismatch; otherwise just requires that it's a valid target.
  static Expected<TargetRef> get(const ObjectFileSchema &Schema, cas::CASID ID,
                                 Optional<Kind> ExpectedKind = None);

  static TargetRef getIndirectSymbol(const ObjectFileSchema &Schema,
                                     NameRef Ref) {
    return TargetRef(Schema, Ref, IndirectSymbol);
  }
  static TargetRef getSymbol(const ObjectFileSchema &Schema,
                             const SymbolRef &Ref);

private:
  TargetRef() = delete;
  TargetRef(const ObjectFileSchema &Schema, cas::CASID ID, Kind K)
      : Schema(&Schema), ID(ID), K(K) {}

  const ObjectFileSchema *Schema;
  cas::CASID ID;
  Kind K;
};

/// An array of targets.
class TargetList {
public:
  bool empty() const { return !size(); }
  size_t size() const { return Last ? Last - First : Node ? 1 : 0; }

  Expected<TargetRef> operator[](size_t I) const { return get(I); }
  Expected<TargetRef> get(size_t I) const {
    assert(I < size() && "past the end");
    return TargetRef::get(Node->getSchema(), Node->getReference(I + First));
  }

  TargetList() = default;
  explicit TargetList(ObjectFormatNodeRef Node, size_t First, size_t Last)
      : Node(Node), First(First), Last(Last) {
    assert(Last == this->Last && "Unexpected overflow");
  }

private:
  Optional<ObjectFormatNodeRef> Node;
  uint32_t First = 0;
  uint32_t Last = 0;
};

/// An array of targets.
///
/// FIXME: Consider appending to \a BlockRef's references when there is only
/// one target, only using a separate object when there are at least two.
class TargetListRef : public SpecificRef<TargetListRef> {
  using SpecificRefT = SpecificRef<TargetListRef>;
  friend class SpecificRef<TargetListRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:target-list";

  size_t getNumTargets() const { return getNumReferences(); }

  TargetList getTargets() const {
    return TargetList(*this, 0, getNumTargets());
  }

  /// Create the given target list. Does not sort the targets, since it's
  /// assumed the order is already relevant.
  static Expected<TargetListRef> create(const ObjectFileSchema &Schema,
                                        ArrayRef<TargetRef> Targets);

  static Expected<TargetListRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<TargetListRef> get(const ObjectFileSchema &Schema,
                                     cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

private:
  explicit TargetListRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// A variant of SymbolRef, IndirectSymbolRef, and BlockRef.
class SymbolDefinitionRef : public ObjectFormatNodeRef {
  friend class BlockRef;
  friend class SymbolRef;
  friend class IndirectSymbolRef;

public:
  enum Kind {
    Alias,         // Points at a SymbolRef.
    IndirectAlias, // Points at an IndirectSymbolRef.
    Block,         // Points at a BlockRef.
  };

  Kind getKind() const { return K; }

  static StringRef getKindString(Kind K);
  StringRef getKindString() const { return getKindString(K); }

  /// Get a \a SymbolDefinitionRef. If \c K is specified, returns an error on
  /// mismatch; otherwise just requires that it's a valid target.
  static Expected<SymbolDefinitionRef> get(Expected<ObjectFormatNodeRef> Ref,
                                           Optional<Kind> ExpectedKind = None);
  static Expected<SymbolDefinitionRef> get(const ObjectFileSchema &Schema,
                                           cas::CASID ID,
                                           Optional<Kind> ExpectedKind = None) {
    return get(Schema.getNode(ID), ExpectedKind);
  }

private:
  SymbolDefinitionRef(ObjectFormatNodeRef Ref, Kind K)
      : ObjectFormatNodeRef(Ref), K(K) {}
  Kind K;
};

/// A block.
///
/// Blocks are stored as:
///
/// - section: section name and permissions.
/// - data: size, alignment, and content.
/// - edges: the fixups and targets.
///
/// Determining if blocks can be shared by-content means checking:
///
/// - 'section' is equivalent (FIXME: is this too strict?)
/// - 'data' is equivalent
/// - 'edges' is compatible:
///     - 'edges.fixups' is equivalent
///     - 'edges.targets' is compatible, as in it's equivalent after
///       recursively resolving symbols
///
/// FIXME: Does the section name really need to be the same to share blocks
/// by-content? Or should the section be split apart somehow? (In particular,
/// consider object formats that use COMDATs, where every symbol is in its own
/// section.)
///
/// FIXME: In the CAS-to-Mach-O converter, some sections will need semantic
/// sorting of blocks (e.g., EH frames: CFEs before FDEs). Maybe this could be
/// modelled with a bit to indicate layout should be post-order?
///
/// FIXME: Hide 'section', 'block-data', 'fixup-list', 'target-info-list', and
/// 'target-list' from the public API of \a BlockRef, making the schema an
/// implementation detail. This allows them to be encoded as data (instead of
/// expensive CAS object references) when they are small. E.g.:
///
/// - Inline single-element target lists, storing the target directly rather
///   than adding an unnecessary indirection.
///
/// - Evaluate storing fixup-list and target-info-list as data when they are
///   small "enough". Tight encodings will often cost only a few bytes, whereas
///   a CAS reference is at least the size of a hash. A simple rule: store
///   inline if there are only 8 (or fewer) fixups.
///
/// - Evaluate storing small block-data objects inline as pure data (especially
///   zero-fill). The size, alignment, and alignment-offset can be stored
///   compactly, and if the content is only a few bytes the overhead of a CAS
///   reference is unnecessary. A simple rule: store inline whenever the
///   content is only 16B (or smaller).
///
/// - Consider sinking 'section' down to 'block-data' (maybe even inlining it
///   there). This reduces the size of 'block'. A bad idea if we support
///   ELF/COFF section names in 'section' -- especially COMDATs -- since that
///   will explode the number of 'block-data'... but probably COMDATs and
///   function sections should be modelled at a higher level, not even
///   referenced by 'block'.
///
/// Note: the primary goal is not to avoid creation of small objects, but to
/// remove indirection.
class BlockRef : public SpecificRef<BlockRef> {
  using SpecificRefT = SpecificRef<BlockRef>;
  friend class SpecificRef<BlockRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:block";

  bool hasEdges() const { return Flags.HasEdges; }
  bool hasAbstractBackedge() const { return Flags.HasAbstractBackedge; }

  cas::CASID getSectionID() const { return getReference(0); }
  cas::CASID getDataID() const { return getReference(1); }

private:
  Optional<size_t> getTargetsIndex() const;
  Optional<cas::CASID> getTargetInfoID() const;

public:
  Expected<SectionRef> getSection() const {
    return SectionRef::get(getSchema(), getSectionID());
  }
  Expected<BlockDataRef> getBlockData() const {
    return BlockDataRef::get(getSchema(), getDataID());
  }
  Expected<FixupList> getFixups() const;
  Expected<TargetInfoList> getTargetInfo() const;
  Expected<TargetList> getTargets() const;

  static Expected<BlockRef>
  create(const ObjectFileSchema &Schema, const jitlink::Block &Block,
         function_ref<Expected<Optional<TargetRef>>(const jitlink::Symbol &)>
             GetTargetRef);

  static Expected<BlockRef> create(const ObjectFileSchema &Schema,
                                   SectionRef Section, BlockDataRef Data) {
    return createImpl(Schema, Section, Data, None, None);
  }
  static Expected<BlockRef> create(const ObjectFileSchema &Schema,
                                   SectionRef Section, BlockDataRef Data,
                                   ArrayRef<TargetInfo> TargetInfo,
                                   ArrayRef<TargetRef> Targets) {
    return createImpl(Schema, Section, Data, TargetInfo, Targets);
  }

  static Expected<BlockRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<BlockRef> get(const ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  SymbolDefinitionRef getAsSymbolDefinition() const {
    return SymbolDefinitionRef(*this, SymbolDefinitionRef::Block);
  }

private:
  struct BlockFlags {
    bool HasEdges = false;
    bool HasTargets = false;
    bool HasTargetInline = false;
    bool HasAbstractBackedge = false;
    bool HasEmbeddedTargetInfo = false;
  };
  BlockFlags Flags;

  explicit BlockRef(SpecificRefT Ref) : SpecificRefT(Ref) {}

  static Expected<BlockRef> createImpl(const ObjectFileSchema &Schema,
                                       SectionRef Section, BlockDataRef Data,
                                       ArrayRef<TargetInfo> TargetInfo,
                                       ArrayRef<TargetRef> Targets);
};

/// A symbol.
///
/// FIXME: jitlink::Symbol has a few fields not here (yet).
///
/// - Size: ELF-specific, and not used for anything. Size of the symbol.
/// - IsCallable: cache of EXEC bit on the block.
class SymbolRef : public SpecificRef<SymbolRef> {
  using SpecificRefT = SpecificRef<SymbolRef>;
  friend class SpecificRef<SymbolRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:symbol";

  /// Semantics for dead-stripping.
  enum DeadStripKind {
    DS_Never,       // Symbols with "used" attribute.
    DS_CompileUnit, // Other "linkonce", "internal", and "private" symbols.
    DS_LinkUnit,    // Other symbols.
    DS_Max = DS_LinkUnit,
  };

  /// Semantics for exporting.
  enum ScopeKind {
    S_Local,  // Symbols with "internal" or "private".
    S_Hidden, // Symbols with "hidden".
    S_Global, // Other (defined) symbols.
    S_Max = S_Global,
  };

  /// Semantics for sharing blocks or merging with other symbols.
  enum MergeKind {
    M_Never = 0,     // Other symbols (see below).
    M_ByName = 1,    // "weak" or "linkonce" symbols.
    M_ByContent = 2, // Symbols with "unnamed_addr" attribute.
    M_ByNameOrContent = M_ByName | M_ByContent,
    M_Max = M_ByNameOrContent,
  };

  /// The various axes of linkage.
  struct Flags {
    DeadStripKind DeadStrip = DS_Never;
    ScopeKind Scope = S_Local;
    MergeKind Merge = M_Never;

    bool operator==(const Flags &RHS) const {
      return DeadStrip == RHS.DeadStrip && Scope == RHS.Scope &&
             Merge == RHS.Merge;
    }
    bool operator!=(const Flags &RHS) const { return !operator==(RHS); }
    bool operator<(const Flags &RHS) const {
      if (DeadStrip < RHS.DeadStrip)
        return true;
      if (DeadStrip > RHS.DeadStrip)
        return false;
      if (Scope < RHS.Scope)
        return true;
      if (Scope > RHS.Scope)
        return false;
      return Merge < RHS.Merge;
    }

    Flags() = default;
    Flags(DeadStripKind DeadStrip, ScopeKind Scope, MergeKind Merge)
        : DeadStrip(DeadStrip), Scope(Scope), Merge(Merge) {}
  };

  static Flags getFlags(const jitlink::Symbol &S);

  /// Anonymous symbols don't have names.
  bool hasName() const { return getNumReferences() > 1; }

  /// True if this symbol is a template for a symbol. This is true if its
  /// definition has an abstract backedge.
  bool isSymbolTemplate() const;

  uint64_t getOffset() const { return Offset; }

  Flags getFlags() const {
    return Flags(getDeadStrip(), getScope(), getMerge());
  }

  DeadStripKind getDeadStrip() const {
    return (DeadStripKind)(((unsigned char)getData()[0] >> 6) & 0x3);
  }
  ScopeKind getScope() const {
    return (ScopeKind)(((unsigned char)getData()[0] >> 4) & 0x3);
  }
  MergeKind getMerge() const {
    return (MergeKind)(((unsigned char)getData()[0] >> 2) & 0x3);
  }

  cas::CASID getDefinitionID() const { return getReference(0); }
  Optional<cas::CASID> getNameID() const {
    return hasName() ? getReference(1) : Optional<cas::CASID>();
  }

  Expected<SymbolDefinitionRef> getDefinition() const {
    return SymbolDefinitionRef::get(getSchema().getNode(getDefinitionID()));
  }
  Expected<Optional<NameRef>> getName() const {
    if (!hasName())
      return None;
    return NameRef::get(getSchema(), *getNameID());
  }

  static Expected<SymbolRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SymbolRef> get(const ObjectFileSchema &Schema,
                                 cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  TargetRef getAsTarget() const {
    return TargetRef::getSymbol(getSchema(), *this);
  }
  Expected<TargetRef> getAsIndirectTarget() const;

  static Expected<SymbolRef> create(const ObjectFileSchema &Schema,
                                    Optional<NameRef> SymbolName,
                                    SymbolDefinitionRef Definition,
                                    uint64_t Offset, Flags F);

  static Expected<SymbolRef>
  create(const ObjectFileSchema &Schema, const jitlink::Symbol &S,
         function_ref<Expected<SymbolDefinitionRef>(const jitlink::Block &)>
             GetDefinitionRef);

private:
  uint64_t Offset;
  explicit SymbolRef(SpecificRefT Ref, uint64_t Offset)
      : SpecificRefT(Ref), Offset(Offset) {}
};

inline TargetRef TargetRef::getSymbol(const ObjectFileSchema &Schema,
                                      const SymbolRef &Ref) {
  return TargetRef(Schema, Ref, Symbol);
}

/// A symbol table.
class SymbolTableRef : public SpecificRef<SymbolTableRef> {
  using SpecificRefT = SpecificRef<SymbolTableRef>;
  friend class SpecificRef<SymbolTableRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:symbol-table";

  size_t getNumAnonymousSymbols() const { return NumAnonymousSymbols; }
  size_t getNumNamedSymbols() const {
    return getNumSymbols() - getNumAnonymousSymbols();
  }
  size_t getNumSymbols() const { return getNumReferences(); }
  cas::CASID getSymbolID(size_t I) const { return getReference(I); }

  Expected<SymbolRef> getSymbol(size_t I) const {
    return SymbolRef::get(getSchema().getNode(getSymbolID(I)));
  }

  Expected<Optional<SymbolRef>> lookupSymbol(NameRef Name) const;

  static Expected<SymbolTableRef> create(const ObjectFileSchema &Schema,
                                         ArrayRef<SymbolRef> Symbols);

  static Expected<SymbolTableRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SymbolTableRef> get(const ObjectFileSchema &Schema,
                                      cas::CASID CASID) {
    return get(Schema.getNode(CASID));
  }

private:
  explicit SymbolTableRef(SpecificRefT Ref, size_t NumAnonymousSymbols)
      : SpecificRefT(Ref), NumAnonymousSymbols(NumAnonymousSymbols) {}

  size_t NumAnonymousSymbols;
};

/// A list of (symbol) names.
class NameListRef : public SpecificRef<NameListRef> {
  using SpecificRefT = SpecificRef<NameListRef>;
  friend class SpecificRef<NameListRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:name-list";

  size_t getNumNames() const { return getNumReferences(); }
  cas::CASID getNameID(size_t I) const { return getReference(I); }
  Expected<NameRef> getName(size_t I) const {
    return NameRef::get(getSchema(), getNameID(I));
  }

  static Expected<NameListRef> create(const ObjectFileSchema &Schema,
                                      MutableArrayRef<NameRef> Names);

  static Expected<NameListRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<NameListRef> get(const ObjectFileSchema &Schema,
                                   cas::CASID CASID) {
    return get(Schema.getNode(CASID));
  }

private:
  explicit NameListRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// An object file / compile unit.
///
/// Note: This wrapper eagerly parses target triple, pointer size, and
/// endianness.
///
/// FIXME: There isn't much dedup going on with target lists and
/// `--inline-unary-target-lists` has almost no effect. Target lists are
/// bloated by the KeepAlive edges to `__eh_frame` blocks. This prevents
/// sharing target-lists (even if two functions have the same callees) and
/// means that almost no block can inline its target-list. Supporting
/// `__compact_unwind` properly will make this even worse since it'll add more
/// KeepAlive edges.
///
/// We should probably do a more direct translation of the object format,
/// which isn't trying to do implicit dead-stripping. If we want to support
/// bit-for-bit round-trip back to Mach-O, something like could be needed
/// anyway.
///
/// Currently we have:
///
///     compile-unit
///       3x symbol table (skipping many dead-strippable symbols)
///         symbol
///           block
///             block-data
///             section
///             target-list (including KeepAlive)
///
/// Instead we could have:
///
///     compile-unit
///       section (referencing all defined symbols)
///         symbol
///           block
///             block-data
///             target-list (skipping KeepAlive)
///
/// ... but this relayering makes it hard to point at data symbols
/// anonymously. The data could be listed explicitly by their section, but
/// then referring to them either requires identifying somehow (e.g.,
/// generating a name) or a combination of section+symbol.
///
/// ... and this relayering defeats a major optimization for `__ehframe` that
/// dedups individual blocks by using a "template", which we also want to apply
/// to `__compact_unwind`.
///
/// Here's another option, where:
///
/// - Sections sometimes point at symbols
/// - Symbols sometimes point at sections
///
/// Split into three for clarity, starting with compile-unit:
///
///     compile-unit
///       section
///         section-header: identifier for section (e.g., `__text`)
///         symbol-runs: list of symbols, organized as a flat list of
///             references, interspersing a "real" symbol with a run of
///             blocks kept alive with it (possibly from other sections).
///           keep-alive-info: list of sections for the run of keep-alive
///               blocks (e.g., `__compact_unwind`, `__eh_frame`), along
///               with any metadata for building symbols from the blocks.
///             section-header: indirect reference to the section for the
///                 block contained in the list of symbols
///           symbol: "real" symbol, followed by a run of blocks (depending
///               on keep-alive info)
///           block: anonymous symbol kept alive by the preceding symbol,
///               stored in the section implied by context
///
/// Blocks reference symbols as before:
///
///     block
///       block-data
///         fixup-list
///         content
///       target-info-list
///       target-list: list of targets, which are a variant of one of
///         indirect-symbol (name-based reference)
///         symbol          (alias with direct reference)
///
/// Anonymous symbols/data can still be directly referenced to avoid
/// requiring an "identity" for them. In that case they need a section
/// header.
///
/// Symbols look like:
///
///     symbol
///       section-header: (optional) for anonymous symbols, to identify
///           the section it needs to go in / how to create it
///       symbol-definition: which is a variant of
///         block           (usual case)
///         indirect-symbol (alias)
///         symbol          (alias with direct reference)
///
/// In summary:
///
/// - The existing 'section' was renamed to 'section-header'.
/// - The new section has explicit symbols. It can also have implicit
///   symbols referenced/created elsewhere.
/// - KeepAlive edges are stored outside of symbols, in
///   symbol-runs+keep-alive-info. The kept-alive blocks can still be
///   templated (as before) so their contents are independent of the
///   associated symbol.
/// - Blocks no longer reference their section explicitly. That must by
///   determined by context of refernece.
/// - Symbols that are referenced directly need to point at their section.
///   Other symbols do not.
///     - Exported symbols should be referenced by name to increase
///       deduplication of callers and to avoid unnecessarily pointing at
///       their section.
///     - Anonymous symbols should be referenced directly unless they need
///       "identity" (e.g., their address is taken or they are involved in a
///       reference cycle).
///     - Local symbols that aren't anonymous could be referenced either
///       way. For symbols with (meaningless) generated names, it seems best
///       to drop the name and reference by content. For names coming from
///       an entity in a source file (e.g., `static void f1() {}`) it's best
///       to reference indirectly to get dedup between builds.
/// - Target lists no longer include keep-alive edges. This should:
///     - Make more target lists match exactly (more dedup).
///     - Reduce the size of them (fewer references overall).
///     - Allow (many?) more to be inlined into their referencing block.
/// - Overall, this should reduce the number of references significantly.
///     - Every "function" drops a reference from its target list, target
///       lists get deduped better, and many functions start inlining target
///       lists.
///     - Anonymous symbols still get deduped and don't need to be pointed
///       at explicitly.
///     - Templated symbols (e.g., `__eh_frame`) are no longer explicitly
///       stored, avoiding a number of references to those blocks.
/// - This also demonstrates a path forward for `__debug*`, once it gets
///   split up between functions.
///     - Debug info is anonymous, and should be kept alive along with the
///       data it references.
///     - The debug info block(s) for a specific function could be added
///       to the symbol-runs.
///     - "Templating" the debug info block(s) also seems potentially
///        useful if it allows dedup. But that's less clear.
class CompileUnitRef : public SpecificRef<CompileUnitRef> {
  using SpecificRefT = SpecificRef<CompileUnitRef>;
  friend class SpecificRef<CompileUnitRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:compile-unit";

  const Triple &getTargetTriple() const { return TT; }
  unsigned getPointerSize() const { return PointerSize; }
  support::endianness getEndianness() const { return Endianness; }

  cas::CASID getDeadStripNeverID() const { return getReference(1); }
  cas::CASID getDeadStripLinkID() const { return getReference(2); }
  cas::CASID getIndirectDeadStripCompileID() const { return getReference(3); }
  cas::CASID getIndirectAnonymousID() const { return getReference(4); }
  cas::CASID getStrongExternalsID() const { return getReference(5); }
  cas::CASID getWeakExternalsID() const { return getReference(6); }
  Expected<SymbolTableRef> getDeadStripNever() const {
    return SymbolTableRef::get(getSchema().getNode(getDeadStripNeverID()));
  }
  Expected<SymbolTableRef> getDeadStripLink() const {
    return SymbolTableRef::get(getSchema().getNode(getDeadStripLinkID()));
  }
  Expected<SymbolTableRef> getIndirectDeadStripCompile() const {
    return SymbolTableRef::get(
        getSchema().getNode(getIndirectDeadStripCompileID()));
  }
  Expected<SymbolTableRef> getIndirectAnonymous() const {
    return SymbolTableRef::get(getSchema().getNode(getIndirectAnonymousID()));
  }
  Expected<NameListRef> getStrongExternals() const {
    return NameListRef::get(getSchema().getNode(getStrongExternalsID()));
  }
  Expected<NameListRef> getWeakExternals() const {
    return NameListRef::get(getSchema().getNode(getWeakExternalsID()));
  }

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
  static Expected<CompileUnitRef>
  create(const ObjectFileSchema &Schema, const Triple &TT, unsigned PointerSize,
         support::endianness Endianness, SymbolTableRef DeadStripNever,
         SymbolTableRef DeadStripLink, SymbolTableRef IndirectDeadStripCompile,
         SymbolTableRef IndirectAnonymous, NameListRef StrongExternals,
         NameListRef WeakExternals);

  /// Create a compile unit out of \p G.
  ///
  /// FIXME: Add configuration options to make more fine-tuned choices.
  static Expected<CompileUnitRef> create(const ObjectFileSchema &Schema,
                                         const jitlink::LinkGraph &G,
                                         raw_ostream *DebugOS = nullptr);

private:
  CompileUnitRef(SpecificRefT Ref, const Triple &TT, unsigned PointerSize,
                 support::endianness Endianness)
      : SpecificRefT(Ref), TT(TT), PointerSize(PointerSize),
        Endianness(Endianness) {}

  Triple TT;
  unsigned PointerSize;
  support::endianness Endianness;
};

} // namespace nestedv1
} // namespace casobjectformats
} // namespace llvm

#endif // LLVM_CASOBJECTFORMATS_NESTEDV1_H
