//===- HashMappedTrie.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/HashMappedTrie.h"
#include "HashMappedTrieIndexGenerator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::cas;

namespace {
class ThreadSafeSubtrie final : public TrieNode {
public:
  TrieNode *get(size_t I) const { return Slots[I].load(); }

  TrieNode *try_set(size_t I, std::unique_ptr<TrieContentBase> &New);

  ThreadSafeSubtrie *sink(size_t I, TrieContentBase *Content,
                          size_t NumSubtrieBits, size_t NewI);

  void printHash(raw_ostream &OS, ArrayRef<uint8_t> Bytes) const;
  void print(raw_ostream &OS) const { print(OS, None); }
  void print(raw_ostream &OS, Optional<std::string> Prefix) const;
  void dump() const { print(dbgs()); }

  static std::unique_ptr<ThreadSafeSubtrie> create(size_t StartBit,
                                                   size_t NumBits);

public:
  ~ThreadSafeSubtrie();
  explicit ThreadSafeSubtrie(size_t StartBit, size_t NumBits);

private:
  // FIXME: Consider adding the following:
  //
  // std::atomic<std::bitset<20>> IsSet;
  //
  // Which could be used to micro-optimize get() to return nullptr without
  // reading from Slots. This would be the algorithm for updating IsSet (after
  // updating Slots):
  //
  //     std::bitset<20> Old = ...;
  //     std::bitset<20> New = Old;
  //     New.set(I);
  //     while (!IsSet.compare_exchange_weak(Old, New, ...))
  //       New |= Old;
  //
  // However, if we expect most accesses to be successful (which we probably
  // do if reading more than writing) than this may not be profitable.

  // For debugging.
  unsigned StartBit = 0;
  unsigned NumBits = 0;

  /// The (co-allocated) slots of the subtrie.
  MutableArrayRef<std::atomic<TrieNode *>> Slots;
};
} // namespace

namespace llvm {
template <> struct isa_impl<ThreadSafeSubtrie, TrieNode> {
  static inline bool doit(const TrieNode &TN) { return TN.IsSubtrie; }
};
} // namespace llvm

std::unique_ptr<ThreadSafeSubtrie> ThreadSafeSubtrie::create(size_t StartBit,
                                                             size_t NumBits) {
  assert(NumBits < 20 && "Tries should have fewer than ~1M slots");
  size_t Size =
      sizeof(ThreadSafeSubtrie) + sizeof(TrieNode *) * (1u << NumBits);
  void *Memory = ::malloc(Size);
  ThreadSafeSubtrie *S = ::new (Memory) ThreadSafeSubtrie(StartBit, NumBits);
  return std::unique_ptr<ThreadSafeSubtrie>(S);
}

ThreadSafeSubtrie::ThreadSafeSubtrie(size_t StartBit, size_t NumBits)
    : TrieNode(true), StartBit(StartBit), NumBits(NumBits),
      Slots(reinterpret_cast<std::atomic<TrieNode *> *>(
                reinterpret_cast<char *>(this) + sizeof(ThreadSafeSubtrie)),
            (1u << NumBits)) {
  for (auto *I = Slots.begin(), *E = Slots.end(); I != E; ++I)
    new (I) std::atomic<TrieNode *>(nullptr);
}

ThreadSafeSubtrie::~ThreadSafeSubtrie() {
  for (auto *I = Slots.begin(), *E = Slots.end(); I != E; ++I) {
    // All the slots are owned.
    delete I->load(std::memory_order_relaxed);
    I->~atomic();
  }
}

TrieNode *ThreadSafeSubtrie::try_set(size_t I,
                                     std::unique_ptr<TrieContentBase> &New) {
  TrieNode *Old = nullptr;
  if (!Slots[I].compare_exchange_strong(Old, New.get()))
    return Old;
  // Success.
  New.release();
  return nullptr;
}

ThreadSafeSubtrie *ThreadSafeSubtrie::sink(size_t I, TrieContentBase *Content,
                                           size_t NumSubtrieBits, size_t NewI) {
  assert(NumSubtrieBits > 0);
  std::unique_ptr<ThreadSafeSubtrie> S =
      create(StartBit + NumBits, NumSubtrieBits);

  assert(NewI < S->Slots.size());
  S->Slots[NewI].store(Content);

  TrieNode *Existing = Content;
  assert(I < Slots.size());
  if (Slots[I].compare_exchange_strong(Existing, S.get()))
    return S.release();

  // Another thread created a subtrie already. Drop S's reference to Content,
  // allowing it to be destructed as it exits scope, and return the existing
  // subtrie.
  S->Slots[NewI].store(nullptr, std::memory_order_relaxed);
  return cast<ThreadSafeSubtrie>(Existing);
}

