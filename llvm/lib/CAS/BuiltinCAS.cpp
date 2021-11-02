//===- BuiltinCAS.cpp -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/HashMappedTrie.h"
#include "llvm/CAS/OnDiskHashMappedTrie.h"
#include "llvm/CAS/ThreadSafeAllocator.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include "llvm/Support/StringSaver.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

static constexpr size_t NumHashBytes = 20;
using HashType = std::array<uint8_t, NumHashBytes>;
using HashRef = ArrayRef<uint8_t>;

class BuiltinNamespace : public Namespace {
  BuiltinNamespace() : Namespace("llvm.builtin.cas[sha1]", NumHashBytes) {}

public:
  void printID(const UniqueIDRef &ID, raw_ostream &OS) const override;
  Error parseID(StringRef Reference, UniqueID &OS) const override;
};

struct HashInfo {
  template <class HasherT> static auto hash(HashType Data) { return Data; }

  template <class HasherT> using HashType = decltype(hash<HasherT>(HashType()));
};

class BuiltinCAS : public CASDB {
public:
  class TempFile;

  using MappedContentReference = OnDiskHashMappedTrie::MappedContentReference;
  struct ContentReference {
    StringRef Metadata;
    StringRef Data;

    ContentReference() = delete;
    ContentReference(StringRef Metadata, StringRef Data)
        : Metadata(Metadata), Data(Data) {}
  };

  /// How objects are stored in memory.
  struct ObjectContentReference {
    StringRef ReferenceBlock;
    StringRef Data;

    size_t getNumReferences() const {
      return ReferenceBlock.size() / NumHashBytes;
    }
    CASID getReference(size_t I) const;
    static Optional<ObjectContentReference>
    getFromContent(ContentReference Content);
  };

  /// A CASID plus a reference to an object stored in memory.
  struct ObjectAndID {
    const ObjectContentReference &Object;
    CASID ID;
  };

  /// List of references, either as CASIDs or a contiguous block.
  struct AbstractReferenceList {
    size_t getNumBytes() const;
    size_t getNumReferences() const;
    void appendTo(SmallVectorImpl<char> &Sink) const;
    void updateHash(SHA1 &Hasher) const;

    AbstractReferenceList() = delete;
    AbstractReferenceList(StringRef ReferenceBlock)
        : ReferenceBlock(ReferenceBlock) {}
    AbstractReferenceList(ArrayRef<CASID> ReferenceList)
        : ReferenceList(ReferenceList) {}
    AbstractReferenceList(NoneType) : ReferenceList(ArrayRef<CASID>()) {}

    Optional<StringRef> ReferenceBlock;
    Optional<ArrayRef<CASID>> ReferenceList;
  };

  struct TreeObjectData;

  /// An object, which may or may not exist yet.
  struct AbstractObjectReference {
    AbstractReferenceList References;
    StringRef Data;

    // Return combined references and data.
    StringRef build(SmallVectorImpl<char> &Metadata,
                    SmallVectorImpl<char> &DataStorage) const;
    HashType computeHash() const;

    AbstractObjectReference(AbstractReferenceList References, StringRef Data)
        : References(References), Data(Data) {}
    AbstractObjectReference(ObjectContentReference Object)
        : References(Object.ReferenceBlock), Data(Object.Data) {}
    AbstractObjectReference(const TreeObjectData &Object);
  };

  Expected<ObjectAndID>
  createObject(AbstractObjectReference Object,
               Optional<sys::fs::mapped_file_region> NullTerminatedMap = None);

  Expected<BlobRef> createBlob(StringRef Data) final {
    return createBlobImpl(Data, None);
  }
  Expected<BlobRef> createBlobImpl(
      StringRef Data,
      Optional<sys::fs::mapped_file_region> NullTerminatedMap = None);
  Expected<TreeRef> createTree(ArrayRef<NamedTreeEntry> Entries = None) final;
  bool isKnownObject(CASID ID) final;
  Optional<ObjectKind> getObjectKind(CASID ID) final;

  Expected<BlobRef>
  createBlobFromOpenFileImpl(sys::fs::file_t FD,
                             Optional<sys::fs::file_status> Status) override;

  Expected<ObjectAndID> getObject(CASID ID);
  Expected<BlobRef> getBlob(CASID ID) final {
    return getBlobFromObject(getObject(ID));
  }

  Expected<NodeRef> getNodeFromObject(Expected<ObjectAndID> Object);
  Expected<NodeRef> createNode(ArrayRef<CASID> References,
                               StringRef Data) final;
  Expected<NodeRef> getNode(CASID ID) final {
    return getNodeFromObject(getObject(ID));
  }

  static Expected<BlobRef> getBlobFromObject(Expected<ObjectAndID> Object);

  Expected<TreeRef> getTreeFromObject(Expected<ObjectAndID> Object);

  Expected<TreeRef> getTree(CASID ID) final {
    return getTreeFromObject(getObject(ID));
  }

  bool isInMemoryOnly() const { return !RootPath; }

  void print(raw_ostream &OS) const final;

  Expected<CASID> getCachedResult(CASID InputID) final;
  Error putCachedResult(CASID InputID, CASID OutputID) final;

