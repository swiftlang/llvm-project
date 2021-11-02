//===- llvm/CAS/CASDB.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASDB_H
#define LLVM_CAS_CASDB_H

#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/Namespace.h"
#include "llvm/CAS/TreeEntry.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h" // FIXME: Split out sys::fs::file_status.
#include "llvm/Support/MemoryBuffer.h"
#include <cstddef>

namespace llvm {
namespace cas {

class CASDB;

/// Kind of CAS object.
enum class ObjectKind {
  Blob, /// Data, with no references.
  Tree, /// Filesystem-style tree, with named references and entry types.
  Node, /// Abstract hierarchical node, with data and references.
};

/// Generic CAS object reference.
class ObjectRef {
public:
  CASID getID() const { return ID; }
  operator CASID() const { return ID; }

protected:
  explicit ObjectRef(CASID ID) : ID(ID) {}

private:
  CASID ID;
};

/// Reference to a blob in the CAS.
class BlobRef : public ObjectRef {
public:
  /// Get the content of the blob. Valid as long as the CAS is valid.
  StringRef getData() const { return Data; }
  ArrayRef<char> getDataArray() const {
    return makeArrayRef(Data.begin(), Data.size());
  }
  StringRef operator*() const { return Data; }
  const StringRef *operator->() const { return &Data; }

  BlobRef() = delete;

private:
  BlobRef(CASID ID, StringRef Data) : ObjectRef(ID), Data(Data) {
    assert(Data.end()[0] == 0 && "Blobs should guarantee null-termination");
  }

  friend class CASDB;
  StringRef Data;
};

/// Reference to a tree CAS object. Reference is passed by value and is
/// expected to be valid as long as the \a CASDB is.
///
/// TODO: Add an API to expose a range of NamedTreeEntry.
///
/// TODO: Consider deferring copying/destruction/etc. to TreeAPI to enable an
/// implementation of CASDB to use reference counting for tree objects. Not
/// sure the utility, though, and it would add cost -- seems easier/better to
/// just make objects valid "forever".
class TreeRef : public ObjectRef {
public:
  bool empty() const { return NumEntries == 0; }
  size_t size() const { return NumEntries; }

  inline Optional<NamedTreeEntry> lookup(StringRef Name) const;
  inline NamedTreeEntry get(size_t I) const;

  /// Visit each tree entry in order, returning an error from \p Callback to
  /// stop early.
  inline Error
  forEachEntry(function_ref<Error(const NamedTreeEntry &)> Callback) const;

  TreeRef() = delete;

private:
  TreeRef(CASID ID, CASDB &CAS, const void *Tree, size_t NumEntries)
      : ObjectRef(ID), CAS(&CAS), Tree(Tree), NumEntries(NumEntries) {}

  friend class CASDB;
  CASDB *CAS;
  const void *Tree;
  size_t NumEntries;
};

/// Reference to an abstract hierarchical node, with data and references.
/// Reference is passed by value and is expected to be valid as long as the \a
/// CASDB is.
class NodeRef : public ObjectRef {
public:
  CASDB &getCAS() const { return *CAS; }

  size_t getNumReferences() const { return NumReferences; }
  inline CASID getReference(size_t I) const;

  /// Visit each reference in order, returning an error from \p Callback to
  /// stop early.
  inline Error forEachReference(function_ref<Error(CASID)> Callback) const;

  /// Get the content of the node. Valid as long as the CAS is valid.
  StringRef getData() const { return Data; }

  NodeRef() = delete;

private:
  NodeRef(CASID ID, CASDB &CAS, const void *Object, size_t NumReferences,
          StringRef Data)
      : ObjectRef(ID), CAS(&CAS), Object(Object), NumReferences(NumReferences),
        Data(Data) {}

