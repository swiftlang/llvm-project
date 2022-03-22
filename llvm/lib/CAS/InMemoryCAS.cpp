//===- InMemoryCAS.cpp ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "BuiltinCAS.h"
#include "llvm/ADT/LazyAtomicPointer.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/CAS/BuiltinObjectHasher.h"
#include "llvm/CAS/HashMappedTrie.h"
#include "llvm/CAS/ThreadSafeAllocator.h"
#include "llvm/Support/Allocator.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::cas::builtin;

namespace {

class InMemoryObject;
class InMemoryString;

/// Index of referenced IDs (map: Hash -> InMemoryObject*). Uses
/// LazyAtomicPointer to coordinate creation of objects.
using InMemoryIndexT =
    ThreadSafeHashMappedTrie<LazyAtomicPointer<const InMemoryObject>,
                             sizeof(HashType)>;

/// Values in \a InMemoryIndexT. \a InMemoryObject's point at this to access
/// their hash.
using InMemoryIndexValueT = InMemoryIndexT::value_type;

/// String pool.
using InMemoryStringPoolT =
    ThreadSafeHashMappedTrie<LazyAtomicPointer<const InMemoryString>,
                             sizeof(HashType)>;

/// Action cache type (map: Hash -> InMemoryObject*). Always refers to existing
/// objects.
using InMemoryCacheT =
    ThreadSafeHashMappedTrie<const InMemoryIndexValueT *, sizeof(HashType)>;

class InMemoryObject {
public:
  enum class Kind {
    /// Node with refs and data.
    RefNode,

    /// Blob with data (8-byte alignment guaranteed, null-terminated).
    RefBlob,

    /// Node with refs and data co-allocated.
    InlineNode,

    /// Blob with data (as above) co-allocated.
    InlineBlob,

    /// Tree: custom node. Pairs of refs pointing at target (arbitrary object)
    /// and names (String), and some data to describe the kind of the entry.
    Tree,

    Max = Tree,
  };

  Kind getKind() const { return IndexAndKind.getInt(); }
  const InMemoryIndexValueT &getIndex() const {
    assert(IndexAndKind.getPointer());
    return *IndexAndKind.getPointer();
  }

  ArrayRef<uint8_t> getHash() const { return getIndex().Hash; }

  InMemoryObject() = delete;
  InMemoryObject(InMemoryObject &&) = delete;
  InMemoryObject(const InMemoryObject &) = delete;

protected:
  InMemoryObject(Kind K, const InMemoryIndexValueT &I) : IndexAndKind(&I, K) {}

private:
  enum Counts : int {
    NumKindBits = 3,
  };
  PointerIntPair<const InMemoryIndexValueT *, NumKindBits, Kind> IndexAndKind;
  static_assert((1U << NumKindBits) <= alignof(InMemoryIndexValueT),
                "Kind will clobber pointer");
  static_assert(((int)Kind::Max >> NumKindBits) == 0, "Kind will be truncated");
};

template <class DerivedT> class GetDataString {
public:
  StringRef getDataString() const {
    ArrayRef<char> Array = static_cast<const DerivedT *>(this)->getData();
    return StringRef(Array.begin(), Array.size());
  }
};

class InMemoryBlob : public InMemoryObject, public GetDataString<InMemoryBlob> {
public:
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == Kind::RefBlob || O->getKind() == Kind::InlineBlob;
  }
  inline ArrayRef<char> getData() const;

private:
  using InMemoryObject::InMemoryObject;
};

class InMemoryRefBlob : public InMemoryBlob {
public:
  static constexpr Kind KindValue = Kind::RefBlob;
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == KindValue;
  }

  ArrayRef<char> getDataImpl() const { return Data; }
  ArrayRef<char> getData() const { return Data; }

  static InMemoryRefBlob &create(function_ref<void *(size_t Size)> Allocate,
                                 const InMemoryIndexValueT &I,
                                 ArrayRef<char> Data) {
    void *Mem = Allocate(sizeof(InMemoryRefBlob));
    return *new (Mem) InMemoryRefBlob(I, Data);
  }