  struct InMemoryOnlyTag {};
  BuiltinCAS() = delete;
  explicit BuiltinCAS(InMemoryOnlyTag) { AlignedInMemoryStrings.emplace(); }
  explicit BuiltinCAS(StringRef RootPath,
                      std::shared_ptr<OnDiskHashMappedTrie> OnDiskObjects,
                      std::shared_ptr<OnDiskHashMappedTrie> OnDiskResults)
      : OnDiskObjects(std::move(OnDiskObjects)),
        OnDiskResults(std::move(OnDiskResults)),
        RootPath(RootPath.str()) {
    SmallString<128> Temp = RootPath;
    sys::path::append(Temp, "tmp.");
    TempPrefix = Temp.str().str();
  }

private:
  // TreeAPI.
  Optional<NamedTreeEntry> lookupInTree(const TreeRef &Tree,
                                        StringRef Name) const final;
  NamedTreeEntry getInTree(const TreeRef &Tree, size_t I) const final;
  Error forEachEntryInTree(
      const TreeRef &Tree,
      function_ref<Error(const NamedTreeEntry &)> Callback) const final;

  // NodeAPI.
  CASID getReferenceInNode(const NodeRef &Ref, size_t I) const final;
  Error forEachReferenceInNode(const NodeRef &Ref,
                               function_ref<Error(CASID)> Callback) const final;

  // Other stuff.

  ContentReference getOrCreatePersistentContent(
      HashRef Hash, StringRef Metadata, StringRef Data,
      Optional<sys::fs::mapped_file_region> NullTerminatedDataMap = None);

  /// Return a version of \p Data that is persistent.
  ///
  /// If \p NullTerminatedMap is sent in, it must be a valid, null-terminated
  /// map that \p Data points to. It will be stored in memory and \p Data
  /// returned.
  ///
  /// Otherwise, a copy of \p Data will be bump-ptr-allocated and returned.
  StringRef persistBufferInMemory(
      StringRef Buffer, bool ShouldNullTerminate,
      Optional<sys::fs::mapped_file_region> NullTerminatedMap);

  StringRef persistMetadataInMemory(StringRef Metadata) {
    return persistBufferInMemory(Metadata, /*ShouldNullTerminate=*/false, None);
  }

  StringRef persistDataInMemory(
      StringRef Data,
      Optional<sys::fs::mapped_file_region> NullTerminatedMap = None) {
    return persistBufferInMemory(Data, /*ShouldNullTerminate=*/true,
                                 std::move(NullTerminatedMap));
  }

  /// Return the buffer from \p Content after making the map (if any)
  /// persistent.
  ContentReference
  persistMappedContentInMemory(MappedContentReference Content) {
    return ContentReference(
        Content.Metadata,
        Content.Map ? persistDataInMemory(Content.Data, std::move(Content.Map))
                    : Content.Data);
  }

  StringRef getPathForID(StringRef BaseDir, CASID ID,
                         SmallVectorImpl<char> &Storage);

  Expected<std::unique_ptr<MemoryBuffer>> openFile(StringRef Path);
  Expected<std::unique_ptr<MemoryBuffer>> openFileWithID(StringRef BaseDir,
                                                         CASID ID);

  Optional<MappedContentReference> openOnDisk(OnDiskHashMappedTrie &Trie,
                                              HashRef Hash);

  std::shared_ptr<OnDiskHashMappedTrie> OnDiskObjects;
  std::shared_ptr<OnDiskHashMappedTrie> OnDiskResults;

  using ObjectCacheType =
      ThreadSafeHashMappedTrie<ObjectContentReference, HashType>;
  ObjectCacheType ObjectCache;

  using ResultCacheType = ThreadSafeHashMappedTrie<HashType, HashType>;
  ResultCacheType ResultCache;

  Optional<std::string> RootPath;
  Optional<std::string> TempPrefix;

  Optional<ThreadSafeAllocator<SpecificBumpPtrAllocator<char>>>
      AlignedInMemoryStrings;
  ThreadSafeAllocator<SpecificBumpPtrAllocator<HashType>> ParsedIDs;
  ThreadSafeAllocator<SpecificBumpPtrAllocator<sys::fs::mapped_file_region>>
      PersistentMaps;
};

} // namespace

static const Namespace &getBuiltinNamespace() {
  static BuiltinNamespace NS;
  return NS;
}

using TreeObjectData = BuiltinCAS::TreeObjectData;
using ObjectContentReference = BuiltinCAS::ObjectContentReference;
using ObjectAndID = BuiltinCAS::ObjectAndID;
using ContentReference = BuiltinCAS::ContentReference;
using AbstractReferenceList = BuiltinCAS::AbstractReferenceList;
using AbstractObjectReference = BuiltinCAS::AbstractObjectReference;

template <typename T> static T reportAsFatalIfError(Expected<T> ValOrErr) {
  if (!ValOrErr)
    report_fatal_error(ValOrErr.takeError());
  return std::move(*ValOrErr);
}

static void reportAsFatalIfError(Error E) {
  if (E)
    report_fatal_error(std::move(E));
}

static HashRef bufferAsRawHash(StringRef Bytes) {
  assert(Bytes.size() == NumHashBytes);
  return HashRef(reinterpret_cast<const uint8_t *>(Bytes.begin()),
                 Bytes.size());
}

static StringRef rawHashAsBuffer(HashRef Hash) {
  assert(Hash.size() == NumHashBytes);
  return StringRef(reinterpret_cast<const char *>(Hash.begin()), Hash.size());
}

static HashType makeHash(HashRef Bytes) {
  assert(Bytes.size() == NumHashBytes);
  HashType Hash;
  ::memcpy(Hash.begin(), Bytes.begin(), Bytes.size());
  return Hash;
}

static HashType makeHash(CASID ID) { return makeHash(ID.getHash()); }

