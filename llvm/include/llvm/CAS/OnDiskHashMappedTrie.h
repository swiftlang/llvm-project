//===- OnDiskHashMappedTrie.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_ONDISKHASHMAPPEDTRIE_H
#define LLVM_CAS_ONDISKHASHMAPPEDTRIE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SHA1.h"
#include <atomic>
#include <mutex>

namespace llvm {

class MemoryBuffer;

namespace cas {

/// On-disk hash-mapped trie. Thread-safe / lock-free.
///
/// This is an on-disk, (mostly) thread-safe key-value store that is (mostly)
/// lock-free. The keys are fixed length, and are expected to be binary hashes
/// with a normal distribution.
///
/// - Thread-safety is achieved through the use of atomics within a shared
///   memory mapping. Atomic access does not work on networked filesystems.
/// - Filesystem locks are used, but only sparingly:
///     - during initialization, for creating / opening an existing store;
///     - rarely on insertion, to resize the store (see \a
///       OnDiskHashMappedTrie::MappedFileInfo::requestFileSize()).
/// - Path is used as a directory:
///     - "index" stores the root trie and subtries.
///     - "data" stores (most of) the entries, like a bump-ptr-allocator.
///     - Large entries are stored externally in a file named by the key.
/// - Code is system-dependent (Windows not yet implemented), and binary format
///   itself is not portable. These are not artifacts that can/should be moved
///   between different systems; they are only appropriate for local storage.
///
/// FIXME: Add support for storing top-level metadata or identifiers that can
/// be created / read during initialization.
///
/// FIXME: Implement for Windows. See comment next to implementation of \a
/// OnDiskHashMappedTrie::MappedFileInfo::open().
class OnDiskHashMappedTrie {
public:
  void operator delete(void *Ptr) { ::free(Ptr); }

  LLVM_DUMP_METHOD void dump() const;
  void print(raw_ostream &OS) const;

public:
  struct MappedContentReference {
    StringRef Metadata;              // Not null-terminated.
    StringRef Data;                  // Always used, always null-terminated.
    sys::fs::mapped_file_region Map; // Needed for large blobs.

    MappedContentReference() = delete;

    explicit MappedContentReference(
        StringRef Metadata, StringRef Data,
        sys::fs::mapped_file_region Map = sys::fs::mapped_file_region())
        : Metadata(Metadata), Data(Data), Map(std::move(Map)) {}
    MappedContentReference(MappedContentReference &&) = default;
  };

  /// Result of a lookup. Suitable for an insertion hint. Maybe could be
  /// expanded into an iterator of sorts, but likely not useful (visiting
  /// everything in the trie should probably be done some way other than
  /// through an iterator pattern).
  class LookupResult {
  public:
    Optional<MappedContentReference> &get() { return Content; }

    explicit operator bool() const { return I == -2u; }

    LookupResult() noexcept = default;
    LookupResult(LookupResult &&) = default;
    LookupResult &operator=(LookupResult &&) = default;

    LookupResult(const LookupResult &) = delete;
    LookupResult &operator=(const LookupResult &) = delete;

  private:
    friend class OnDiskHashMappedTrie;
    LookupResult(MappedContentReference Content)
        : Content(std::move(Content)), I(-2u) {}
    LookupResult(void *S, unsigned I, unsigned B) : S(S), I(I), B(B) {}

    bool isHint() const { return I != -1u && I != -2u; }

    Optional<MappedContentReference> Content;
    const void *S = nullptr;
    unsigned I = -1u;
    unsigned B = 0;
  };

  LookupResult lookup(ArrayRef<uint8_t> Hash) const;

  /// Returns the content in the map.
  MappedContentReference insert(LookupResult Hint, ArrayRef<uint8_t> Hash,
                                StringRef Metadata, StringRef Data);

  MappedContentReference insert(ArrayRef<uint8_t> Hash, StringRef Metadata,
                                StringRef Data) {
    return insert(LookupResult(), Hash, Metadata, Data);
  }

  static Expected<std::shared_ptr<OnDiskHashMappedTrie>>
  create(const Twine &Path, size_t NumHashBits, uint64_t MaxMapSize,
         Optional<size_t> InitialNumRootBits = None,
         Optional<size_t> InitialNumSubtrieBits = None);

  // Move can be implemented if we add support to mapped_file_region. No need
  // to move the mutexes.
  OnDiskHashMappedTrie(OnDiskHashMappedTrie &&RHS) = delete;
  OnDiskHashMappedTrie &operator=(OnDiskHashMappedTrie &&RHS) = delete;

  // No copy. Just call \a create() again.
  OnDiskHashMappedTrie(const OnDiskHashMappedTrie &) = delete;
  OnDiskHashMappedTrie &operator=(const OnDiskHashMappedTrie &) = delete;

  /// Should be private, but std::make_shared needs access.
  OnDiskHashMappedTrie() = default;

private:
  ~OnDiskHashMappedTrie();
  struct MappedFileInfo {
    std::string Path;
    sys::fs::file_t FD;
    sys::fs::mapped_file_region Map;
    std::atomic<uint64_t> OnDiskSize;
    std::mutex Mutex;

    static Error open(Optional<MappedFileInfo> &MFI, StringRef Path,
                      size_t InitialSize, size_t MaxSize,
                      function_ref<void(char *)> NewFileConstructor);

    MappedFileInfo(MappedFileInfo &&) = delete;
    MappedFileInfo(const MappedFileInfo &) = delete;
    MappedFileInfo(StringRef Path, sys::fs::file_t FD, size_t MapSize,
                   std::error_code &EC);
    ~MappedFileInfo();
    Error requestFileSize(uint64_t Size);
  };
  Optional<MappedFileInfo> Index;
  Optional<MappedFileInfo> Data;
};

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_ONDISKHASHMAPPEDTRIE_H