  friend class CASDB;
  CASDB *CAS;
  const void *Object;
  size_t NumReferences;
  StringRef Data;
};

/// CAS Database. Hashes objects, stores them, provides in-memory lifetime, and
/// maintains an action cache.
///
/// TODO: Split this up into four different APIs. Remaining description is for
/// those APIs.
///
/// - ObjectHasher. Knows how to hash objects (blobs, trees, and nodes) to
///   produce a UniqueID.
///     - identify{Blob,Node,Tree}   => UniqueID
///     - identify{Blob,Node,Tree}   => UniqueIDRef + out-param: SmallVectorImpl<uint8_t>&
///     - Eventually: overloads for identify() with continuation out-param
///     - Eventually: overloads for identify() with std::future out-param
///
/// - ObjectStore : ObjectHasher. Storage for objects. Has low-level "put" and
///   "get" APIs suitable for receiving memory regions from another process,
///   giving both clients and implementations leeway to provide raw memory
///   regions or copy data out. APIs take a "Capture" out-parameter (see (far)
///   below) that configures the capabilities/preferences of the client.
///     - get{Blob,Node,Tree}        => Out-param: Capture
///     - create{Blob,Node,Tree}     => Out-param: UniqueID
///     - create{Blob,Node,Tree}     => Out-param: UniqueID + Capture
///     - createBlobFromOnDiskPath   => Out-param: UniqueID + Capture
///     - createBlobFromOpenFile     => Out-param: UniqueID + Capture
///     - Eventually: overloads with continuation out-param
///     - Eventually: overloads with std::future out-param
///     - A given ObjectHasher implementation can have various ObjectStore
///       implementations, depending on configuration / use case. E.g., for the
///       builtin CAS:
///         - BuiltinSingleFileCAS: single file contains all CAS objects, suitable
///           for an "opaque" artifact.
///         - BuiltinFilePerObjectCAS: every CAS object in a separate file. Great for
///           debugging the implementation.
///         - BuiltinDatabaseCAS: Like the current \a BuiltinCAS.
///         - BuiltinNullCAS: No storage. `get*()` APIs always fail, and
///           `create*()` APIs don't do anything but produce a UniqueID. Useful
///           for InMemoryCAS below.
///
/// - InMemoryView : ObjectStore. Concrete class (not abstract) that adapts another
///   ObjectStore and provides an in-memory view of it. Guarantees in-memory lifetime
///   for objects (via classes \c InMemoryBlob, \c InMemoryTree, and \c
///   InMemoryNode). Re-exports the low-level ObjectStore APIs and adds new high-level
///   APIs; uses the "Capture" out-parameters to get memory mapped regions from the
///   adapted ObjectStore. Takes on the \a Namespace of the adapted ObjectStore.
///     - \c InMemoryObject. Base class for objects. Has UniqueID (Namespace / hash size
///       / hash). Probably change \a ObjectRef to wrap a non-null \c InMemoryObject*.
///     - \c InMemoryBlob : InMemoryObject. Adds a StringRef for Data.
///     - \c InMemoryNode : InMemoryObject. Adds a StringRef for Data. Adds
///       ArrayRef containing a flat array of hashes (which combines with
///       Namespace to make a FlatUniqueIDArrayRef).
///     - \c InMemoryTree : InMemoryObject. Simplest to use a specific format and
///       translate from underlying CAS object to make the in-memory view.
///     - get{Blob,Node,Tree}        => InMemoryBlob*,InMemoryNode*,InMemoryTree*
///     - create{Blob,Node,Tree}     => InMemoryBlob*,InMemoryNode*,InMemoryTree*
///     - Eventually: overloads with continuation out-param
///     - Eventually: overloads with std::future out-param
///     - Examples:
///          - If an InMemoryView adapts a BuiltinDatabaseCAS, it should be
///            equivalent to the current return of \a createOnDiskCAS().
///          - If an InMemoryView adapts a BuiltinNullCAS, it should be
///            equivalent to the current return of \a createInMemoryCAS().
///
/// - ActionCache. Assigned to a specific Namespace, is a cache/map from an
///   arbitrary StringRef to a UniqueID. Must be in the same namespace as the
///   IDs it stores.
///     - put: (StringRef, UniqueIDRef)           => Error
///     - get: (StringRef, UniqueID&)             => Error
///     - get: (StringRef, Optional<UniqueIDRef>) => Error // provides lifetime
///     - Concrete subclasses:
///         - InMemory -- can use ThreadSafeHashMappedTrie
///         - OnDisk -- can use OnDiskHashMappedTrie, but lots of options; needs
///           configuration
///         - Plugin -- simple plugin API for the cache itself
///
/// Plugins:
/// ========
///
/// - InMemoryView does not have plugins
///     - Adapts other (concrete) ObjectStores
///     - Stores mapped memory regions, as necessary
///     - Provides views of trees/blobs/etc.
///
/// - ObjectStorePlugin : ObjectStore
///     - Move BuiltinCAS and subclasses into a plugin
///     - Plugins should be out-of-process (in a daemon)
///     - Use memory mapped IPC to avoid overhead / keep BuiltinCAS fast
///     - Maybe: add an "in-memory store" plugin that's ephemeral, lasting just
///       until the daemon shuts down but sharing work as long as the daemon
///       lives
///
/// - ActionCachePlugin : ActionCache
///     - Move OnDiskActionCache into a plugin
///     - Plugins should be out-of-process (in a daemon)
///     - Maybe: add an "in-memory cache" plugin that's ephemeral, lasting just
///       until the daemon shuts down but sharing work as long as the daemon
///       lives
///
/// "Capture" out-parameters:
/// =========================
///
/// For ObjectStore APIs, there are three axes:
/// - How is the data stored? Is the ObjectStore able to provide direct access
///   to it somehow? With what lifetime?
/// - How sophisticated is the client? Given how the object is stored, is it
///   able to use it directly and/or manage allocations?
/// - How big is the object? What are the overhead tradeoffs for copying,
///   managing allocations, and/or splitting up virtual memory?
///   it directly? In managing extra allocations?
///
/// The proposed design is for each API to have a single out-parameter that the
/// client uses to configure its capabilities, and the ObjectStore picks from
/// the ones available to send data over. There should be a "default" transfer
/// schema that all clients and all implementations/plugins support.
///
/// - BlobCapture
///     - Stream: raw_ostream&. Default (client/store must both support).
///       Client expected to configure with \a raw_svector_stream or similar.
///     - Persistent: StringRef. Store guarantees lifetime as long as the CAS
///       is alive. Client can set 'RequireNullTerminated', in which case store
///       should not use unless it's able to guarantee a null-terminated view.
///     - Managed: (StringRef,Destructor). Lifetime managed by client. A \c
///       std::unique-ptr<MemoryBuffer> or equivalent. 'RequiresMinSize' can
///       be set to disallow use when blobs are too small (e.g., the client
///       may want to copy to bump-ptr-allocated memory and skip the allocation
///       traffic past a given threshold).
/// - NodeCapture
///     - Stream: (raw_ostream&,SmallVectorImpl<UniqueID>&). Default. Data is
///       streamed; refs are appended.
///     - Persistent: (StringRef,FlatUniqueIDArrayRef). Guaranteed lifetime.
///       Client can set 'RequireNullTerminated' for the data.
///     - Managed: (StringRef,FlatUniqueIDArrayRef,Destructor). Lifetime
///       managed by client. 'RequiresMinSize' (sum of storage) can be set to
///       disallow use when nodes are too small (e.g., the client may want to
///       copy to bump-ptr-allocated memory and skip the allocation traffic
///       past a given threshold).
/// - TreeCapture
///     - Visitor: (function_ref<Error(const NamedTreeEntry&)>). Default.
///       Entries visited one at a time.
///     - Vector: (SmallVectorImpl<NamedTreeEntry>&,StringSaver*=nulltpr,
///                UniqueIDSaver*=nullptr).
///       Entries pushed back. Each NamedTreeEntry::getName() and \a
///       TreeEntry::getID() must have guaranteed lifetime, unless optional
///       StringSaver and UniqueIDSaver provided. Names required to be
///       null-terminated.
///     - Persistent: (FlatUniqueIDArrayRef,StringRef,ArrayRef<Encoding>).
///       Return flat array of entry IDs, name block, and array of some
///       encoded data for EntryKind and NameOffset. There could be many
///       encodings; simplest way to start is to require a specific encoding
///       that is convenient for InMemoryTree to wrap BuiltinCAS, and if a
///       specific ObjectStore cannot provide it, fall back to Vector or
///       Visitor. Other plugins would likely fall back to Visitor essentially
///       all the time. If this leaves perf on the floor in practice, we can
///       design something more flexible that solves the problem better (e.g.,
///       support a fixed set of schemas/encodings, or virtualize decoding
///       function, or ...).
///     - Managed: (FlatUniqueIDArrayRef,StringRef,ArrayRef<Encoding>,
///                 Destructor).
///       As with Persistent, but with a managed lifetime. 'RequiresMinSize'
///       (sum of storage) can be set to disallow use when nodes are too small
///       e.g., the client may want to copy to bump-ptr-allocated memory and
///       skip the allocation traffic past a given threshold).
class CASDB {
public:
  const Namespace &getNamespace() const { return NS; }