static void extractPrintableHash(CASID ID, SmallVectorImpl<char> &Dest) {
  ArrayRef<uint8_t> RawHash = ID.getHash();
  assert(Dest.empty());
  assert(RawHash.size() == 20);
  Dest.reserve(40);
  auto ToChar = [](uint8_t Bit) {
    if (Bit < 10)
      return '0' + Bit;
    return Bit - 10 + 'a';
  };
  for (uint8_t Bit : RawHash) {
    uint8_t High = Bit >> 4;
    uint8_t Low = Bit & 0xf;
    Dest.push_back(ToChar(High));
    Dest.push_back(ToChar(Low));
  }
}

static HashType stringToHash(StringRef Chars) {
  auto FromChar = [](char Ch) -> unsigned {
    if (Ch >= '0' && Ch <= '9')
      return Ch - '0';
    assert(Ch >= 'a');
    assert(Ch <= 'f');
    return Ch - 'a' + 10;
  };

  HashType Hash;
  assert(Chars.size() == sizeof(Hash) * 2);
  for (int I = 0, E = sizeof(Hash); I != E; ++I) {
    uint8_t High = FromChar(Chars[I * 2]);
    uint8_t Low = FromChar(Chars[I * 2 + 1]);
    Hash[I] = (High << 4) | Low;
  }
  return Hash;
}

static StringRef getCASIDPrefix() { return "~{CASFS}:"; }

void BuiltinNamespace::printIDImpl(const UniqueIDRef &ID, raw_ostream &OS) const override {
  assert(&ID.getNamespace() == this && "Wrong namespace");

  SmallString<64> Hash;
  extractPrintableHash(CASID(ID.getHash()), Hash);
  OS << getCASIDPrefix() << Hash;
}

Error BuiltinNamespace::parseID(StringRef Reference, UniqueID &ID) {
  if (!Reference.consume_front(getCASIDPrefix()))
    return createStringError(std::make_error_code(std::errc::invalid_argument),
                             "invalid cas-id '" + Reference + "'");

  // FIXME: Allow shortened references?
  if (Reference.size() != 2 * NumHashBytes)
    return createStringError(std::make_error_code(std::errc::invalid_argument),
                             "wrong size for cas-id hash '" + Reference + "'");

  ID = UniqueIDRef(*this, HashType(stringToHash(Reference)));
  return Error::success();
}

void BuiltinCAS::print(raw_ostream &OS) const {
  if (RootPath)
    OS << "on-disk-root-path: " << *RootPath << "\n";
  OS << "in-memory-objects: ";
  ObjectCache.print(OS);
  OS << "in-memory-results: ";
  ResultCache.print(OS);
}

static Error createUnknownObjectError(CASID ID) {
  SmallString<64> Hash;
  extractPrintableHash(ID, Hash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "unknown object '" + Hash + "'");
}

static Error createCorruptObjectError(CASID ID) {
  SmallString<64> Hash;
  extractPrintableHash(ID, Hash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "corrupt object '" + Hash + "'");
}

static Error createInvalidObjectError(CASID ID, StringRef Kind) {
  SmallString<64> Hash;
  extractPrintableHash(ID, Hash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "invalid object '" + Hash + "' for kind '" + Kind + "'");
}

// Stable numbers for serializing trees.
constexpr static uint32_t EntryKindIDForTree = 1;
constexpr static uint32_t EntryKindIDForRegular = 2;
constexpr static uint32_t EntryKindIDForExecutable = 3;
constexpr static uint32_t EntryKindIDForSymlink = 4;

constexpr static StringLiteral CASTreeMagic = "\xcas-tree\001";
static_assert(CASTreeMagic.size() % alignof(uint64_t) == 0,
              "Magic not 8-byte aligned");

static void printTree(raw_ostream &OS, ArrayRef<NamedTreeEntry> SortedEntries,
                      SmallVectorImpl<CASID> &References) {
  // Hash input has format:
  //
  //  input := header unnamed-entry* name*
  //  header := 'cas-tree' num-entries
  //  unnamed-entry := kind name-offset
  //  name := name-string NULL
  OS << CASTreeMagic;
  support::endian::Writer EW(OS, support::endianness::little);
  uint64_t StringsOffset = 0;
  for (const NamedTreeEntry &Entry : SortedEntries) {
    assert(Entry.getID().getHash().size() == NumHashBytes);
    References.push_back(Entry.getID());

    uint64_t Kind = 0;
    switch (Entry.getKind()) {
    case TreeEntry::Regular:
      Kind = EntryKindIDForRegular;
      break;
    case TreeEntry::Tree:
      Kind = EntryKindIDForTree;
      break;
    case TreeEntry::Executable:
      Kind = EntryKindIDForExecutable;
      break;
    case TreeEntry::Symlink:
      Kind = EntryKindIDForSymlink;
      break;
    }
    assert(Kind < (1u << 8) && "Kind needs to be 8 bits");

    assert(StringsOffset < (1ull << 56) && "Absurdly large string?");
    EW.write(StringsOffset | (Kind << 56));
    StringsOffset +=
        alignTo(Entry.getName().size() + 1, Align(4)) + sizeof(uint32_t);
  }
  for (const NamedTreeEntry &Entry : SortedEntries) {
    EW.write(uint32_t(Entry.getName().size()));
    OS << Entry.getName();
    for (int I = 0, E = alignTo(Entry.getName().size() + 1, Align(4)) -
                        Entry.getName().size();
         I != E; ++I)
      OS.write(0);
  }
}

CASID BuiltinCAS::ObjectContentReference::getReference(size_t I) const {
  assert(I < getNumReferences() && "Reference out of range");
  return CASID(
      bufferAsRawHash(ReferenceBlock.substr(I * NumHashBytes, NumHashBytes)));
}