private:
  InMemoryRefBlob(const InMemoryIndexValueT &I, ArrayRef<char> Data)
      : InMemoryBlob(KindValue, I), Data(Data) {
    assert(isAddrAligned(Align(8), Data.data()) && "Expected 8-byte alignment");
    assert(*Data.end() == 0 && "Expected null-termination");
  }
  ArrayRef<char> Data;
};

class InMemoryInlineBlob : public InMemoryBlob {
public:
  static constexpr Kind KindValue = Kind::InlineBlob;
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == KindValue;
  }

  ArrayRef<char> getDataImpl() const {
    return makeArrayRef(reinterpret_cast<const char *>(this + 1), Size);
  }
  ArrayRef<char> getData() const { return getDataImpl(); }

  static InMemoryInlineBlob &create(function_ref<void *(size_t Size)> Allocate,
                                    const InMemoryIndexValueT &I,
                                    ArrayRef<char> Data) {
    void *Mem = Allocate(sizeof(InMemoryInlineBlob) + Data.size() + 1);
    return *new (Mem) InMemoryInlineBlob(I, Data);
  }

private:
  InMemoryInlineBlob(const InMemoryIndexValueT &I, ArrayRef<char> Data)
      : InMemoryBlob(KindValue, I), Size(Data.size()) {
    auto *Begin = reinterpret_cast<char *>(this + 1);
    llvm::copy(Data, Begin);
    Begin[Data.size()] = 0;
  }
  size_t Size;
};

class InMemoryNode : public InMemoryObject, public GetDataString<InMemoryNode> {
public:
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == Kind::RefNode || O->getKind() == Kind::InlineNode;
  }
  inline ArrayRef<char> getData() const;
  inline ArrayRef<const InMemoryObject *> getRefs() const;

private:
  using InMemoryObject::InMemoryObject;
};

class InMemoryRefNode : public InMemoryNode {
public:
  static constexpr Kind KindValue = Kind::RefNode;
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == KindValue;
  }

  ArrayRef<const InMemoryObject *> getRefsImpl() const { return Refs; }
  ArrayRef<const InMemoryObject *> getRefs() const { return Refs; }
  ArrayRef<char> getDataImpl() const { return Data; }
  ArrayRef<char> getData() const { return Data; }

  static InMemoryRefNode &create(function_ref<void *(size_t Size)> Allocate,
                                 const InMemoryIndexValueT &I,
                                 ArrayRef<const InMemoryObject *> Refs,
                                 ArrayRef<char> Data) {
    void *Mem = Allocate(sizeof(InMemoryRefNode));
    return *new (Mem) InMemoryRefNode(I, Refs, Data);
  }

private:
  InMemoryRefNode(const InMemoryIndexValueT &I,
                  ArrayRef<const InMemoryObject *> Refs, ArrayRef<char> Data)
      : InMemoryNode(KindValue, I), Refs(Refs), Data(Data) {
    assert(isAddrAligned(Align(8), this) && "Expected 8-byte alignment");
    assert(isAddrAligned(Align(8), Data.data()) && "Expected 8-byte alignment");
    assert(*Data.end() == 0 && "Expected null-termination");
  }

  ArrayRef<const InMemoryObject *> Refs;
  ArrayRef<char> Data;
};

class InMemoryInlineNode : public InMemoryNode {
public:
  static constexpr Kind KindValue = Kind::InlineNode;
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == KindValue;
  }

  ArrayRef<const InMemoryObject *> getRefs() const { return getRefsImpl(); }
  ArrayRef<const InMemoryObject *> getRefsImpl() const {
    return makeArrayRef(
        reinterpret_cast<const InMemoryObject *const *>(this + 1), NumRefs);
  }

  ArrayRef<char> getData() const { return getDataImpl(); }
  ArrayRef<char> getDataImpl() const {
    ArrayRef<const InMemoryObject *> Refs = getRefs();
    return makeArrayRef(
        reinterpret_cast<const char *>(Refs.data() + Refs.size()), DataSize);
  }

  static InMemoryInlineNode &create(function_ref<void *(size_t Size)> Allocate,
                                    const InMemoryIndexValueT &I,
                                    ArrayRef<const InMemoryObject *> Refs,
                                    ArrayRef<char> Data) {
    void *Mem = Allocate(sizeof(InMemoryInlineNode) +
                         sizeof(uintptr_t) * Refs.size() + Data.size() + 1);
    return *new (Mem) InMemoryInlineNode(I, Refs, Data);
  }

