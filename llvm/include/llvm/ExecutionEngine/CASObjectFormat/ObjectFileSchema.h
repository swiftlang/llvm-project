//===- llvm/ExecutionEngine/CASObjectFormat/ObjectFileSchema.h --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_CASOBJECTFORMAT_OBJECTFILESCHEMA_H
#define LLVM_EXECUTIONENGINE_CASOBJECTFORMAT_OBJECTFILESCHEMA_H

#include "llvm/CAS/CASDB.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

namespace llvm {
namespace casobjectformat {

class ObjectFileSchema;
class ObjectFormatNodeRef : public cas::NodeRef {
public:
  static Expected<ObjectFormatNodeRef> get(ObjectFileSchema &Schema,
                                           Expected<cas::NodeRef> Ref);
  StringRef getKindString() const;

  ObjectFileSchema &getSchema() const { return *Schema; }

  ObjectFormatNodeRef() = delete;

protected:
  ObjectFormatNodeRef(ObjectFileSchema &Schema, const cas::NodeRef &Node)
      : cas::NodeRef(Node), Schema(&Schema) {}

private:
  ObjectFileSchema *Schema;
};

class ObjectFileSchema {
public:
  bool isNodeKind(const cas::NodeRef &Node, StringRef Kind);
  Optional<StringRef> getKindString(const cas::NodeRef &Node);
  Optional<cas::CASID> getKindStringID(StringRef KindString);
  bool isKindID(const cas::NodeRef &Node);

  ObjectFileSchema(cas::CASDB &CAS) : CAS(CAS) {}
  cas::CASDB &CAS;

  Expected<ObjectFormatNodeRef> createNode(ArrayRef<cas::CASID> IDs,
                                           StringRef Data) {
    return ObjectFormatNodeRef::get(*this, CAS.createNode(IDs, Data));
  }
  Expected<ObjectFormatNodeRef> getNode(cas::CASID ID) {
    return ObjectFormatNodeRef::get(*this, CAS.getNode(ID));
  }

private:
  // Two-way map. Always small enough for linear search.
  SmallVector<std::pair<cas::CASID, StringRef>, 16> KindStringCache;
  Optional<cas::CASID> RootKindID;
  Error fillCache();
};

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

template <class DerivedT>
class LeafRef : public SpecificRef<LeafRef<DerivedT>, DerivedT> {
  using SpecificRefT = SpecificRef<LeafRef<DerivedT>, DerivedT>;
  friend class SpecificRef<LeafRef<DerivedT>, DerivedT>;

  // Hide non-leaf APIs.
  size_t getNumReferences() const;
  cas::CASID getReference(size_t I) const;
  Error forEachReference(function_ref<Error(cas::CASID)> Callback) const;

protected:
  static Expected<DerivedT> get(Expected<ObjectFormatNodeRef> Ref) {
    if (auto Leaf = getLeaf(std::move(Ref)))
      return DerivedT(*Leaf);
    else
      return Leaf.takeError();
  }

  static Expected<LeafRef> getLeaf(Expected<ObjectFormatNodeRef> Ref) {
    auto SpecificRef = SpecificRefT::getSpecific(std::move(Ref));
    if (!SpecificRef)
      return SpecificRef.takeError();
    if (SpecificRef->getNumReferences() != 1)
      return createStringError(inconvertibleErrorCode(), "corrupt leaf object");
    return LeafRef(*SpecificRef);
  }

  LeafRef(ObjectFormatNodeRef Ref) : SpecificRefT(Ref) {}
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
class SectionRef : public SpecificRef<SectionRef> {
  using SpecificRefT = SpecificRef<SectionRef>;
  friend class SpecificRef<SectionRef>;

public:
  // FIXME: Support "huge" bit?
  static constexpr StringLiteral KindString = "cas.o:section";

  cas::CASID getNameID() const { return getReference(1); }
  Expected<cas::BlobRef> getName() const {
    return getCAS().getBlob(getNameID());
  }
  sys::Memory::ProtectionFlags getProtectionFlags() const;

  static Expected<SectionRef> create(ObjectFileSchema &Schema,
                                     cas::BlobRef SectionName,
                                     sys::Memory::ProtectionFlags Protections);
  static Expected<SectionRef> create(ObjectFileSchema &Schema,
                                     const jitlink::Section &S);

