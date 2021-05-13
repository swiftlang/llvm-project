//===- HashMappedTrie.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_HASHMAPPEDTRIE_H
#define LLVM_CAS_HASHMAPPEDTRIE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/SHA1.h"
#include <atomic>

namespace llvm {

class MemoryBuffer;

namespace cas {

struct alignas(2) TrieNode {
  const bool IsSubtrie = false;

  TrieNode(bool IsSubtrie) : IsSubtrie(IsSubtrie) {}

  static void *operator new(size_t Size) { return ::malloc(Size); }
  void operator delete(void *Ptr) { ::free(Ptr); }
};

struct TrieContentBase : public TrieNode {
  ArrayRef<uint8_t> Bytes;

protected:
  TrieContentBase() : TrieNode(false) {}
  void initialize(ArrayRef<uint8_t> Bytes) { this->Bytes = Bytes; }
};

} // namespace cas

template <> struct isa_impl<cas::TrieContentBase, cas::TrieNode> {
  static inline bool doit(const cas::TrieNode &TN) { return !TN.IsSubtrie; }
};

namespace cas {

template <class DataT, class HashT>
struct TrieContent : public TrieContentBase {
  struct HashedDataType {
    HashT Hash;
    DataT Data;

    struct EmplaceDataTag {};
    HashedDataType(HashedDataType &&) = default;
    HashedDataType(const HashedDataType &) = default;
    HashedDataType(const HashT &Hash, const DataT &Data)
        : Hash(Hash), Data(Data) {}
    HashedDataType(const HashT &Hash, DataT &&Data)
        : Hash(Hash), Data(std::move(Data)) {}
    template <class... ArgsT>
    HashedDataType(EmplaceDataTag, ArgsT &&... Args)
        : Data(std::forward<ArgsT>(Args)...) {}
    template <class... ArgsT>
    HashedDataType(const HashT &Hash, EmplaceDataTag, ArgsT &&... Args)
        : Hash(Hash), Data(std::forward<ArgsT>(Args)...) {}
  };

  HashedDataType HashedData;

  TrieContent() = delete;

  TrieContent(HashedDataType &&HashedData) : HashedData(std::move(HashedData)) {
    initialize();
  }

  TrieContent(const HashedDataType &HashedData) : HashedData(HashedData) {
    initialize();
  }

  template <class... ArgsT>
  TrieContent(const HashT &Hash, ArgsT &&... Args)
      : HashedData(Hash, typename HashedDataType::EmplaceDataTag(),
                   std::forward<ArgsT>(Args)...) {
    initialize();
  }

protected:
  void initialize() { TrieContentBase::initialize(HashedData.Hash); }
}; // namespace cas

/// Base class for a lock-free thread-safe hash-mapped trie.
class ThreadSafeHashMappedTrieBase {
public:
  void operator delete(void *Ptr) { ::free(Ptr); }

  static constexpr size_t DefaultNumRootBits = 6;
  static constexpr size_t DefaultNumSubtrieBits = 4;

  LLVM_DUMP_METHOD void dump() const;
  void print(raw_ostream &OS) const;

protected:
  /// Result of a lookup. Suitable for an insertion hint. Maybe could be
  /// expanded into an iterator of sorts, but likely not useful (visiting
  /// everything in the trie should probably be done some way other than
  /// through an iterator pattern).
  class LookupResultBase {
  protected:
    const TrieContentBase *get() const {
      return I == -2u ? static_cast<TrieContentBase *>(P) : nullptr;
    }

  public:
    LookupResultBase() noexcept = default;
    LookupResultBase(LookupResultBase &&) = default;
    LookupResultBase(const LookupResultBase &) = default;
    LookupResultBase &operator=(LookupResultBase &&) = default;
    LookupResultBase &operator=(const LookupResultBase &) = default;

  private:
    friend class ThreadSafeHashMappedTrieBase;
    LookupResultBase(TrieContentBase &Content) : P(&Content), I(-2u) {}
    LookupResultBase(void *P, unsigned I, unsigned B) : P(P), I(I), B(B) {}

    bool isHint() const { return I != -1u && I != -2u; }

    void *P = nullptr;
    unsigned I = -1u;
    unsigned B = 0;
  };