private:
  InMemoryInlineNode(const InMemoryIndexValueT &I,
                     ArrayRef<const InMemoryObject *> Refs, ArrayRef<char> Data)
      : InMemoryNode(KindValue, I), NumRefs(Refs.size()),
        DataSize(Data.size()) {
    auto *BeginRefs = reinterpret_cast<const InMemoryObject **>(this + 1);
    llvm::copy(Refs, BeginRefs);
    auto *BeginData = reinterpret_cast<char *>(BeginRefs + NumRefs);
    llvm::copy(Data, BeginData);
    BeginData[Data.size()] = 0;
  }
  uint32_t NumRefs;
  uint32_t DataSize;
};

class InMemoryTree : public InMemoryObject {
public:
  static constexpr Kind KindValue = Kind::Tree;
  static bool classof(const InMemoryObject *O) {
    return O->getKind() == KindValue;
  }

  struct NamedRef {
    const InMemoryObject *Ref;
    const InMemoryString *Name;
  };

  struct NamedEntry {
    const InMemoryObject *Ref;
    const InMemoryString *Name;
    TreeEntry::EntryKind Kind;
  };

  ArrayRef<NamedRef> getNamedRefs() const {
    return makeArrayRef(reinterpret_cast<const NamedRef *>(this + 1), Size);
  }

  ArrayRef<TreeEntry::EntryKind> getKinds() const {
    ArrayRef<NamedRef> Refs = getNamedRefs();
    return makeArrayRef(reinterpret_cast<const TreeEntry::EntryKind *>(
                            Refs.data() + Refs.size()),
                        Size);
  }

  Optional<NamedEntry> find(StringRef Name) const;

  bool empty() const { return !Size; }
  size_t size() const { return Size; }

  NamedEntry operator[](ptrdiff_t I) const {
    assert((size_t)I < size());
    NamedRef NR = getNamedRefs()[I];
    return NamedEntry{NR.Ref, NR.Name, getKinds()[I]};
  }

  static InMemoryTree &create(function_ref<void *(size_t Size)> Allocate,
                              const InMemoryIndexValueT &I,
                              ArrayRef<NamedEntry> Entries);

private:
  InMemoryTree(const InMemoryIndexValueT &I, ArrayRef<NamedEntry> Entries);
  size_t Size;
};

/// Internal string type.
class InMemoryString {
public:
  StringRef get() const {
    return StringRef(reinterpret_cast<const char *>(this + 1), Size);
  }

  static InMemoryString &create(function_ref<void *(size_t Size)> Allocate,
                                StringRef String) {
    assert(String.size() <= UINT32_MAX && "Expected strings smaller than 4GB");
    void *Mem = Allocate(sizeof(InMemoryString) + String.size() + 1);
    return *new (Mem) InMemoryString(String);
  }

private:
  InMemoryString(StringRef String) : Size(String.size()) {
    auto *Begin = reinterpret_cast<char *>(this + 1);
    llvm::copy(String, Begin);
    Begin[String.size()] = 0;
  }
  size_t Size;
};

/// In-memory CAS database and action cache (the latter should be separated).
class InMemoryCAS : public BuiltinCAS {
public:
  Expected<CASID> parseIDImpl(ArrayRef<uint8_t> Hash) final {
    return getID(indexHash(Hash));
  }

  Expected<BlobProxy> createBlobImpl(ArrayRef<uint8_t> ComputedHash,
                                     ArrayRef<char> Data) final;
  Expected<TreeProxy>
  createTreeImpl(ArrayRef<uint8_t> ComputedHash,
                 ArrayRef<NamedTreeEntry> SortedEntries) final;
  Expected<NodeProxy> createNodeImpl(ArrayRef<uint8_t> ComputedHash,
                                     ArrayRef<CASID> References,
                                     ArrayRef<char> Data) final;
  Optional<ObjectKind> getObjectKind(CASID ID) final;

  Expected<BlobProxy>
  createBlobFromNullTerminatedRegion(ArrayRef<uint8_t> ComputedHash,
                                     sys::fs::mapped_file_region Map) override;