struct BuiltinCAS::TreeObjectData {
  Optional<ArrayRef<CASID>> References;
  Optional<StringRef> Data;

  Optional<SmallVector<CASID, 16>> ReferenceStorage;
  Optional<SmallString<1024>> DataStorage;
};

BuiltinCAS::AbstractObjectReference::AbstractObjectReference(
    const TreeObjectData &Object)
    : References(*Object.References), Data(*Object.Data) {}

static bool isValidTree(ObjectContentReference Object) {
  // Check for magic.
  StringRef Data = Object.Data;
  if (!Data.consume_front(CASTreeMagic))
    return false;

  // Check for an empty tree.
  uint64_t NumEntries = Object.getNumReferences();
  if (!NumEntries)
    return Data.empty();

  // Check that there's space for the strings table.
  //
  // TODO: Sanity check a couple of tree entries.
  const size_t EntrySize = sizeof(uint64_t);
  return NumEntries * EntrySize < Data.size();
}

static Optional<NamedTreeEntry> getTreeEntryImpl(ObjectContentReference Object,
                                                 size_t I) {
  uint64_t NumEntries = Object.getNumReferences();
  assert(I < NumEntries);
  CASID ID = Object.getReference(I);

  assert(Object.Data.size() >= CASTreeMagic.size() && "Expected valid tree");
  const char *Start = Object.Data.begin() + CASTreeMagic.size();

  const size_t EntrySize = sizeof(uint64_t);
  size_t StringsTableOffset = NumEntries * EntrySize;
  assert(ptrdiff_t(StringsTableOffset) < Object.Data.end() - Start &&
         "Expected valid tree");
  const char *StringsTable = Start + StringsTableOffset;

  using namespace llvm::support;
  uint64_t EntryData = endian::read<uint64_t, endianness::little, aligned>(
      Start + I * EntrySize);
  uint64_t KindID = EntryData >> 56;
  uint64_t StringOffset = EntryData & ((1ull << 56) - 1ull);

  // Get the kind.
  Optional<TreeEntry::EntryKind> Kind;
  switch (KindID) {
  default:
    return None;
  case EntryKindIDForRegular:
    Kind = TreeEntry::Regular;
    break;
  case EntryKindIDForTree:
    Kind = TreeEntry::Tree;
    break;
  case EntryKindIDForExecutable:
    Kind = TreeEntry::Executable;
    break;
  case EntryKindIDForSymlink:
    Kind = TreeEntry::Symlink;
    break;
  }

  // Get the name.
  if (ptrdiff_t(StringOffset + sizeof(uint32_t)) >
      Object.Data.end() - StringsTable)
    return None;
  const char *String = StringsTable + StringOffset;
  uint32_t StringSize =
      endian::readNext<uint32_t, endianness::little, aligned>(String);
  if (ptrdiff_t(StringSize) + 1 > Object.Data.end() - String)
    return None;
  if (String[StringSize] != 0)
    return None;
  StringRef Name(String, StringSize);

  return NamedTreeEntry(ID, *Kind, Name);
}

static NamedTreeEntry getTreeEntry(ObjectContentReference Object, size_t I) {
  Optional<NamedTreeEntry> MaybeEntry = getTreeEntryImpl(Object, I);

  // FIXME: probably want to a report a corrupt object?
  assert(MaybeEntry && "Parse error");
  return *MaybeEntry;
}

static TreeObjectData makeTree(ArrayRef<NamedTreeEntry> SortedEntries) {
  TreeObjectData Tree;
  {
    // Reserve enough references.
    Tree.ReferenceStorage.emplace();
    Tree.ReferenceStorage->reserve(SortedEntries.size());

    // Reserve enough chars.
    size_t NumChars = CASTreeMagic.size();
    for (auto &E : SortedEntries)
      NumChars += sizeof(uint64_t) + sizeof(uint32_t) +
                  alignTo(E.getName().size() + sizeof(char), Align(4));
    Tree.DataStorage.emplace();
    Tree.DataStorage->reserve(NumChars);

    // Print the tree.
    raw_svector_ostream OS(*Tree.DataStorage);
    printTree(OS, SortedEntries, *Tree.ReferenceStorage);
    assert(Tree.DataStorage->size() == NumChars && "Reservation doesn't match");
  }
  assert(Tree.ReferenceStorage->size() == SortedEntries.size() && "Reservation doesn't match");

  Tree.Data = *Tree.DataStorage;
  Tree.References = *Tree.ReferenceStorage;
  return Tree;
}

HashType AbstractObjectReference::computeHash() const {
  SmallString<sizeof(uint64_t) * 3> Sizes;
  {
    raw_svector_ostream OS(Sizes);
    llvm::support::endian::Writer EW(OS, support::endianness::little);
    EW.write(uint64_t(References.getNumBytes()));
    EW.write(uint64_t(Data.size()));
  }

  SHA1 Hasher;
  Hasher.update(Sizes);
  References.updateHash(Hasher);
  Hasher.update(Data);
  return makeHash(bufferAsRawHash(Hasher.final()));
}

size_t BuiltinCAS::AbstractReferenceList::getNumBytes() const {
  return ReferenceBlock ? ReferenceBlock->size()
                        : ReferenceList->size() * NumHashBytes;
}

size_t BuiltinCAS::AbstractReferenceList::getNumReferences() const {
  return ReferenceBlock ? ReferenceBlock->size() / NumHashBytes
                        : ReferenceList->size();
}