  LookupResultBase lookup(ArrayRef<uint8_t> Hash) const;

  /// Returns the actual content in the map, potentially a different node.
  TrieContentBase &insert(LookupResultBase Hint,
                          std::unique_ptr<TrieContentBase> Content);

  ThreadSafeHashMappedTrieBase() = delete;

  ThreadSafeHashMappedTrieBase(Optional<size_t> NumRootBits = None,
                               Optional<size_t> NumSubtrieBits = None)
      : NumRootBits(NumRootBits ? *NumRootBits : DefaultNumRootBits),
        NumSubtrieBits(NumSubtrieBits ? *NumSubtrieBits
                                      : DefaultNumSubtrieBits),
        Root(nullptr) {
    assert((!NumRootBits || *NumRootBits < 20) &&
           "Root should have fewer than ~1M slots");
    assert((!NumSubtrieBits || *NumSubtrieBits < 10) &&
           "Subtries should have fewer than ~1K slots");
  }

  ~ThreadSafeHashMappedTrieBase();

  ThreadSafeHashMappedTrieBase(ThreadSafeHashMappedTrieBase &&RHS);

  // Move assignment can be implemented in a thread-safe way if NumRootBits and
  // NumSubtrieBits are stored inside the Root.
  ThreadSafeHashMappedTrieBase &
  operator=(ThreadSafeHashMappedTrieBase &&RHS) = delete;

  // No copy.
  ThreadSafeHashMappedTrieBase(const ThreadSafeHashMappedTrieBase &) = delete;
  ThreadSafeHashMappedTrieBase &
  operator=(const ThreadSafeHashMappedTrieBase &) = delete;

private:
  unsigned NumRootBits;
  unsigned NumSubtrieBits;
  std::atomic<TrieNode *> Root;
};

/// Lock-free thread-safe hash-mapped trie.
template <class T, class HashT>
class ThreadSafeHashMappedTrie : ThreadSafeHashMappedTrieBase {
  using TrieContentType = TrieContent<T, HashT>;

public:
  using ThreadSafeHashMappedTrieBase::operator delete;
  using HashType = HashT;
  using HashedDataType = typename TrieContentType::HashedDataType;

  using ThreadSafeHashMappedTrieBase::dump;
  using ThreadSafeHashMappedTrieBase::print;

  class LookupResult : LookupResultBase {
  public:
    const HashedDataType *get() const {
      if (const TrieContentBase *B = LookupResultBase::get())
        return &static_cast<const TrieContentType *>(B)->HashedData;
      return nullptr;
    }
    const HashedDataType &operator*() const {
      assert(get());
      return *get();
    }
    const HashedDataType *operator->() const {
      assert(get());
      return get();
    }
    explicit operator bool() const { return get(); }

    LookupResult() = default;
    LookupResult(LookupResult &&) = default;
    LookupResult(const LookupResult &) = default;
    LookupResult &operator=(LookupResult &&) = default;
    LookupResult &operator=(const LookupResult &) = default;

  private:
    LookupResult(LookupResultBase Result) : LookupResultBase(Result) {}
    friend class ThreadSafeHashMappedTrie;
  };

  /// Insert with a hint. Default-constructed hint will work, but it's
  /// recommended to start with a lookup to avoid overhead in object creation
  /// if it already exists.
  const HashedDataType &insert(LookupResult Hint, HashedDataType &&HashedData) {
    return static_cast<TrieContentType &>(
               ThreadSafeHashMappedTrieBase::insert(
                   Hint,
                   std::make_unique<TrieContentType>(std::move(HashedData))))
        .HashedData;
  }

  const HashedDataType &insert(LookupResult Hint,
                               const HashedDataType &HashedData) {
    return static_cast<TrieContentType &>(
               ThreadSafeHashMappedTrieBase::insert(
                   Hint, std::make_unique<TrieContentType>(HashedData)))
        .HashedData;
  }

  LookupResult lookup(ArrayRef<uint8_t> Hash) const {
    assert(Hash.size() == std::tuple_size<HashT>::value);
    return ThreadSafeHashMappedTrieBase::lookup(Hash);
  }