  CASID getID(const InMemoryIndexValueT &I) const {
    return CASID::getFromInternalID(*this, reinterpret_cast<uintptr_t>(&I));
  }
  CASID getID(const InMemoryObject &O) const { return getID(O.getIndex()); }
  const InMemoryIndexValueT &extractIndexFromID(CASID ID) const {
    assert(&ID.getContext() == this);
    return *reinterpret_cast<const InMemoryIndexValueT *>(
        (uintptr_t)ID.getInternalID(*this));
  }
  InMemoryIndexValueT &extractIndexFromID(CASID ID) {
    return const_cast<InMemoryIndexValueT &>(
        const_cast<const InMemoryCAS *>(this)->extractIndexFromID(ID));
  }

  ArrayRef<uint8_t> getHashImpl(const CASID &ID) const final {
    return extractIndexFromID(ID).Hash;
  }

  Expected<BlobProxy> getBlobProxy(const InMemoryObject &Object) {
    if (auto *Blob = dyn_cast<InMemoryBlob>(&Object))
      return makeBlobProxy(getID(Object), Blob->getDataString());
    return createInvalidObjectError(getID(Object), ObjectKind::Blob);
  }

  Expected<NodeProxy> getNodeProxy(const InMemoryObject &Object) {
    if (auto *Node = dyn_cast<InMemoryNode>(&Object))
      return makeNodeProxy(getID(Object), Node, Node->getRefs().size(),
                           Node->getDataString());
    return createInvalidObjectError(getID(Object), ObjectKind::Node);
  }

  Expected<TreeProxy> getTreeProxy(const InMemoryObject &Object) {
    if (auto *Tree = dyn_cast<InMemoryTree>(&Object))
      return makeTreeProxy(getID(Object), Tree, Tree->size());
    return createInvalidObjectError(getID(Object), ObjectKind::Tree);
  }

  InMemoryIndexValueT &indexHash(ArrayRef<uint8_t> Hash) {
    return *Index.insertLazy(
        Hash, [](auto ValueConstructor) { ValueConstructor.emplace(nullptr); });
  }

  /// TODO: Consider callers to actually do an insert and to return a handle to
  /// the slot in the trie.
  const InMemoryObject *getObject(CASID ID) const {
    if (&ID.getContext() == this)
      return extractIndexFromID(ID).Data;
    assert(ID.getContext().getHashSchemaIdentifier() ==
               getHashSchemaIdentifier() &&
           "Expected ID from same hash schema");
    if (InMemoryIndexT::const_pointer P = Index.find(ID.getHash()))
      return P->Data;
    return nullptr;
  }

  const InMemoryString &getOrCreateString(StringRef String);
  Expected<BlobProxy> getBlob(CASID ID) final {
    if (const InMemoryObject *Object = getObject(ID))
      return getBlobProxy(*Object);
    return createInvalidObjectError(ID, ObjectKind::Blob);
  }
  Expected<NodeProxy> getNode(CASID ID) final {
    if (const InMemoryObject *Object = getObject(ID))
      return getNodeProxy(*Object);
    return createInvalidObjectError(ID, ObjectKind::Node);
  }
  Expected<TreeProxy> getTree(CASID ID) final {
    if (const InMemoryObject *Object = getObject(ID))
      return getTreeProxy(*Object);
    return createInvalidObjectError(ID, ObjectKind::Tree);
  }
  void print(raw_ostream &OS) const final;

  Expected<CASID> getCachedResult(CASID InputID) final;
  Error putCachedResult(CASID InputID, CASID OutputID) final;

  InMemoryCAS() = default;

private:
  // TreeAPI.
  NamedTreeEntry makeTreeEntry(const InMemoryTree::NamedEntry &Entry) const {
    return NamedTreeEntry(getID(*Entry.Ref), Entry.Kind, Entry.Name->get());
  }
  Optional<NamedTreeEntry> lookupInTree(const TreeProxy &Handle,
                                        StringRef Name) const final;
  NamedTreeEntry getInTree(const TreeProxy &Handle, size_t I) const final;

  Expected<StringRef> getNodeName(const NodeProxy &Node, size_t I) const final;