void BuiltinCAS::AbstractReferenceList::appendTo(
    SmallVectorImpl<char> &Sink) const {
  if (ReferenceBlock) {
    Sink.append(ReferenceBlock->begin(), ReferenceBlock->end());
    return;
  }
  for (CASID ID : *ReferenceList) {
    auto Hash = rawHashAsBuffer(ID.getHash());
    assert(Hash.size() == NumHashBytes);
    Sink.append(Hash.begin(), Hash.end());
  }
}

void BuiltinCAS::AbstractReferenceList::updateHash(SHA1 &Hasher) const {
  if (ReferenceBlock) {
    Hasher.update(*ReferenceBlock);
    return;
  }
  for (CASID ID : *ReferenceList) {
    assert(ID.getHash().size() == NumHashBytes);
    Hasher.update(ID.getHash());
  }
}

StringRef BuiltinCAS::AbstractObjectReference::build(
    SmallVectorImpl<char> &Metadata, SmallVectorImpl<char> &DataStorage) const {
  assert(Metadata.empty());
  assert(DataStorage.empty());

  Metadata.reserve(sizeof(uint64_t) * 2);
  raw_svector_ostream OS(Metadata);
  support::endian::Writer EW(OS, support::endianness::little);
  EW.write(uint64_t(References.getNumReferences()));
  EW.write(uint64_t(Data.size()));

  // No references. No need to copy out data.
  if (!References.getNumReferences())
    return Data;

  // Reserve enough space for the reference block, padding for 8 byte
  // alignment, and the data.
  uint64_t ReferenceBlockSize = References.getNumBytes();
  uint64_t StoragePadding =
      alignTo(ReferenceBlockSize, Align(8)) - ReferenceBlockSize;
  assert(StoragePadding < 8);
  uint64_t ExpectedStorageSize =
      ReferenceBlockSize + StoragePadding + Data.size();
  DataStorage.reserve(ExpectedStorageSize);

  // Build the references and data.
  References.appendTo(DataStorage);
  assert(DataStorage.size() == ReferenceBlockSize);
  if (StoragePadding)
    DataStorage.resize(DataStorage.size() + StoragePadding, 0);
  DataStorage.append(Data.begin(), Data.end());
  assert(ExpectedStorageSize == DataStorage.size());
  return StringRef(DataStorage.begin(), DataStorage.size());
}

Optional<ObjectContentReference>
ObjectContentReference::getFromContent(ContentReference Content) {
  if (Content.Metadata.size() < sizeof(uint64_t) * 2)
    return None;

  using namespace llvm::support;

  // Read the metadata.
  const char *Current = Content.Metadata.begin();
  size_t NumReferences =
      endian::readNext<uint64_t, endianness::little, aligned>(Current);
  size_t DataSize =
      endian::readNext<uint64_t, endianness::little, aligned>(Current);
  ObjectContentReference Object;

  // Read the reference block and data.
  uint64_t ReferenceBlockSize = NumReferences * NumHashBytes;
  uint64_t StoragePadding =
      alignTo(ReferenceBlockSize, Align(8)) - ReferenceBlockSize;
  if (Content.Data.size() != ReferenceBlockSize + StoragePadding + DataSize)
    return None; // Error.
  Object.ReferenceBlock = Content.Data.take_front(ReferenceBlockSize);
  Object.Data = Content.Data.take_back(DataSize);
  return Object;
}

bool BuiltinCAS::isKnownObject(CASID ID) {
  return !errorToBool(getObject(ID).takeError());
}

Optional<ObjectKind> BuiltinCAS::getObjectKind(CASID ID) {
  Expected<ObjectAndID> ExpectedObject = getObject(ID);
  if (!ExpectedObject) {
    consumeError(ExpectedObject.takeError());
    return None;
  }
  if (!errorToBool(getTreeFromObject(*ExpectedObject).takeError()))
    return ObjectKind::Tree;
  if (!errorToBool(getBlobFromObject(*ExpectedObject).takeError()))
    return ObjectKind::Blob;
  return ObjectKind::Node;
}

Expected<ObjectAndID> BuiltinCAS::getObject(CASID ID) {
  HashRef Hash = ID.getHash();
  auto Lookup = ObjectCache.lookup(Hash);
  if (Lookup)
    return ObjectAndID{Lookup->Data, CASID(Lookup->Hash)};

  if (isInMemoryOnly())
    return createUnknownObjectError(ID);

  Optional<MappedContentReference> File = openOnDisk(*OnDiskObjects, Hash);
  if (!File)
    return createUnknownObjectError(ID);

  ContentReference Content = persistMappedContentInMemory(std::move(*File));
  Optional<ObjectContentReference> Object =
      ObjectContentReference::getFromContent(Content);
  if (!Object)
    return createCorruptObjectError(ID);

  auto &Insertion = ObjectCache.insert(
      Lookup, ObjectCacheType::HashedDataType(makeHash(Hash), *Object));
  return ObjectAndID{Insertion.Data, CASID(Insertion.Hash)};
}

Expected<BlobRef> BuiltinCAS::getBlobFromObject(Expected<ObjectAndID> Object) {
  if (!Object)
    return Object.takeError();

  if (Object->Object.getNumReferences())
    return createInvalidObjectError(Object->ID, "blob");

  return makeBlobRef(Object->ID, Object->Object.Data);
}

Expected<TreeRef> BuiltinCAS::getTreeFromObject(Expected<ObjectAndID> Object) {
  if (!Object)
    return Object.takeError();

  if (!isValidTree(Object->Object))
    return createInvalidObjectError(Object->ID, "tree");

  return makeTreeRef(Object->ID, &Object->Object,
                     Object->Object.getNumReferences());
}