  /// Convenience wrapper around \a Namespace::printID().
  void printID(raw_ostream &OS, UniqueIDRef ID) const {
    getNamespace().printID(OS, ID);
  }

  Error parseID(raw_ostream &OS, UniqueID &ID) const {
    return getNamespace().parseID(OS, ID);
  }

  /// Get a \p CASID from a \p Reference, which should have been generated by
  /// \a printCASID(). This succeeds as long as \p Reference is valid
  /// (correctly formatted); it does not refer to an object that exists, just
  /// be a reference that has been constructed correctly.
  ///
  /// TODO: Remove once callers use \a parseID().
  Expected<CASID> parseCASID(StringRef Reference);

  /// Print \p ID to \p OS, returning an error if \p ID is not a valid \p CASID
  /// for this CAS. If \p ID is valid for the CAS schema but unknown to this
  /// instance (say, because it was generated by another instance), this should
  /// not return an error.
  ///
  /// TODO: Remove once callers use \a printID().
  Error printCASID(raw_ostream &OS, CASID ID);

  Expected<std::string> convertCASIDToString(CASID ID);

  Error getPrintedCASID(CASID ID, SmallVectorImpl<char> &Reference);

  virtual Expected<BlobRef> createBlob(StringRef Data) = 0;

  virtual Expected<TreeRef>
  createTree(ArrayRef<NamedTreeEntry> Entries = None) = 0;