  Error forEachEntryInTree(
      const TreeProxy &Tree,
      function_ref<Error(const NamedTreeEntry &)> Callback) const final;

  // NodeAPI.
  CASID getReferenceInNode(const NodeProxy &Handle, size_t I) const final;
  Error forEachReferenceInNode(const NodeProxy &Handle,
                               function_ref<Error(CASID)> Callback) const final;

  /// Index of referenced IDs (map: Hash -> InMemoryObject*). Mapped to nullptr
  /// as a convenient way to store hashes.
  ///
  /// - Insert nullptr on lookups.
  /// - InMemoryObject points back to here.
  InMemoryIndexT Index;

  /// String pool for trees.
  InMemoryStringPoolT StringPool;

  /// Action cache (map: Hash -> InMemoryObject*).
  InMemoryCacheT ActionCache;

  ThreadSafeAllocator<BumpPtrAllocator> Objects;
  ThreadSafeAllocator<BumpPtrAllocator> Strings;
  ThreadSafeAllocator<SpecificBumpPtrAllocator<sys::fs::mapped_file_region>>
      MemoryMaps;
};

} // end anonymous namespace

Optional<ObjectKind> InMemoryCAS::getObjectKind(CASID ID) {
  const InMemoryObject *Object = getObject(ID);
  if (!Object)
    return None;
  switch (Object->getKind()) {
  case InMemoryObject::Kind::RefNode:
  case InMemoryObject::Kind::InlineNode:
    return ObjectKind::Node;

  case InMemoryObject::Kind::RefBlob:
  case InMemoryObject::Kind::InlineBlob:
    return ObjectKind::Blob;

  case InMemoryObject::Kind::Tree:
    return ObjectKind::Tree;
  }
}

ArrayRef<char> InMemoryBlob::getData() const {
  if (auto *Derived = dyn_cast<InMemoryRefBlob>(this))
    return Derived->getDataImpl();
  return cast<InMemoryInlineBlob>(this)->getDataImpl();
}

ArrayRef<char> InMemoryNode::getData() const {
  if (auto *Derived = dyn_cast<InMemoryRefNode>(this))
    return Derived->getDataImpl();
  return cast<InMemoryInlineNode>(this)->getDataImpl();
}

ArrayRef<const InMemoryObject *> InMemoryNode::getRefs() const {
  if (auto *Derived = dyn_cast<InMemoryRefNode>(this))
    return Derived->getRefsImpl();
  return cast<InMemoryInlineNode>(this)->getRefsImpl();
}

void InMemoryCAS::print(raw_ostream &OS) const {
  OS << "index: ";
  Index.print(OS);
  OS << "strings: ";
  Index.print(OS);
  OS << "action-cache: ";
  ActionCache.print(OS);
}

InMemoryTree &InMemoryTree::create(function_ref<void *(size_t Size)> Allocate,
                                   const InMemoryIndexValueT &I,
                                   ArrayRef<NamedEntry> Entries) {
  void *Mem =
      Allocate(sizeof(InMemoryTree) + sizeof(NamedRef) * Entries.size() +
               sizeof(TreeEntry::EntryKind) * Entries.size());
  return *new (Mem) InMemoryTree(I, Entries);
}

InMemoryTree::InMemoryTree(const InMemoryIndexValueT &I,
                           ArrayRef<NamedEntry> Entries)
    : InMemoryObject(KindValue, I), Size(Entries.size()) {
  const InMemoryString *LastName = nullptr;
  auto *Ref = reinterpret_cast<NamedRef *>(this + 1);
  for (const NamedEntry &E : Entries) {
    assert(E.Ref);
    assert(E.Name);
    new (Ref++) NamedRef{E.Ref, E.Name};

    (void)(LastName);
    assert((!LastName || LastName->get() < E.Name->get()) &&
           "Expected names to be unique and sorted");
  }
  auto *Entry = reinterpret_cast<TreeEntry::EntryKind *>(Ref);
  for (const NamedEntry &E : Entries)
    new (Entry++) TreeEntry::EntryKind(E.Kind);
}