Optional<NamedTreeEntry> BuiltinCAS::lookupInTree(const TreeRef &Tree,
                                                  StringRef Name) const {
  // FIXME: Consider adding an on-disk hash table for large trees.
  auto &Object = *static_cast<const ObjectContentReference *>(getTreePtr(Tree));

  // Start with a binary search, if there are enough entries.
  const size_t MaxLinearSearchSize = 4;
  size_t First = 0, Last = Object.getNumReferences();
  while (Last - First > MaxLinearSearchSize) {
    size_t I = (First + Last) / 2;
    NamedTreeEntry Entry = getTreeEntry(Object, I);
    switch (Name.compare(Entry.getName())) {
    case 0:
      return Entry;
    case -1:
      Last = I;
      break;
    case 1:
      First = I + 1;
      break;
    }
  }

  // Use a linear search for small trees.
  for (; First != Last; ++First) {
    NamedTreeEntry Entry = getTreeEntry(Object, First);
    if (Name == Entry.getName())
      return Entry;
  }

  return None;
}

NamedTreeEntry BuiltinCAS::getInTree(const TreeRef &Tree, size_t I) const {
  auto &Object = *static_cast<const ObjectContentReference *>(getTreePtr(Tree));
  return getTreeEntry(Object, I);
}

Error BuiltinCAS::forEachEntryInTree(
    const TreeRef &Tree,
    function_ref<Error(const NamedTreeEntry &)> Callback) const {
  auto &Object = *static_cast<const ObjectContentReference *>(getTreePtr(Tree));
  for (size_t I = 0, E = Object.getNumReferences(); I != E; ++I)
    if (Error E = Callback(getTreeEntry(Object, I)))
      return E;
  return Error::success();
}

Expected<TreeRef> BuiltinCAS::createTree(ArrayRef<NamedTreeEntry> Entries) {
  SmallVector<NamedTreeEntry> Sorted(Entries.begin(), Entries.end());

  // Ensure a stable order for tree entries and ignore name collisions.
  std::stable_sort(Sorted.begin(), Sorted.end());
  Sorted.erase(std::unique(Sorted.begin(), Sorted.end()), Sorted.end());

  // Check each name is valid.
  //
  // FIXME: Is this too expensive? Maybe we don't really care...
  for (const NamedTreeEntry &Entry : Sorted)
    if (Entry.getName().find_first_of("/\0\n") != StringRef::npos)
      return errorCodeToError(
          std::make_error_code(std::errc::invalid_argument));

  return getTreeFromObject(createObject(makeTree(Sorted)));
}

Expected<NodeRef> BuiltinCAS::createNode(ArrayRef<CASID> References,
                                         StringRef Data) {
  return getNodeFromObject(
      createObject(AbstractObjectReference(References, Data)));
}

Expected<NodeRef> BuiltinCAS::getNodeFromObject(Expected<ObjectAndID> Object) {
  if (!Object)
    return Object.takeError();

  return makeNodeRef(Object->ID, &Object->Object,
                     Object->Object.getNumReferences(), Object->Object.Data);
}

CASID BuiltinCAS::getReferenceInNode(const NodeRef &Ref, size_t I) const {
  auto &Object = *static_cast<const ObjectContentReference *>(getNodePtr(Ref));
  return Object.getReference(I);
}

Error BuiltinCAS::forEachReferenceInNode(
    const NodeRef &Ref, function_ref<Error(CASID)> Callback) const {
  auto &Object = *static_cast<const ObjectContentReference *>(getNodePtr(Ref));
  for (size_t I = 0, E = Object.getNumReferences(); I != E; ++I)
    if (Error E = Callback(Object.getReference(I)))
      return E;
  return Error::success();
}

Expected<BlobRef>
BuiltinCAS::createBlobFromOpenFileImpl(sys::fs::file_t FD,
                                       Optional<sys::fs::file_status> Status) {
#if defined(__CYGWIN__)
  static int PageSize = 4096;
#else
  static int PageSize = sys::Process::getPageSizeEstimate();
#endif

  if (!Status) {
    Status.emplace();
    if (std::error_code EC = sys::fs::status(FD, *Status))
      return errorCodeToError(EC);
  }

  // Check whether we can trust the size from stat.
  Optional<uint64_t> StatusFileSize;
  if (Status->type() == sys::fs::file_type::regular_file ||
      Status->type() == sys::fs::file_type::block_file)
    StatusFileSize = Status->getSize();

  sys::fs::mapped_file_region MappedContent;
  SmallString<sys::fs::DefaultReadChunkSize> StreamedContent;
  Optional<StringRef> Content;
  if (StatusFileSize && *StatusFileSize > 4u * 4096) {
    std::error_code EC;
    MappedContent = sys::fs::mapped_file_region(
        FD, sys::fs::mapped_file_region::readonly, *StatusFileSize,
        /*offset=*/0, EC);
    if (EC)
      return errorCodeToError(EC);
    Content.emplace(MappedContent.data(), MappedContent.size());
  } else {
    if (Error E = sys::fs::readNativeFileToEOF(FD, StreamedContent))
      return std::move(E);
    Content.emplace(StreamedContent);
  }

  // If we used mmap, check if it's actually null-terminated. Note that the
  // contents could have changed between stat and open if this file is
  // volatile, so we need to check for a null character.
  //
  // If the map *is* null-terminated, pass it in to allow an in-memory trie to
  // use the file-backed storage (in case we aren't file-backed ourselves).
  Optional<sys::fs::mapped_file_region> NullTerminatedMap;
  if (MappedContent && (*StatusFileSize & (PageSize - 1)) &&
      Content->end()[0] == 0)
    NullTerminatedMap = std::move(MappedContent);

  return createBlobImpl(*Content, std::move(NullTerminatedMap));
}