  virtual Expected<NodeRef> createNode(ArrayRef<CASID> References,
                                       StringRef Data) = 0;

  /// Default implementation reads \p FD and calls \a createBlob(). Does not
  /// take ownership of \p FD; the caller is responsible for closing it.
  ///
  /// If \p Status is sent in it is to be treated as a hint. Implementations
  /// must protect against the file size potentially growing after the status
  /// was taken (i.e., they cannot assume that an mmap will be null-terminated
  /// where \p Status implies).
  ///
  /// Returns the \a CASID and the size of the file.
  Expected<BlobRef>
  createBlobFromOpenFile(sys::fs::file_t FD,
                         Optional<sys::fs::file_status> Status = None) {
    return createBlobFromOpenFileImpl(FD, Status);
  }

protected:
  virtual Expected<BlobRef>
  createBlobFromOpenFileImpl(sys::fs::file_t FD,
                             Optional<sys::fs::file_status> Status);

public:
  virtual Expected<BlobRef> getBlob(CASID ID) = 0;
  virtual Expected<TreeRef> getTree(CASID ID) = 0;
  virtual Expected<NodeRef> getNode(CASID ID) = 0;

  virtual Optional<ObjectKind> getObjectKind(CASID ID) = 0;
  virtual bool isKnownObject(CASID ID) { return bool(getObjectKind(ID)); }