  ThreadSafeHashMappedTrie(Optional<size_t> NumRootBits = None,
                           Optional<size_t> NumSubtrieBits = None)
      : ThreadSafeHashMappedTrieBase(NumRootBits, NumSubtrieBits) {}

  // Move constructor okay.
  ThreadSafeHashMappedTrie(ThreadSafeHashMappedTrie &&) = default;

  // No move assignment or any copy.
  ThreadSafeHashMappedTrie &operator=(ThreadSafeHashMappedTrie &&) = delete;
  ThreadSafeHashMappedTrie(const ThreadSafeHashMappedTrie &) = delete;
  ThreadSafeHashMappedTrie &
  operator=(const ThreadSafeHashMappedTrie &) = delete;
};

template <class T> struct HashMappedTrieInfo;

template <> struct HashMappedTrieInfo<ArrayRef<uint8_t>> {
  template <class HasherT> static auto hash(ArrayRef<uint8_t> Data) {
    return HasherT::template hash(Data);
  }
};

template <> struct HashMappedTrieInfo<StringRef> {
  template <class HasherT> static auto hash(StringRef Data) {
    return HasherT::hash(
        makeArrayRef(reinterpret_cast<const uint8_t *>(Data.begin()),
                     reinterpret_cast<const uint8_t *>(Data.end())));
  }
};

template <class HasherT>
using HashTypeForHasher =
    decltype(HasherT::hash(std::declval<ArrayRef<uint8_t>>()));

template <>
struct HashMappedTrieInfo<std::string> : HashMappedTrieInfo<StringRef> {};

template <class T, class HasherT = SHA1,
          class HashInfoT = HashMappedTrieInfo<T>>
class ThreadSafeHashMappedTrieSet
    : ThreadSafeHashMappedTrie<T, HashTypeForHasher<HasherT>> {
  using HashType = HashTypeForHasher<HasherT>;
  using TrieType = ThreadSafeHashMappedTrie<T, HashType>;
  using HashedDataType = typename TrieType::HashedDataType;

public:
  using TrieType::dump;
  using TrieType::print;

  class LookupResult : TrieType::LookupResult {
    using BaseType = typename TrieType::LookupResult;

  public:
    const T *get() const {
      if (const auto *V = BaseType::get())
        return &static_cast<const HashedDataType *>(V)->Data;
      return nullptr;
    }
    const T &operator*() const {
      assert(get());
      return *get();
    }
    const T *operator->() const {
      assert(get());
      return get();
    }
    explicit operator bool() const { return get(); }

    LookupResult() = default;
    LookupResult(LookupResult &&) = default;
    LookupResult(const LookupResult &) = default;
    LookupResult &operator=(LookupResult &&) = default;
    LookupResult &operator=(const LookupResult &) = default;

  private:
    LookupResult(BaseType Result) : BaseType(Result) {}
    friend class ThreadSafeHashMappedTrieSet;
  };

  ThreadSafeHashMappedTrieSet(Optional<size_t> NumRootBits = None,
                              Optional<size_t> NumSubtrieBits = None)
      : TrieType(NumRootBits, NumSubtrieBits) {}
  LookupResult lookup(const T &Value) const {
    return LookupResult(
        TrieType::lookup(HashInfoT::template hash<HasherT>(Value)));
  }
  template <class OtherT> LookupResult lookupAs(const OtherT &Value) const {
    return LookupResult(
        TrieType::lookup(HashInfoT::template hash<HasherT>(Value)));
  }
  const T &insert(LookupResult Hint, T &&Value) {
    HashType Hash = HashInfoT::template hash<HasherT>(Value);
    return TrieType::insert(typename LookupResult::BaseType(Hint),
                            HashedDataType(Hash, std::move(Value)))
        .Data;
  }
  const T &insert(LookupResult Hint, const T &Value) {
    HashType Hash = HashInfoT::template hash<HasherT>(Value);
    return TrieType::insert(typename LookupResult::BaseType(Hint),
                            HashedDataType(Hash, Value))
        .Data;
  }
  const T &insert(T &&Value) { return insert(LookupResult(), Value); }
  const T &insert(const T &Value) { return insert(LookupResult(), Value); }
};

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_HASHMAPPEDTRIE_H