Expected<ObjectAndID> BuiltinCAS::createObject(
    AbstractObjectReference Object,
    Optional<sys::fs::mapped_file_region> NullTerminatedMap) {
  HashType Hash = Object.computeHash();
  auto Lookup = ObjectCache.lookup(Hash);
  if (Lookup) {
    assert(Object.Data == Lookup->Data.Data);
    return ObjectAndID{Lookup->Data, CASID(Lookup->Hash)};
  }

  SmallString<256> Metadata;
  SmallString<1024> DataStorage;

  // Build the metadata and data. We can reuse the null-terminated map only if
  // the data wasn't moved to DataStorage.
  StringRef ReferenceBlockAndData = Object.build(Metadata, DataStorage);
  Optional<sys::fs::mapped_file_region> DataMapToPersist;
  if (DataStorage.empty())
    DataMapToPersist = std::move(NullTerminatedMap);

  ContentReference Content = getOrCreatePersistentContent(
      Hash, Metadata, ReferenceBlockAndData, std::move(DataMapToPersist));
  Optional<ObjectContentReference> PersistentObject =
      ObjectContentReference::getFromContent(Content);
  assert(PersistentObject && "Expected persistant object to be valid");

  auto &Insertion = ObjectCache.insert(
      Lookup, ObjectCacheType::HashedDataType(Hash, *PersistentObject));
  return ObjectAndID{Insertion.Data, CASID(Insertion.Hash)};
}

Expected<BlobRef> BuiltinCAS::createBlobImpl(
    StringRef Data, Optional<sys::fs::mapped_file_region> NullTerminatedMap) {
  return getBlobFromObject(
      createObject(AbstractObjectReference(None, Data),
                   std::move(NullTerminatedMap)));
}

ContentReference BuiltinCAS::getOrCreatePersistentContent(
    HashRef Hash, StringRef Metadata, StringRef Data,
    Optional<sys::fs::mapped_file_region> NullTerminatedDataMap) {
  assert(isInMemoryOnly() == (OnDiskObjects == nullptr));
  if (OnDiskObjects)
    return persistMappedContentInMemory(
        OnDiskObjects->insert(Hash, Metadata, Data));
  return ContentReference(
      persistMetadataInMemory(Metadata),
      persistDataInMemory(Data, std::move(NullTerminatedDataMap)));
}

StringRef BuiltinCAS::persistBufferInMemory(
    StringRef Buffer, bool ShouldNullTerminate,
    Optional<sys::fs::mapped_file_region> NullTerminatedMap) {
  if (NullTerminatedMap) {
    assert(NullTerminatedMap->data() == Buffer.begin());
    assert(Buffer.end()[0] == 0);
    new (PersistentMaps.Allocate())
        sys::fs::mapped_file_region(std::move(*NullTerminatedMap));
    return Buffer;
  }

  auto GetPadding = [](uint64_t Size) -> uint64_t {
    if (uint64_t Remainder = Size % 8)
      return 8 - Remainder;
    return 0;
  };
  const uint64_t PaddingSize = ShouldNullTerminate
                                   ? (GetPadding(Buffer.size() + 1) + 1)
                                   : GetPadding(Buffer.size());

  // Use the in-memory allocator for content. This is only expected when we
  // don't have on-disk objects.
  assert(isInMemoryOnly());
  char *InMemoryBuffer =
      AlignedInMemoryStrings->Allocate(Buffer.size() + PaddingSize);
  ::memcpy(InMemoryBuffer, Buffer.begin(), Buffer.size());
  ::memset(InMemoryBuffer + Buffer.size(), 0, PaddingSize);
  return StringRef(InMemoryBuffer, Buffer.size());
}

static Error createResultCacheMissError(CASID InputID) {
  SmallString<64> InputHash;
  extractPrintableHash(InputID, InputHash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "no result for '" + InputHash + "'");
}

static Error createResultCachePoisonedError(CASID InputID, CASID OutputID,
                                            CASID ExistingOutputID) {
  SmallString<64> InputHash, OutputHash, ExistingOutputHash;
  extractPrintableHash(InputID, InputHash);
  extractPrintableHash(OutputID, OutputHash);
  extractPrintableHash(ExistingOutputID, ExistingOutputHash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "cache poisoned for '" + InputHash + "' (new='" +
                               OutputHash + "' vs. existing '" +
                               ExistingOutputHash + "')");
}

static Error createResultCacheCorruptError(CASID InputID) {
  SmallString<64> InputHash;
  extractPrintableHash(InputID, InputHash);
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "result cache corrupt for '" + InputHash + "'");
}

Expected<CASID> BuiltinCAS::getCachedResult(CASID InputID) {
  auto Lookup = ResultCache.lookup(InputID.getHash());
  if (Lookup)
    return CASID(Lookup->Data);

  // FIXME: Odd to have an error for a cache miss. Maybe this function should
  // just return Optional.
  if (isInMemoryOnly())
    return createResultCacheMissError(InputID);

  Optional<MappedContentReference> File =
      openOnDisk(*OnDiskResults, InputID.getHash());
  if (!File)
    return createResultCacheMissError(InputID);
  if (File->Metadata != "results") // FIXME: Should cache this object.
    return createCorruptObjectError(InputID);

  // Drop File.Map on the floor since we're storing a std::string.
  if (File->Data.size() != NumHashBytes)
    reportAsFatalIfError(createResultCacheCorruptError(InputID));
  HashRef OutputHash = bufferAsRawHash(File->Data);
  return CASID(ResultCache
                   .insert(Lookup, ResultCacheType::HashedDataType(
                                       makeHash(InputID), makeHash(OutputHash)))
                   .Data);
}