  virtual Expected<CASID> getCachedResult(CASID InputID) = 0;
  virtual Error putCachedResult(CASID InputID, CASID OutputID) = 0;

  virtual void print(raw_ostream &) const {}
  void dump() const;

  virtual ~CASDB() = default;

protected:
  // Support for TreeRef.
  friend class TreeRef;
  virtual Optional<NamedTreeEntry> lookupInTree(const TreeRef &Tree,
                                                StringRef Name) const = 0;
  virtual NamedTreeEntry getInTree(const TreeRef &Tree, size_t I) const = 0;
  virtual Error forEachEntryInTree(
      const TreeRef &Tree,
      function_ref<Error(const NamedTreeEntry &)> Callback) const = 0;

  /// Build a \a BlobRef. For use by derived classes to access the private
  /// constructor of \a BlobRef. Templated as a hack to allow this to be
  /// declared before \a TreeRef.
  static BlobRef makeBlobRef(CASID ID, StringRef Data) {
    return BlobRef(ID, Data);
  }

  /// Extract the tree pointer from \p Ref. For use by derived classes to
  /// access the private pointer member. Ensures \p Ref comes from this
  /// instance.
  ///
  const void *getTreePtr(const TreeRef &Ref) const {
    assert(Ref.CAS == this);
    assert(Ref.Tree);
    return Ref.Tree;
  }

  /// Build a \a TreeRef from a pointer. For use by derived classes to access
  /// the private constructor of \a TreeRef.
  TreeRef makeTreeRef(CASID ID, const void *TreePtr, size_t NumEntries) {
    assert(TreePtr);
    return TreeRef(ID, *this, TreePtr, NumEntries);
  }

  // Support for NodeRef.
  friend class NodeRef;
  virtual CASID getReferenceInNode(const NodeRef &Ref, size_t I) const = 0;
  virtual Error
  forEachReferenceInNode(const NodeRef &Ref,
                         function_ref<Error(CASID)> Callback) const = 0;

  /// Extract the object pointer from \p Ref. For use by derived classes to
  /// access the private pointer member. Ensures \p Ref comes from this
  /// instance.
  ///
  const void *getNodePtr(const NodeRef &Ref) const {
    assert(Ref.CAS == this);
    assert(Ref.Object);
    return Ref.Object;
  }

  /// Build a \a NodeRef from a pointer. For use by derived classes to
  /// access the private constructor of \a NodeRef.
  NodeRef makeNodeRef(CASID ID, const void *ObjectPtr, size_t NumReferences,
                      StringRef Data) {
    assert(ObjectPtr);
    return NodeRef(ID, *this, ObjectPtr, NumReferences, Data);
  }

  CASDB(const Namespace &NS) : NS(NS) {}

private:
  const Namespace &NS;
};

Optional<NamedTreeEntry> TreeRef::lookup(StringRef Name) const {
  return CAS->lookupInTree(*this, Name);
}

NamedTreeEntry TreeRef::get(size_t I) const { return CAS->getInTree(*this, I); }

Error TreeRef::forEachEntry(
    function_ref<Error(const NamedTreeEntry &)> Callback) const {
  return CAS->forEachEntryInTree(*this, Callback);
}

CASID NodeRef::getReference(size_t I) const {
  return CAS->getReferenceInNode(*this, I);
}

Error NodeRef::forEachReference(function_ref<Error(CASID)> Callback) const {
  return CAS->forEachReferenceInNode(*this, Callback);
}

Expected<std::unique_ptr<CASDB>>
createPluginCAS(StringRef PluginPath, ArrayRef<std::string> PluginArgs = None);
std::unique_ptr<CASDB> createInMemoryCAS();
Expected<std::unique_ptr<CASDB>> createOnDiskCAS(const Twine &Path);

void getDefaultOnDiskCASPath(SmallVectorImpl<char> &Path);
void getDefaultOnDiskCASStableID(SmallVectorImpl<char> &Path);

std::string getDefaultOnDiskCASPath();
std::string getDefaultOnDiskCASStableID();

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_CASDB_H