ThreadSafeHashMappedTrieBase::LookupResultBase
ThreadSafeHashMappedTrieBase::lookup(ArrayRef<uint8_t> Hash) const {
  assert(!Hash.empty() && "Uninitialized hash");

  ThreadSafeSubtrie *S = cast_or_null<ThreadSafeSubtrie>(Root.load());
  if (!S)
    return LookupResultBase();

  IndexGenerator IndexGen{NumRootBits, NumSubtrieBits, Hash};
  size_t Index = IndexGen.next();
  for (;;) {
    // Try to set the content.
    TrieNode *Existing = S->get(Index);
    if (!Existing)
      return LookupResultBase(S, Index, *IndexGen.StartBit);

    // Check for an exact match.
    if (auto *ExistingContent = dyn_cast<TrieContentBase>(Existing))
      return ExistingContent->Bytes == Hash
                 ? LookupResultBase(*ExistingContent)
                 : LookupResultBase(S, Index, *IndexGen.StartBit);

    Index = IndexGen.next();
    S = cast<ThreadSafeSubtrie>(Existing);
  }
}

TrieContentBase &
ThreadSafeHashMappedTrieBase::insert(LookupResultBase Hint,
                                     std::unique_ptr<TrieContentBase> Content) {
  ArrayRef<uint8_t> Hash = Content->Bytes;
  assert(!Hash.empty() && "Uninitialized hash");

  ThreadSafeSubtrie *S = cast_or_null<ThreadSafeSubtrie>(Root.load());
  if (!S) {
    TrieNode *ExpectedRoot = nullptr;
    std::unique_ptr<ThreadSafeSubtrie> NewRoot =
        ThreadSafeSubtrie::create(0, NumRootBits);
    if (Root.compare_exchange_strong(ExpectedRoot, NewRoot.get()))
      S = NewRoot.release();
    else
      S = cast<ThreadSafeSubtrie>(ExpectedRoot);
  }

  TrieContentBase *OriginalContent = Content.get();
  IndexGenerator IndexGen{NumRootBits, NumSubtrieBits, Hash};
  size_t Index;
  if (Hint.isHint()) {
    S = static_cast<ThreadSafeSubtrie *>(Hint.P);
    Index = IndexGen.hint(Hint.I, Hint.B);
  } else {
    Index = IndexGen.next();
  }

  for (;;) {
    // Try to set the content.
    TrieNode *Existing = S->try_set(Index, Content);
    assert((Existing == nullptr) == (Content == nullptr));
    if (!Existing)
      return *OriginalContent; // Success!

    if (isa<ThreadSafeSubtrie>(Existing)) {
      S = cast<ThreadSafeSubtrie>(Existing);
      Index = IndexGen.next();
      continue;
    }

    // Check for an exact match.
    auto *ExistingContent = cast<TrieContentBase>(Existing);
    if (ExistingContent->Bytes == Hash)
      return *ExistingContent;

    // Sink the existing content as long as the indexes match.
    for (;;) {
      size_t NextIndex = IndexGen.next();
      size_t NewIndexForExistingContent =
          IndexGen.getCollidingBits(ExistingContent->Bytes);
      S = S->sink(Index, ExistingContent, IndexGen.getNumBits(),
                  NewIndexForExistingContent);
      Index = NextIndex;

      // Found the difference.
      if (NextIndex != NewIndexForExistingContent)
        break;
    }
  }
}

static void printHexDigit(raw_ostream &OS, uint8_t Digit) {
  if (Digit < 10)
    OS << char(Digit + '0');
  else
    OS << char(Digit - 10 + 'a');
}

static void printHexDigits(raw_ostream &OS, ArrayRef<uint8_t> Bytes,
                           size_t StartBit, size_t NumBits) {
  assert(StartBit % 4 == 0);
  assert(NumBits % 4 == 0);
  for (size_t I = StartBit, E = StartBit + NumBits; I != E; I += 4) {
    uint8_t HexPair = Bytes[I / 8];
    uint8_t HexDigit = I % 8 == 0 ? HexPair >> 4 : HexPair & 0xf;
    printHexDigit(OS, HexDigit);
  }
}