Error BuiltinCAS::putCachedResult(CASID InputID, CASID OutputID) {
  auto Lookup = ResultCache.lookup(InputID.getHash());

  auto checkOutput = [&](HashRef ExistingOutput) -> Error {
    if (ExistingOutput == OutputID.getHash())
      return Error::success();
    // FIXME: probably...
    reportAsFatalIfError(createResultCachePoisonedError(InputID, OutputID,
                                                        CASID(ExistingOutput)));
    return Error::success();
  };
  if (Lookup)
    return checkOutput(Lookup->Data);

  if (!isInMemoryOnly()) {
    // Store on-disk and check any existing data matches.
    HashRef OutputHash = OutputID.getHash();
    MappedContentReference File = OnDiskResults->insert(
        InputID.getHash(), "results", rawHashAsBuffer(OutputHash));
    if (Error E = checkOutput(bufferAsRawHash(File.Data)))
      return E;
  }

  // Store in-memory.
  return checkOutput(
      ResultCache
          .insert(Lookup, ResultCacheType::HashedDataType(makeHash(InputID),
                                                          makeHash(OutputID)))
          .Data);
}

Optional<BuiltinCAS::MappedContentReference>
BuiltinCAS::openOnDisk(OnDiskHashMappedTrie &Trie, HashRef Hash) {
  if (auto Lookup = Trie.lookup(Hash))
    return std::move(Lookup.get());
  return None;
}

Expected<std::unique_ptr<MemoryBuffer>> BuiltinCAS::openFile(StringRef Path) {
  assert(!isInMemoryOnly());
  return errorOrToExpected(llvm::MemoryBuffer::getFile(Path));
}

Expected<std::unique_ptr<MemoryBuffer>>
BuiltinCAS::openFileWithID(StringRef BaseDir, CASID ID) {
  assert(!isInMemoryOnly());
  SmallString<256> Path = StringRef(*RootPath);
  sys::path::append(Path, BaseDir);

  SmallString<64> PrintableHash;
  extractPrintableHash(ID, PrintableHash);
  sys::path::append(Path, PrintableHash);

  Expected<std::unique_ptr<MemoryBuffer>> Buffer = openFile(Path);
  if (Buffer)
    return Buffer;

  // FIXME: Should we sometimes return the error directly?
  consumeError(Buffer.takeError());
  return createStringError(std::make_error_code(std::errc::invalid_argument),
                           "unknown blob '" + PrintableHash + "'");
}

StringRef BuiltinCAS::getPathForID(StringRef BaseDir, CASID ID,
                                   SmallVectorImpl<char> &Storage) {
  Storage.assign(RootPath->begin(), RootPath->end());
  sys::path::append(Storage, BaseDir);

  SmallString<64> PrintableHash;
  extractPrintableHash(ID, PrintableHash);
  sys::path::append(Storage, PrintableHash);

  return StringRef(Storage.begin(), Storage.size());
}

Expected<std::unique_ptr<CASDB>> cas::createOnDiskCAS(const Twine &Path) {
  SmallString<256> AbsPath;
  Path.toVector(AbsPath);
  sys::fs::make_absolute(AbsPath);

  if (AbsPath == getDefaultOnDiskCASStableID())
    AbsPath = StringRef(getDefaultOnDiskCASPath());

  if (std::error_code EC = sys::fs::create_directories(AbsPath)) {
    return createFileError(AbsPath, EC);
  }

  std::shared_ptr<OnDiskHashMappedTrie> OnDiskObjects;
  std::shared_ptr<OnDiskHashMappedTrie> OnDiskResults;

  uint64_t GB = 1024ull * 1024ull * 1024ull;
  if (auto Expected = OnDiskHashMappedTrie::create(
          AbsPath.str() + "/objects", NumHashBytes * sizeof(uint8_t), 16 * GB))
    OnDiskObjects = std::move(*Expected);
  else
    return Expected.takeError();
  if (auto Expected = OnDiskHashMappedTrie::create(
          AbsPath.str() + "/results", NumHashBytes * sizeof(uint8_t), GB))
    OnDiskResults = std::move(*Expected);
  else
    return Expected.takeError();

  return std::make_unique<BuiltinCAS>(AbsPath, std::move(OnDiskObjects),
                                      std::move(OnDiskResults));
}

std::unique_ptr<CASDB> cas::createInMemoryCAS() {
  return std::make_unique<BuiltinCAS>(BuiltinCAS::InMemoryOnlyTag());
}

// FIXME: Proxy not portable. Maybe also error-prone?
constexpr StringLiteral DefaultDirProxy = "/^llvm::cas::builtin::default";
constexpr StringLiteral DefaultName = "llvm.cas.builtin.default";

void cas::getDefaultOnDiskCASPath(SmallVectorImpl<char> &Path) {
  if (!llvm::sys::path::cache_directory(Path))
    report_fatal_error("cannot get default cache directory");
  llvm::sys::path::append(Path, DefaultName);
}

std::string cas::getDefaultOnDiskCASPath() {
  SmallString<128> Path;
  getDefaultOnDiskCASPath(Path);
  return Path.str().str();
}

void cas::getDefaultOnDiskCASStableID(SmallVectorImpl<char> &Path) {
  Path.assign(DefaultDirProxy.begin(), DefaultDirProxy.end());
  llvm::sys::path::append(Path, DefaultName);
}

std::string cas::getDefaultOnDiskCASStableID() {
  SmallString<128> Path;
  getDefaultOnDiskCASStableID(Path);
  return Path.str().str();
}