  static Expected<SectionRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SectionRef> get(ObjectFileSchema &Schema, cas::CASID ID) {
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
  /// FIXME: This is used as a type-id. It should probably be a reference to a
  /// CAS object to avoid requiring "kind-string" support in CASDB.
  static constexpr StringLiteral KindString = "cas.o:block-data";

  bool isZeroFill() const { return getNumReferences() == 1; }
  uint64_t getSize() const;
  uint64_t getAlignment() const;
  uint64_t getAlignmentOffset() const;

  Optional<cas::CASID> getContentID() const {
    return isZeroFill() ? Optional<cas::CASID>() : getReference(1);
  }
  Expected<Optional<cas::BlobRef>> getContent() const {
    if (Optional<cas::CASID> Content = getContentID())
      return getCAS().getBlob(*Content);
    return None;
  }

  static Expected<BlockDataRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<BlockDataRef> get(ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  static Expected<BlockDataRef> createZeroFill(ObjectFileSchema &Schema,
                                               uint64_t Size,
                                               uint64_t Alignment,
                                               uint64_t AlignmentOffset);

  static Expected<BlockDataRef> createContent(ObjectFileSchema &Schema,
                                              cas::BlobRef Blob,
                                              uint64_t Alignment,
                                              uint64_t AlignmentOffset);

  static Expected<BlockDataRef> createContent(ObjectFileSchema &Schema,
                                              StringRef Content,
                                              uint64_t Alignment,
                                              uint64_t AlignmentOffset);

  static Expected<BlockDataRef> create(ObjectFileSchema &Schema,
                                       const jitlink::Block &Block);

private:
  explicit BlockDataRef(SpecificRefT Ref) : SpecificRefT(Ref) {}

  static Expected<BlockDataRef> createImpl(ObjectFileSchema &Schema,
                                           Optional<cas::BlobRef> Content,
                                           uint64_t Size, uint64_t Alignment,
                                           uint64_t AlignmentOffset);
};

/// An array of fixup offsets and kinds.
class FixupListRef : public LeafRef<FixupListRef> {
  using LeafRefT = LeafRef<FixupListRef>;
  friend class LeafRef<FixupListRef>;

  // FIXME: Is this ABI-stable, or should we use something bigger in case we
  // need more space in the future?
  //
  // Lang: Might as well use 16 bits or 32 bits, for expansion purposes. E.g.,
  // X86 generic edges: only difference is whether linker can consider it a
  // candidate for optimization.
  // Duncan: should that be a property of the fixup or the target?
  // Lang: came up in jitlink plugins for indirection, when synthesizing a jump
  // stub for PLT. Do the same thing with a permanent ability to indirect. Need
  // to think more.
  //
  // FIXME: Formalize CAS types separately from what jitlink has.
  using SerializedKindT = jitlink::Edge::Kind;
  using SerializedOffsetT = jitlink::Edge::OffsetT;
  static constexpr size_t SerializedFixupSize =
      sizeof(SerializedKindT) + sizeof(SerializedOffsetT);

public:
  static constexpr StringLiteral KindString = "cas.o:fixup-list";

  static Expected<FixupListRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<FixupListRef> get(ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }
  size_t getNumEdges() const { return getData().size() / SerializedFixupSize; }
  jitlink::Edge::OffsetT getFixupOffset(size_t I) const;
  jitlink::Edge::Kind getFixupKind(size_t I) const;

  static Expected<FixupListRef> create(ObjectFileSchema &Schema,
                                       ArrayRef<const jitlink::Edge *> Edges);

private:
  explicit FixupListRef(LeafRefT Ref) : LeafRefT(Ref) {}
};

/// An array of target indices and addends, parallel to \a FixupListRef. The
/// target indexes point into an associated \a TargetListRef.
class TargetInfoListRef : public LeafRef<TargetInfoListRef> {
  using LeafRefT = LeafRef<TargetInfoListRef>;
  friend class LeafRef<TargetInfoListRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:target-info-list";

  size_t getNumEdges() const;
  size_t getTargetIndex(size_t I) const;
  jitlink::Edge::AddendT getAddend(size_t I) const;

  static Expected<TargetInfoListRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<TargetInfoListRef> get(ObjectFileSchema &Schema,
                                         cas::CASID ID) {
    return get(Schema.getNode(ID));
  }
  static Expected<TargetInfoListRef>
  create(ObjectFileSchema &Schema, ArrayRef<size_t> TargetIndices,
         ArrayRef<jitlink::Edge::AddendT> Addends);

private:
  explicit TargetInfoListRef(LeafRefT Ref) : LeafRefT(Ref) {}
};

/// A variant of SymbolRef and IndirectSymbolRef. The kind is cached.
class TargetRef : public ObjectFormatNodeRef {
  friend class SymbolRef;
  friend class IndirectSymbolRef;

public:
  enum Kind {
    Symbol,
    IndirectSymbol,
  };

  Kind getKind() const { return K; }

  static StringRef getKindString(Kind K);
  StringRef getKindString() const { return getKindString(K); }

  /// Get a \a TargetRef. If \c Kind is specified, returns an error on
  /// mismatch; otherwise just requires that it's a valid target.
  static Expected<TargetRef> get(Expected<ObjectFormatNodeRef> Ref,
                                 Optional<Kind> ExpectedKind = None);
  static Expected<TargetRef> get(ObjectFileSchema &Schema, cas::CASID ID,
                                 Optional<Kind> ExpectedKind = None) {
    return get(Schema.getNode(ID), ExpectedKind);
  }

private:
  TargetRef(ObjectFormatNodeRef Ref, Kind K) : ObjectFormatNodeRef(Ref), K(K) {}
  Kind K;
};

/// An array of targets.
class TargetListRef : public SpecificRef<TargetListRef> {
  using SpecificRefT = SpecificRef<TargetListRef>;
  friend class SpecificRef<TargetListRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:target-list";

  bool hasAbstractBackedge() const;

  size_t getNumTargets() const { return getNumReferences() - 1; }

  Expected<TargetRef> getTarget(size_t I) const {
    return TargetRef::get(getSchema(), getTargetID(I));
  }
  cas::CASID getTargetID(size_t I) const { return getReference(I + 1); }

  /// Create the given target list. Does not sort the targets, since it's
  /// assumed the order is already relevant.
  static Expected<TargetListRef> create(ObjectFileSchema &Schema,
                                        ArrayRef<TargetRef> Targets);

  static Expected<TargetListRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<TargetListRef> get(ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

private:
  explicit TargetListRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// An indirect reference to a symbol.
///
/// - Reference an external symbol.
/// - Reference a symbol in a cycle.
///
/// FIXME: Should IsExternal be here? Here's what's wrong with it:
///
/// - Adds complexity.
/// - Blocks redundancy in some cases.
class IndirectSymbolRef : public SpecificRef<IndirectSymbolRef> {
  using SpecificRefT = SpecificRef<IndirectSymbolRef>;
  friend class SpecificRef<IndirectSymbolRef>;

public:
  static const constexpr StringLiteral KindString = "cas.o:indirect-symbol";

  bool isExternal() const;
  bool isAbstractBackedge() const;
  cas::CASID getNameID() const { return getReference(1); }
  Expected<cas::BlobRef> getName() const {
    return getCAS().getBlob(getNameID());
  }

  TargetRef getAsTarget() const {
    return TargetRef(*this, TargetRef::IndirectSymbol);
  }

  static Expected<IndirectSymbolRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<IndirectSymbolRef> get(ObjectFileSchema &Schema,
                                         cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  static Expected<IndirectSymbolRef> create(ObjectFileSchema &Schema,
                                            cas::BlobRef Name, bool IsExternal);
  static Expected<IndirectSymbolRef> create(ObjectFileSchema &Schema,
                                            StringRef Name, bool IsExternal);
  static Expected<IndirectSymbolRef> create(ObjectFileSchema &Schema,
                                            const jitlink::Symbol &S);

private:
  explicit IndirectSymbolRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
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
  static Expected<SymbolDefinitionRef> get(ObjectFileSchema &Schema,
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
/// sorting of blocks (e.g., EH frames: CFEs before FDEs).
///
/// FIXME: Evaluate the following (major) refactoring:
///
/// - Delete the current 'edge-list' and 'target-list', which don't seem
///   effective at exploiting redundancy.
/// - Create a new 'target-list' that only stores TargetRef, sorted by target.
///   This should get us some redundancy when the list of symbols referenced
///   changes independently of the list addends or edge order. Mostly similar
///   to 'symbol-table', but a different sort order and it can reference
///   'indirect-symbol'.
/// - Create a data-only 'edge-list' that is a parallel array to 'fixup-list',
///   containing indexes into 'target-list' and addends.
/// - Reference 'edge-list' and 'target-list' directly from 'block' (they
///   should not reference each other).
///
/// FIXME: Sink 'fixup-list' down to 'block-data'.
///
/// FIXME: Consider sinking 'section' down to 'block-data' (maybe even inlining
/// it there). This reduces the size of 'block'. A bad idea if we support
/// ELF/COFF section names in 'section' -- especially COMDATs -- since that
/// will explode the number of 'block-data'... but probably COMDATs and
/// function sections should be modelled at a higher level, not even referenced
/// by 'block'.
class BlockRef : public SpecificRef<BlockRef> {
  using SpecificRefT = SpecificRef<BlockRef>;
  friend class SpecificRef<BlockRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:block";

  bool hasEdges() const { return getNumReferences() > 3; }

  bool hasAbstractBackedge() const;

  cas::CASID getSectionID() const { return getReference(1); }
  cas::CASID getDataID() const { return getReference(2); }
  Optional<cas::CASID> getFixupsID() const {
    return hasEdges() ? getReference(3) : Optional<cas::CASID>();
  }
  Optional<cas::CASID> getTargetInfoID() const {
    return hasEdges() ? getReference(4) : Optional<cas::CASID>();
  }
  Optional<cas::CASID> getTargetsID() const {
    return hasEdges() ? getReference(5) : Optional<cas::CASID>();
  }

  Expected<SectionRef> getSection() const {
    return SectionRef::get(getSchema(), getSectionID());
  }
  Expected<BlockDataRef> getData() const {
    return BlockDataRef::get(getSchema(), getDataID());
  }
  Expected<Optional<FixupListRef>> getFixups() const {
    if (Optional<cas::CASID> Fixups = getFixupsID())
      return FixupListRef::get(getSchema(), *Fixups);
    return None;
  }
  Expected<Optional<TargetInfoListRef>> getTargetInfo() const {
    if (Optional<cas::CASID> TargetInfo = getTargetInfoID())
      return TargetInfoListRef::get(getSchema(), *TargetInfo);
    return None;
  }
  Expected<Optional<TargetListRef>> getTargets() const {
    if (Optional<cas::CASID> Targets = getTargetsID())
      return TargetListRef::get(getSchema(), *Targets);
    return None;
  }

  static Expected<BlockRef>
  create(ObjectFileSchema &Schema, const jitlink::Block &Block,
         function_ref<Expected<TargetRef>(
             const jitlink::Symbol &, jitlink::Edge::Kind, bool IsFromData,
             jitlink::Edge::AddendT &Addend, Optional<StringRef> &SplitContent)>
             GetTargetRef);

  static Expected<BlockRef> create(ObjectFileSchema &Schema, SectionRef Section,
                                   BlockDataRef Data) {
    return createImpl(Schema, Section, Data, None, None, None);
  }
  static Expected<BlockRef> create(ObjectFileSchema &Schema, SectionRef Section,
                                   BlockDataRef Data, FixupListRef Fixups,
                                   TargetInfoListRef TargetInfo,
                                   TargetListRef Targets) {
    return createImpl(Schema, Section, Data, Fixups, TargetInfo, Targets);
  }

  static Expected<BlockRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<BlockRef> get(ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  SymbolDefinitionRef getAsSymbolDefinition() const {
    return SymbolDefinitionRef(*this, SymbolDefinitionRef::Block);
  }

private:
  explicit BlockRef(SpecificRefT Ref) : SpecificRefT(Ref) {}

  static Expected<BlockRef> createImpl(ObjectFileSchema &Schema,
                                       SectionRef Section, BlockDataRef Data,
                                       Optional<FixupListRef> Fixups,
                                       Optional<TargetInfoListRef> TargetInfo,
                                       Optional<TargetListRef> Targets);
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
  bool hasName() const { return getNumReferences() > 2; }

  /// True if this symbol is a template for a symbol. This is true if its
  /// definition has an abstract backedge.
  bool isSymbolTemplate() const;

  uint64_t getOffset() const;

  Flags getFlags() const {
    return Flags(getDeadStrip(), getScope(), getMerge());
  }

  DeadStripKind getDeadStrip() const { return (DeadStripKind)getData()[8]; }
  ScopeKind getScope() const { return (ScopeKind)getData()[9]; }
  MergeKind getMerge() const { return (MergeKind)getData()[10]; }

  cas::CASID getDefinitionID() const { return getReference(1); }
  Optional<cas::CASID> getNameID() const {
    return hasName() ? getReference(2) : Optional<cas::CASID>();
  }

  Expected<SymbolDefinitionRef> getDefinition() const {
    return SymbolDefinitionRef::get(getSchema().getNode(getDefinitionID()));
  }
  Expected<Optional<cas::BlobRef>> getName() const {
    if (!hasName())
      return None;
    return getCAS().getBlob(*getNameID());
  }

  static Expected<SymbolRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SymbolRef> get(ObjectFileSchema &Schema, cas::CASID ID) {
    return get(Schema.getNode(ID));
  }

  Expected<IndirectSymbolRef> createIndirectSymbol() const;

  TargetRef getAsTarget() const { return TargetRef(*this, TargetRef::Symbol); }

  static Expected<SymbolRef> create(ObjectFileSchema &Schema,
                                    Optional<cas::BlobRef> SymbolName,
                                    SymbolDefinitionRef Definition,
                                    uint64_t Offset, Flags F);

  static Expected<SymbolRef>
  create(ObjectFileSchema &Schema, const jitlink::Symbol &S,
         function_ref<Expected<SymbolDefinitionRef>(const jitlink::Block &)>
             GetDefinitionRef);

private:
  explicit SymbolRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// A symbol table.
class SymbolTableRef : public SpecificRef<SymbolTableRef> {
  using SpecificRefT = SpecificRef<SymbolTableRef>;
  friend class SpecificRef<SymbolTableRef>;

public:
  static constexpr StringLiteral KindString = "cas.o:symbol-table";

  size_t getNumAnonymousSymbols() const;
  size_t getNumNamedSymbols() const {
    return getNumSymbols() - getNumAnonymousSymbols();
  }
  size_t getNumSymbols() const { return getNumReferences() - 1; }
  cas::CASID getSymbolID(size_t I) const { return getReference(I + 1); }

  Expected<SymbolRef> getSymbol(size_t I) const {
    return SymbolRef::get(getSchema().getNode(getSymbolID(I)));
  }

  Expected<Optional<SymbolRef>> lookupSymbol(cas::BlobRef Name) const;

  static Expected<SymbolTableRef> create(ObjectFileSchema &Schema,
                                         ArrayRef<SymbolRef> Symbols);

  static Expected<SymbolTableRef> get(Expected<ObjectFormatNodeRef> Ref);
  static Expected<SymbolTableRef> get(ObjectFileSchema &Schema,
                                      cas::CASID CASID) {
    return get(Schema.getNode(CASID));
  }

private:
  explicit SymbolTableRef(SpecificRefT Ref) : SpecificRefT(Ref) {}
};

/// An object file / compile unit.
///
/// Note: This wrapper eagerly parses target triple, pointer size, and
/// endianness.
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
  static Expected<CompileUnitRef> get(ObjectFileSchema &Schema,
                                      cas::CASID &ID) {
    return get(Schema.getNode(ID));
  }
  static Expected<CompileUnitRef>
  create(ObjectFileSchema &Schema, const Triple &TT, unsigned PointerSize,
         support::endianness Endianness, SymbolTableRef DeadStripNever,
         SymbolTableRef DeadStripLink, SymbolTableRef IndirectDeadStripCompile);

  /// Create a compile unit out of \p G.
  ///
  /// FIXME: Add configuration options to make more fine-tuned choices.
  static Expected<CompileUnitRef> create(ObjectFileSchema &Schema,
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

} // namespace casobjectformat
} // namespace llvm

#endif // LLVM_EXECUTIONENGINE_CASOBJECTFORMAT_OBJECTFILESCHEMA_H