static void printBits(raw_ostream &OS, ArrayRef<uint8_t> Bytes, size_t StartBit,
                      size_t NumBits) {
  assert(StartBit + NumBits <= Bytes.size() * 8u);
  for (size_t I = StartBit, E = StartBit + NumBits; I != E; ++I) {
    uint8_t Byte = Bytes[I / 8];
    size_t ByteOffset = I % 8;
    if (size_t ByteShift = 8 - ByteOffset - 1)
      Byte >>= ByteShift;
    OS << (Byte & 0x1 ? '1' : '0');
  }
}

void ThreadSafeSubtrie::printHash(raw_ostream &OS,
                                  ArrayRef<uint8_t> Bytes) const {
  // afb[1c:00*01110*0]def
  size_t EndBit = StartBit + NumBits;
  size_t HashEndBit = Bytes.size() * 8u;

  size_t FirstBinaryBit = StartBit & ~0x3u;
  printHexDigits(OS, Bytes, 0, FirstBinaryBit);

  size_t LastBinaryBit = (EndBit + 3u) & ~0x3u;
  OS << "[";
  printBits(OS, Bytes, FirstBinaryBit, LastBinaryBit - FirstBinaryBit);
  OS << "]";

  printHexDigits(OS, Bytes, LastBinaryBit, HashEndBit - LastBinaryBit);
}

static void appendIndexBits(std::string &Prefix, size_t Index,
                            size_t NumSlots) {
  std::string Bits;
  for (size_t NumBits = 1u; NumBits < NumSlots; NumBits <<= 1) {
    Bits.push_back('0' + (Index & 0x1));
    Index >>= 1;
  }
  for (char Ch : llvm::reverse(Bits))
    Prefix += Ch;
}

static void printPrefix(raw_ostream &OS, StringRef Prefix) {
  while (Prefix.size() >= 4) {
    uint8_t Digit;
    bool ErrorParsingBinary = Prefix.take_front(4).getAsInteger(2, Digit);
    assert(!ErrorParsingBinary);
    (void)ErrorParsingBinary;
    printHexDigit(OS, Digit);
    Prefix = Prefix.drop_front(4);
  }
  if (!Prefix.empty())
    OS << "[" << Prefix << "]";
}

void ThreadSafeSubtrie::print(raw_ostream &OS,
                              Optional<std::string> Prefix) const {
  if (!Prefix) {
    OS << "root";
    Prefix.emplace();
  } else {
    OS << "subtrie=";
    printPrefix(OS, *Prefix);
  }

  OS << " num-slots=" << Slots.size() << "\n";
  SmallVector<ThreadSafeSubtrie *> Subs;
  SmallVector<std::string> Prefixes;
  for (size_t I = 0, E = Slots.size(); I != E; ++I) {
    TrieNode *N = get(I);
    if (!N)
      continue;
    OS << "- index=" << I << " ";
    if (auto *S = dyn_cast<ThreadSafeSubtrie>(N)) {
      std::string SubtriePrefix = *Prefix;
      appendIndexBits(SubtriePrefix, I, Slots.size());
      OS << "subtrie=";
      printPrefix(OS, SubtriePrefix);
      OS << "\n";
      Subs.push_back(S);
      Prefixes.push_back(SubtriePrefix);
      continue;
    }
    auto *Content = cast<TrieContentBase>(N);
    OS << "content=";
    printHash(OS, Content->Bytes);
    OS << "\n";
  }
  for (size_t I = 0, E = Subs.size(); I != E; ++I)
    Subs[I]->print(OS, Prefixes[I]);
}

void ThreadSafeHashMappedTrieBase::print(raw_ostream &OS) const {
  OS << "root-bits=" << NumRootBits << " subtrie-bits=" << NumSubtrieBits
     << "\n";
  if (auto *S = cast_or_null<ThreadSafeSubtrie>(Root.load()))
    S->print(OS);
  else
    OS << "[no-root]\n";
}

LLVM_DUMP_METHOD void ThreadSafeHashMappedTrieBase::dump() const {
  print(dbgs());
}

ThreadSafeHashMappedTrieBase::ThreadSafeHashMappedTrieBase(
    ThreadSafeHashMappedTrieBase &&RHS)
    : NumRootBits(RHS.NumRootBits), NumSubtrieBits(RHS.NumSubtrieBits) {
  // Steal the root from RHS.
  TrieNode *RHSRoot = nullptr;
  while (!RHS.Root.compare_exchange_weak(RHSRoot, nullptr)) {
  }
  Root.store(RHSRoot);
}

ThreadSafeHashMappedTrieBase::~ThreadSafeHashMappedTrieBase() {
  delete static_cast<ThreadSafeSubtrie *>(Root.load());
}