Optional<InMemoryTree::NamedEntry> InMemoryTree::find(StringRef Name) const {
  auto Compare = [](const NamedRef &LHS, StringRef RHS) {
    return LHS.Name->get().compare(RHS) < 0;
  };

  ArrayRef<NamedRef> Refs = getNamedRefs();
  const NamedRef *I = std::lower_bound(Refs.begin(), Refs.end(), Name, Compare);
  if (I == Refs.end() || I->Name->get() != Name)
    return None;
  return operator[](I - Refs.begin());
}

Expected<BlobProxy> InMemoryCAS::createBlobImpl(ArrayRef<uint8_t> ComputedHash,
                                                ArrayRef<char> Data) {
  // Look up the hash in the index, initializing to nullptr if it's new.
  auto &I = indexHash(ComputedHash);

  // Load or generate.
  auto Allocator = [&](size_t Size) -> void * {
    return Objects.Allocate(Size, alignof(InMemoryObject));
  };
  auto Generator = [&]() -> const InMemoryObject * {
    return &InMemoryInlineBlob::create(Allocator, I, Data);
  };

  return getBlobProxy(I.Data.loadOrGenerate(Generator));
}

Expected<BlobProxy> InMemoryCAS::createBlobFromNullTerminatedRegion(
    ArrayRef<uint8_t> ComputedHash, sys::fs::mapped_file_region Map) {
  // Look up the hash in the index, initializing to nullptr if it's new.
  ArrayRef<char> Data(Map.data(), Map.size());
  auto &I = indexHash(ComputedHash);

  // Load or generate.
  auto Allocator = [&](size_t Size) -> void * {
    return Objects.Allocate(Size, alignof(InMemoryObject));
  };
  auto Generator = [&]() -> const InMemoryObject * {
    return &InMemoryRefBlob::create(Allocator, I, Data);
  };
  const InMemoryObject &Object = I.Data.loadOrGenerate(Generator);

  // Save Map if the winning blob uses it.
  if (auto *Blob = dyn_cast<InMemoryRefBlob>(&Object))
    if (Blob->getData().data() == Map.data())
      new (MemoryMaps.Allocate()) sys::fs::mapped_file_region(std::move(Map));

  return getBlobProxy(Object);
}

Expected<NodeProxy> InMemoryCAS::createNodeImpl(ArrayRef<uint8_t> ComputedHash,
                                                ArrayRef<CASID> Refs,
                                                ArrayRef<char> Data) {
  // Look up the hash in the index, initializing to nullptr if it's new.
  auto &I = indexHash(ComputedHash);
  if (const InMemoryObject *Node = I.Data.load())
    return getNodeProxy(*Node);

  // Create the node.
  SmallVector<const InMemoryObject *> InternalRefs;
  for (CASID ID : Refs) {
    if (const InMemoryObject *Object = getObject(ID))
      InternalRefs.push_back(Object);
    else
      return createInvalidObjectError(ID, ObjectKind::Node);
  }
  auto Allocator = [&](size_t Size) -> void * {
    return Objects.Allocate(Size, alignof(InMemoryObject));
  };
  auto Generator = [&]() -> const InMemoryObject * {
    return &InMemoryInlineNode::create(Allocator, I, InternalRefs, Data);
  };
  return getNodeProxy(I.Data.loadOrGenerate(Generator));
}

const InMemoryString &InMemoryCAS::getOrCreateString(StringRef String) {
  InMemoryStringPoolT::value_type S = *StringPool.insertLazy(
      BuiltinObjectHasher<HasherT>::hashString(String),
      [&](auto ValueConstructor) { ValueConstructor.emplace(nullptr); });

  auto Allocator = [&](size_t Size) -> void * {
    return Strings.Allocate(Size, alignof(InMemoryString));
  };
  auto Generator = [&]() -> const InMemoryString * {
    return &InMemoryString::create(Allocator, String);
  };
  return S.Data.loadOrGenerate(Generator);
}

Expected<TreeProxy>
InMemoryCAS::createTreeImpl(ArrayRef<uint8_t> ComputedHash,
                            ArrayRef<NamedTreeEntry> SortedEntries) {
  // Look up the hash in the index, initializing to nullptr if it's new.
  auto &I = indexHash(ComputedHash);
  if (const InMemoryObject *Tree = I.Data.load())
    return getTreeProxy(*Tree);

  // Create the tree.
  SmallVector<InMemoryTree::NamedEntry> InternalEntries;
  for (const NamedTreeEntry &E : SortedEntries) {
    InternalEntries.push_back(
        {getObject(E.getID()), &getOrCreateString(E.getName()), E.getKind()});
    if (!InternalEntries.back().Ref)
      return createUnknownObjectError(E.getID());
  }
  auto Allocator = [&](size_t Size) -> void * {
    return Objects.Allocate(Size, alignof(InMemoryObject));
  };
  auto Generator = [&]() -> const InMemoryObject * {
    return &InMemoryTree::create(Allocator, I, InternalEntries);
  };
  return getTreeProxy(I.Data.loadOrGenerate(Generator));
}

Expected<CASID> InMemoryCAS::getCachedResult(CASID InputID) {
  InMemoryCacheT::pointer P = ActionCache.find(InputID.getHash());
  if (!P)
    return createResultCacheMissError(InputID);

  /// TODO: Although, consider inserting null on cache misses and returning a
  /// handle to avoid a second lookup!
  assert(P->Data && "Unexpected null in result cache");
  return getID(*P->Data);
}

Error InMemoryCAS::putCachedResult(CASID InputID, CASID OutputID) {
  const InMemoryIndexT::value_type &Expected = indexHash(OutputID.getHash());
  const InMemoryCacheT::value_type &Cached =
      *ActionCache.insertLazy(InputID.getHash(), [&](auto ValueConstructor) {
        ValueConstructor.emplace(&Expected);
      });

  /// TODO: Although, consider changing \a getCachedResult() to insert nullptr
  /// and returning a handle on cache misses!
  assert(Cached.Data && "Unexpected null in result cache");
  const InMemoryIndexT::value_type &Observed = *Cached.Data;
  if (&Expected == &Observed)
    return Error::success();

  return createResultCachePoisonedError(InputID, OutputID, getID(Observed));
}

Optional<NamedTreeEntry> InMemoryCAS::lookupInTree(const TreeProxy &Handle,
                                                   StringRef Name) const {
  auto &Tree = *reinterpret_cast<const InMemoryTree *>(getTreePtr(Handle));
  if (Optional<InMemoryTree::NamedEntry> E = Tree.find(Name))
    return makeTreeEntry(*E);
  return None;
}

NamedTreeEntry InMemoryCAS::getInTree(const TreeProxy &Handle, size_t I) const {
  auto &Tree = *reinterpret_cast<const InMemoryTree *>(getTreePtr(Handle));
  assert(I < Tree.size() && "Invalid index");
  return makeTreeEntry(Tree[I]);
}

Error InMemoryCAS::forEachEntryInTree(
    const TreeProxy &Handle,
    function_ref<Error(const NamedTreeEntry &)> Callback) const {
  auto &Tree = *reinterpret_cast<const InMemoryTree *>(getTreePtr(Handle));
  for (size_t I = 0, E = Tree.size(); I != E; ++I)
    if (Error E = Callback(makeTreeEntry(Tree[I])))
      return E;
  return Error::success();
}

CASID InMemoryCAS::getReferenceInNode(const NodeProxy &Handle, size_t I) const {
  auto &Node = *reinterpret_cast<const InMemoryNode *>(getNodePtr(Handle));
  assert(I < Node.getRefs().size() && "Invalid index");
  return getID(*Node.getRefs()[I]);
}

Error InMemoryCAS::forEachReferenceInNode(
    const NodeProxy &Handle, function_ref<Error(CASID)> Callback) const {
  auto &Node = *reinterpret_cast<const InMemoryNode *>(getNodePtr(Handle));
  for (const InMemoryObject *Object : Node.getRefs())
    if (Error E = Callback(getID(*Object)))
      return E;
  return Error::success();
}

std::unique_ptr<CASDB> cas::createInMemoryCAS() {
  return std::make_unique<InMemoryCAS>();
}

Expected<StringRef> InMemoryCAS::getNodeName(const NodeProxy &Node,
                                             size_t I) const {
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "Cannot list nodes for an InMemoryCAS");
}
