//===- llvm-cas-object-format.cpp - Tool for the CAS object format --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ExecutionEngine/CASObjectFormat/ObjectFileSchema.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/MachO_x86_64.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casobjectformat;

cl::opt<int> NumThreads("num-threads", cl::desc("Num worker threads."));
cl::opt<bool> Dump("dump", cl::desc("Dump link-graph."));
cl::opt<bool>
    JustBlobs("just-blobs",
              cl::desc("Just ingest blobs, instead of breaking up files."));
cl::opt<bool> DebugIngest("debug-ingest",
                          cl::desc("Debug ingesting the object file."));
cl::opt<bool> SplitEHFrames("split-eh", cl::desc("Split eh-frame sections."),
                            cl::init(true));
cl::opt<std::string> CASPath("cas", cl::desc("Path to CAS on disk."));
cl::list<std::string> InputFiles(cl::Positional, cl::desc("Input object"));
cl::opt<bool> ComputeStats("object-stats",
                           cl::desc("Compute and print out stats."));
cl::opt<std::string> JustDsymutil("just-dsymutil",
                                  cl::desc("Just run dsymutil."));

namespace {

class SharedStream {
public:
  SharedStream(raw_ostream &OS) : OS(OS) {}
  void applyLocked(llvm::function_ref<void(raw_ostream &OS)> Fn) {
    std::unique_lock<std::mutex> LockGuard(Lock);
    Fn(OS);
    OS.flush();
  }

private:
  std::mutex Lock;
  raw_ostream &OS;
};

} // namespace

static CASID readFile(CASDB &CAS, StringRef InputFile, SharedStream &OS);
static void computeStats(CASDB &CAS, ArrayRef<CASID> IDs);

int main(int argc, char *argv[]) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  cl::ParseCommandLineOptions(argc, argv);

  std::unique_ptr<CASDB> CAS =
      StringRef(CASPath).empty()
          ? createInMemoryCAS()
          : ExitOnErr(createOnDiskCAS(CASPath == "default-on-disk"
                                          ? getDefaultOnDiskCASPath()
                                          : CASPath));

  Optional<ThreadPool> Pool;
  if (NumThreads != 1) {
    ThreadPoolStrategy PoolStrategy = hardware_concurrency();
    if (NumThreads != 0)
      PoolStrategy.ThreadsRequested = NumThreads;
    Pool.emplace(PoolStrategy);
  }

  StringMap<Optional<CASID>> Files;
  SmallVector<CASID> SummaryIDs;
  SharedStream OS(outs());
  std::mutex Lock;
  for (StringRef IF : InputFiles) {
    auto MaybeID = CAS->parseCASID(IF);
    if (MaybeID) {
      SummaryIDs.emplace_back(*MaybeID);
      continue;
    }
    consumeError(MaybeID.takeError());

    if (Pool) {
      Pool->async([&, IF]() {
        CASID ID = readFile(*CAS, IF, OS);
        std::unique_lock<std::mutex> LockGuard(Lock);
        assert(!Files.count(IF));
        Files[IF] = ID;
      });
    } else {
      CASID ID = readFile(*CAS, IF, OS);
      assert(!Files.count(IF));
      Files[IF] = ID;
    }
  }
  if (Pool)
    Pool->wait();

  outs() << "\n";
  if (!Files.empty()) {
    outs() << "Making summary object...\n";
    HierarchicalTreeBuilder Builder;
    for (auto &File : Files)
      Builder.push(*File.second, TreeEntry::Regular, File.first());
    CASID SummaryID = ExitOnErr(Builder.create(*CAS));
    SummaryIDs.emplace_back(SummaryID);
    outs() << "summary tree: ";
    ExitOnErr(CAS->printCASID(outs(), SummaryID));
    outs() << "\n";
  }

  if (ComputeStats)
    computeStats(*CAS, SummaryIDs);
}

namespace {

class ExternalBitVector {
public:
  using BitWord = uintptr_t;

  enum { BITWORD_SIZE = (unsigned)sizeof(BitWord) * CHAR_BIT };

  static_assert(BITWORD_SIZE == 64 || BITWORD_SIZE == 32,
                "Unsupported word size");

  unsigned Size; // Size of bitvector in bits.
  BitWord *Bits; // Actual bits.

  size_t getNumBitWords() const { return NumBitWords(Size); }

public:
  typedef unsigned size_type;

  // Encapsulation of a single bit.
  class reference {

    BitWord *WordRef;
    unsigned BitPos;

  public:
    reference(ExternalBitVector &b, unsigned Idx) {
      WordRef = &b.Bits[Idx / BITWORD_SIZE];
      BitPos = Idx % BITWORD_SIZE;
    }

    reference() = delete;
    reference(const reference &) = default;

    reference &operator=(reference t) {
      *this = bool(t);
      return *this;
    }

    reference &operator=(bool t) {
      if (t)
        *WordRef |= BitWord(1) << BitPos;
      else
        *WordRef &= ~(BitWord(1) << BitPos);
      return *this;
    }

    operator bool() const { return ((*WordRef) & (BitWord(1) << BitPos)) != 0; }
  };

  typedef const_set_bits_iterator_impl<ExternalBitVector>
      const_set_bits_iterator;
  typedef const_set_bits_iterator set_iterator;

  const_set_bits_iterator set_bits_begin() const {
    return const_set_bits_iterator(*this);
  }
  const_set_bits_iterator set_bits_end() const {
    return const_set_bits_iterator(*this, -1);
  }
  iterator_range<const_set_bits_iterator> set_bits() const {
    return make_range(set_bits_begin(), set_bits_end());
  }

  ExternalBitVector() = delete;
  ExternalBitVector(const ExternalBitVector &) = delete;
  ExternalBitVector(ExternalBitVector &&X) : Size(X.Size), Bits(X.Bits) {
    X.Size = 0;
    X.Bits = nullptr;
  }

private:
  struct just_allocate_tag {};
  explicit ExternalBitVector(just_allocate_tag,
                             SpecificBumpPtrAllocator<BitWord> &Alloc,
                             unsigned Size)
      : Size(Size) {
    size_t NumWords = getNumBitWords();
    Bits = Alloc.Allocate(NumWords);
  }

public:
  explicit ExternalBitVector(SpecificBumpPtrAllocator<BitWord> &Alloc,
                             unsigned Size, bool Value = false)
      : ExternalBitVector(just_allocate_tag{}, Alloc, Size) {
    init_words(Value);
    if (Value)
      clear_unused_bits();
  }

  explicit ExternalBitVector(SpecificBumpPtrAllocator<BitWord> &Alloc,
                             const ExternalBitVector &ToCopy)
      : ExternalBitVector(just_allocate_tag{}, Alloc, ToCopy.Size) {
    std::copy(ToCopy.Bits, ToCopy.Bits + getNumBitWords(), Bits);
  }

  /// empty - Tests whether there are no bits in this bitvector.
  bool empty() const { return Size == 0; }

  /// size - Returns the number of bits in this bitvector.
  size_type size() const { return Size; }

  /// count - Returns the number of bits which are set.
  size_type count() const {
    unsigned NumBits = 0;
    for (auto Bit : getData())
      NumBits += countPopulation(Bit);
    return NumBits;
  }

  /// any - Returns true if any bit is set.
  bool any() const {
    return any_of(getData(), [](BitWord Bit) { return Bit != 0; });
  }

  /// all - Returns true if all bits are set.
  bool all() const {
    for (unsigned i = 0; i < Size / BITWORD_SIZE; ++i)
      if (Bits[i] != ~BitWord(0))
        return false;

    // If bits remain check that they are ones. The unused bits are always zero.
    if (unsigned Remainder = Size % BITWORD_SIZE)
      return Bits[Size / BITWORD_SIZE] == (BitWord(1) << Remainder) - 1;

    return true;
  }

  /// none - Returns true if none of the bits are set.
  bool none() const { return !any(); }

  /// find_first_in - Returns the index of the first set / unset bit,
  /// depending on \p Set, in the range [Begin, End).
  /// Returns -1 if all bits in the range are unset / set.
  int find_first_in(unsigned Begin, unsigned End, bool Set = true) const {
    assert(Begin <= End && End <= Size);
    if (Begin == End)
      return -1;

    unsigned FirstWord = Begin / BITWORD_SIZE;
    unsigned LastWord = (End - 1) / BITWORD_SIZE;

    // Check subsequent words.
    // The code below is based on search for the first _set_ bit. If
    // we're searching for the first _unset_, we just take the
    // complement of each word before we use it and apply
    // the same method.
    for (unsigned i = FirstWord; i <= LastWord; ++i) {
      BitWord Copy = Bits[i];
      if (!Set)
        Copy = ~Copy;

      if (i == FirstWord) {
        unsigned FirstBit = Begin % BITWORD_SIZE;
        Copy &= maskTrailingZeros<BitWord>(FirstBit);
      }

      if (i == LastWord) {
        unsigned LastBit = (End - 1) % BITWORD_SIZE;
        Copy &= maskTrailingOnes<BitWord>(LastBit + 1);
      }
      if (Copy != 0)
        return i * BITWORD_SIZE + countTrailingZeros(Copy);
    }
    return -1;
  }

  /// find_last_in - Returns the index of the last set bit in the range
  /// [Begin, End).  Returns -1 if all bits in the range are unset.
  int find_last_in(unsigned Begin, unsigned End) const {
    assert(Begin <= End && End <= Size);
    if (Begin == End)
      return -1;

    unsigned LastWord = (End - 1) / BITWORD_SIZE;
    unsigned FirstWord = Begin / BITWORD_SIZE;

    for (unsigned i = LastWord + 1; i >= FirstWord + 1; --i) {
      unsigned CurrentWord = i - 1;

      BitWord Copy = Bits[CurrentWord];
      if (CurrentWord == LastWord) {
        unsigned LastBit = (End - 1) % BITWORD_SIZE;
        Copy &= maskTrailingOnes<BitWord>(LastBit + 1);
      }

      if (CurrentWord == FirstWord) {
        unsigned FirstBit = Begin % BITWORD_SIZE;
        Copy &= maskTrailingZeros<BitWord>(FirstBit);
      }

      if (Copy != 0)
        return (CurrentWord + 1) * BITWORD_SIZE - countLeadingZeros(Copy) - 1;
    }

    return -1;
  }

  /// find_first_unset_in - Returns the index of the first unset bit in the
  /// range [Begin, End).  Returns -1 if all bits in the range are set.
  int find_first_unset_in(unsigned Begin, unsigned End) const {
    return find_first_in(Begin, End, /* Set = */ false);
  }

  /// find_last_unset_in - Returns the index of the last unset bit in the
  /// range [Begin, End).  Returns -1 if all bits in the range are set.
  int find_last_unset_in(unsigned Begin, unsigned End) const {
    assert(Begin <= End && End <= Size);
    if (Begin == End)
      return -1;

    unsigned LastWord = (End - 1) / BITWORD_SIZE;
    unsigned FirstWord = Begin / BITWORD_SIZE;

    for (unsigned i = LastWord + 1; i >= FirstWord + 1; --i) {
      unsigned CurrentWord = i - 1;

      BitWord Copy = Bits[CurrentWord];
      if (CurrentWord == LastWord) {
        unsigned LastBit = (End - 1) % BITWORD_SIZE;
        Copy |= maskTrailingZeros<BitWord>(LastBit + 1);
      }

      if (CurrentWord == FirstWord) {
        unsigned FirstBit = Begin % BITWORD_SIZE;
        Copy |= maskTrailingOnes<BitWord>(FirstBit);
      }

      if (Copy != ~BitWord(0)) {
        unsigned Result =
            (CurrentWord + 1) * BITWORD_SIZE - countLeadingOnes(Copy) - 1;
        return Result < Size ? Result : -1;
      }
    }
    return -1;
  }

  /// find_first - Returns the index of the first set bit, -1 if none
  /// of the bits are set.
  int find_first() const { return find_first_in(0, Size); }

  /// find_last - Returns the index of the last set bit, -1 if none of the bits
  /// are set.
  int find_last() const { return find_last_in(0, Size); }

  /// find_next - Returns the index of the next set bit following the
  /// "Prev" bit. Returns -1 if the next set bit is not found.
  int find_next(unsigned Prev) const { return find_first_in(Prev + 1, Size); }

  /// find_prev - Returns the index of the first set bit that precedes the
  /// the bit at \p PriorTo.  Returns -1 if all previous bits are unset.
  int find_prev(unsigned PriorTo) const { return find_last_in(0, PriorTo); }

  /// find_first_unset - Returns the index of the first unset bit, -1 if all
  /// of the bits are set.
  int find_first_unset() const { return find_first_unset_in(0, Size); }

  /// find_next_unset - Returns the index of the next unset bit following the
  /// "Prev" bit.  Returns -1 if all remaining bits are set.
  int find_next_unset(unsigned Prev) const {
    return find_first_unset_in(Prev + 1, Size);
  }

  /// find_last_unset - Returns the index of the last unset bit, -1 if all of
  /// the bits are set.
  int find_last_unset() const { return find_last_unset_in(0, Size); }

  /// find_prev_unset - Returns the index of the first unset bit that precedes
  /// the bit at \p PriorTo.  Returns -1 if all previous bits are set.
  int find_prev_unset(unsigned PriorTo) {
    return find_last_unset_in(0, PriorTo);
  }

  // Set, reset, flip
  ExternalBitVector &set() {
    init_words(true);
    clear_unused_bits();
    return *this;
  }

  ExternalBitVector &set(unsigned Idx) {
    assert(Idx < Size && "access in bound");
    Bits[Idx / BITWORD_SIZE] |= BitWord(1) << (Idx % BITWORD_SIZE);
    return *this;
  }

  /// set - Efficiently set a range of bits in [I, E)
  ExternalBitVector &set(unsigned I, unsigned E) {
    assert(I <= E && "Attempted to set backwards range!");
    assert(E <= size() && "Attempted to set out-of-bounds range!");

    if (I == E)
      return *this;

    if (I / BITWORD_SIZE == E / BITWORD_SIZE) {
      BitWord EMask = BitWord(1) << (E % BITWORD_SIZE);
      BitWord IMask = BitWord(1) << (I % BITWORD_SIZE);
      BitWord Mask = EMask - IMask;
      Bits[I / BITWORD_SIZE] |= Mask;
      return *this;
    }

    BitWord PrefixMask = ~BitWord(0) << (I % BITWORD_SIZE);
    Bits[I / BITWORD_SIZE] |= PrefixMask;
    I = alignTo(I, BITWORD_SIZE);

    for (; I + BITWORD_SIZE <= E; I += BITWORD_SIZE)
      Bits[I / BITWORD_SIZE] = ~BitWord(0);

    BitWord PostfixMask = (BitWord(1) << (E % BITWORD_SIZE)) - 1;
    if (I < E)
      Bits[I / BITWORD_SIZE] |= PostfixMask;

    return *this;
  }

  ExternalBitVector &reset() {
    init_words(false);
    return *this;
  }

  ExternalBitVector &reset(unsigned Idx) {
    Bits[Idx / BITWORD_SIZE] &= ~(BitWord(1) << (Idx % BITWORD_SIZE));
    return *this;
  }

  /// reset - Efficiently reset a range of bits in [I, E)
  ExternalBitVector &reset(unsigned I, unsigned E) {
    assert(I <= E && "Attempted to reset backwards range!");
    assert(E <= size() && "Attempted to reset out-of-bounds range!");

    if (I == E)
      return *this;

    if (I / BITWORD_SIZE == E / BITWORD_SIZE) {
      BitWord EMask = BitWord(1) << (E % BITWORD_SIZE);
      BitWord IMask = BitWord(1) << (I % BITWORD_SIZE);
      BitWord Mask = EMask - IMask;
      Bits[I / BITWORD_SIZE] &= ~Mask;
      return *this;
    }

    BitWord PrefixMask = ~BitWord(0) << (I % BITWORD_SIZE);
    Bits[I / BITWORD_SIZE] &= ~PrefixMask;
    I = alignTo(I, BITWORD_SIZE);

    for (; I + BITWORD_SIZE <= E; I += BITWORD_SIZE)
      Bits[I / BITWORD_SIZE] = BitWord(0);

    BitWord PostfixMask = (BitWord(1) << (E % BITWORD_SIZE)) - 1;
    if (I < E)
      Bits[I / BITWORD_SIZE] &= ~PostfixMask;

    return *this;
  }

  ExternalBitVector &flip() {
    for (auto &Bit : makeMutableArrayRef(Bits, getNumBitWords()))
      Bit = ~Bit;
    clear_unused_bits();
    return *this;
  }

  ExternalBitVector &flip(unsigned Idx) {
    Bits[Idx / BITWORD_SIZE] ^= BitWord(1) << (Idx % BITWORD_SIZE);
    return *this;
  }

  // Indexing.
  reference operator[](unsigned Idx) {
    assert(Idx < Size && "Out-of-bounds Bit access.");
    return reference(*this, Idx);
  }

  bool operator[](unsigned Idx) const {
    assert(Idx < Size && "Out-of-bounds Bit access.");
    BitWord Mask = BitWord(1) << (Idx % BITWORD_SIZE);
    return (Bits[Idx / BITWORD_SIZE] & Mask) != 0;
  }

  bool test(unsigned Idx) const { return (*this)[Idx]; }

  // Comparison operators.
  bool operator==(const ExternalBitVector &RHS) const {
    if (size() != RHS.size())
      return false;
    unsigned NumWords = getNumBitWords();
    return std::equal(Bits, Bits + NumWords, RHS.Bits);
  }

  bool operator!=(const ExternalBitVector &RHS) const {
    return !(*this == RHS);
  }

  /// Intersection, union, disjoint union.
  ExternalBitVector &operator&=(const ExternalBitVector &RHS) {
    unsigned ThisWords = getNumBitWords();
    unsigned RHSWords = RHS.getNumBitWords();
    unsigned i;
    for (i = 0; i != std::min(ThisWords, RHSWords); ++i)
      Bits[i] &= RHS.Bits[i];

    // Any bits that are just in this bitvector become zero, because they aren't
    // in the RHS bit vector.  Any words only in RHS are ignored because they
    // are already zero in the LHS.
    for (; i != ThisWords; ++i)
      Bits[i] = 0;

    return *this;
  }

  /// reset - Reset bits that are set in RHS. Same as *this &= ~RHS.
  ExternalBitVector &reset(const ExternalBitVector &RHS) {
    unsigned ThisWords = getNumBitWords();
    unsigned RHSWords = RHS.getNumBitWords();
    for (unsigned i = 0; i != std::min(ThisWords, RHSWords); ++i)
      Bits[i] &= ~RHS.Bits[i];
    return *this;
  }

  /// test - Check if (This - RHS) is zero.
  /// This is the same as reset(RHS) and any().
  bool test(const ExternalBitVector &RHS) const {
    unsigned ThisWords = getNumBitWords();
    unsigned RHSWords = RHS.getNumBitWords();
    unsigned i;
    for (i = 0; i != std::min(ThisWords, RHSWords); ++i)
      if ((Bits[i] & ~RHS.Bits[i]) != 0)
        return true;

    for (; i != ThisWords; ++i)
      if (Bits[i] != 0)
        return true;

    return false;
  }

  ExternalBitVector &operator|=(const ExternalBitVector &RHS) {
    assert(size() >= RHS.size());
    for (size_t i = 0, e = RHS.getNumBitWords(); i != e; ++i)
      Bits[i] |= RHS.Bits[i];
    return *this;
  }

  ExternalBitVector &operator^=(const ExternalBitVector &RHS) {
    assert(size() >= RHS.size());
    for (size_t i = 0, e = RHS.getNumBitWords(); i != e; ++i)
      Bits[i] ^= RHS.Bits[i];
    return *this;
  }

  ExternalBitVector &operator>>=(unsigned N) {
    assert(N <= Size);
    if (LLVM_UNLIKELY(empty() || N == 0))
      return *this;

    unsigned NumWords = getNumBitWords();
    assert(NumWords >= 1);

    wordShr(N / BITWORD_SIZE);

    unsigned BitDistance = N % BITWORD_SIZE;
    if (BitDistance == 0)
      return *this;

    // When the shift size is not a multiple of the word size, then we have
    // a tricky situation where each word in succession needs to extract some
    // of the bits from the next word and or them into this word while
    // shifting this word to make room for the new bits.  This has to be done
    // for every word in the array.

    // Since we're shifting each word right, some bits will fall off the end
    // of each word to the right, and empty space will be created on the left.
    // The final word in the array will lose bits permanently, so starting at
    // the beginning, work forwards shifting each word to the right, and
    // OR'ing in the bits from the end of the next word to the beginning of
    // the current word.

    // Example:
    //   Starting with {0xAABBCCDD, 0xEEFF0011, 0x22334455} and shifting right
    //   by 4 bits.
    // Step 1: Word[0] >>= 4           ; 0x0ABBCCDD
    // Step 2: Word[0] |= 0x10000000   ; 0x1ABBCCDD
    // Step 3: Word[1] >>= 4           ; 0x0EEFF001
    // Step 4: Word[1] |= 0x50000000   ; 0x5EEFF001
    // Step 5: Word[2] >>= 4           ; 0x02334455
    // Result: { 0x1ABBCCDD, 0x5EEFF001, 0x02334455 }
    const BitWord Mask = maskTrailingOnes<BitWord>(BitDistance);
    const unsigned LSH = BITWORD_SIZE - BitDistance;

    for (unsigned I = 0; I < NumWords - 1; ++I) {
      Bits[I] >>= BitDistance;
      Bits[I] |= (Bits[I + 1] & Mask) << LSH;
    }

    Bits[NumWords - 1] >>= BitDistance;

    return *this;
  }

  ExternalBitVector &operator<<=(unsigned N) {
    assert(N <= Size);
    if (LLVM_UNLIKELY(empty() || N == 0))
      return *this;

    unsigned NumWords = getNumBitWords();
    assert(NumWords >= 1);

    wordShl(N / BITWORD_SIZE);

    unsigned BitDistance = N % BITWORD_SIZE;
    if (BitDistance == 0)
      return *this;

    // When the shift size is not a multiple of the word size, then we have
    // a tricky situation where each word in succession needs to extract some
    // of the bits from the previous word and or them into this word while
    // shifting this word to make room for the new bits.  This has to be done
    // for every word in the array.  This is similar to the algorithm outlined
    // in operator>>=, but backwards.

    // Since we're shifting each word left, some bits will fall off the end
    // of each word to the left, and empty space will be created on the right.
    // The first word in the array will lose bits permanently, so starting at
    // the end, work backwards shifting each word to the left, and OR'ing
    // in the bits from the end of the next word to the beginning of the
    // current word.

    // Example:
    //   Starting with {0xAABBCCDD, 0xEEFF0011, 0x22334455} and shifting left
    //   by 4 bits.
    // Step 1: Word[2] <<= 4           ; 0x23344550
    // Step 2: Word[2] |= 0x0000000E   ; 0x2334455E
    // Step 3: Word[1] <<= 4           ; 0xEFF00110
    // Step 4: Word[1] |= 0x0000000A   ; 0xEFF0011A
    // Step 5: Word[0] <<= 4           ; 0xABBCCDD0
    // Result: { 0xABBCCDD0, 0xEFF0011A, 0x2334455E }
    const BitWord Mask = maskLeadingOnes<BitWord>(BitDistance);
    const unsigned RSH = BITWORD_SIZE - BitDistance;

    for (int I = NumWords - 1; I > 0; --I) {
      Bits[I] <<= BitDistance;
      Bits[I] |= (Bits[I - 1] & Mask) >> RSH;
    }
    Bits[0] <<= BitDistance;
    clear_unused_bits();

    return *this;
  }

  void swap(ExternalBitVector &RHS) {
    std::swap(Bits, RHS.Bits);
    std::swap(Size, RHS.Size);
  }

  ArrayRef<BitWord> getData() const { return {&Bits[0], getNumBitWords()}; }

private:
  /// Perform a logical left shift of \p Count words by moving everything
  /// \p Count words to the right in memory.
  ///
  /// While confusing, words are stored from least significant at Bits[0] to
  /// most significant at Bits[NumWords-1].  A logical shift left, however,
  /// moves the current least significant bit to a higher logical index, and
  /// fills the previous least significant bits with 0.  Thus, we actually
  /// need to move the bytes of the memory to the right, not to the left.
  /// Example:
  ///   Words = [0xBBBBAAAA, 0xDDDDFFFF, 0x00000000, 0xDDDD0000]
  /// represents a BitVector where 0xBBBBAAAA contain the least significant
  /// bits.  So if we want to shift the BitVector left by 2 words, we need
  /// to turn this into 0x00000000 0x00000000 0xBBBBAAAA 0xDDDDFFFF by using a
  /// memmove which moves right, not left.
  void wordShl(uint32_t Count) {
    if (Count == 0)
      return;

    uint32_t NumWords = getNumBitWords();

    // Since we always move Word-sized chunks of data with src and dest both
    // aligned to a word-boundary, we don't need to worry about endianness
    // here.
    std::copy(Bits, Bits + NumWords - Count, Bits + Count);
    std::fill(Bits, Bits + Count, 0);
    clear_unused_bits();
  }

  /// Perform a logical right shift of \p Count words by moving those
  /// words to the left in memory.  See wordShl for more information.
  ///
  void wordShr(uint32_t Count) {
    if (Count == 0)
      return;

    uint32_t NumWords = getNumBitWords();

    std::copy(Bits + Count, Bits + NumWords, Bits);
    std::fill(Bits + NumWords - Count, Bits + NumWords, 0);
  }

  int next_unset_in_word(int WordIndex, BitWord Word) const {
    unsigned Result = WordIndex * BITWORD_SIZE + countTrailingOnes(Word);
    return Result < size() ? Result : -1;
  }

  static unsigned NumBitWords(unsigned S) {
    return (S + BITWORD_SIZE - 1) / BITWORD_SIZE;
  }

  // Set the unused bits in the high words.
  void set_unused_bits(bool t = true) {
    //  Then set any stray high bits of the last used word.
    if (unsigned ExtraBits = Size % BITWORD_SIZE) {
      BitWord ExtraBitMask = ~BitWord(0) << ExtraBits;
      if (t)
        Bits[getNumBitWords() - 1] |= ExtraBitMask;
      else
        Bits[getNumBitWords() - 1] &= ~ExtraBitMask;
    }
  }

  // Clear the unused bits in the high words.
  void clear_unused_bits() { set_unused_bits(false); }

  void init_words(bool t) {
    std::fill(Bits, Bits + getNumBitWords(), 0 - (BitWord)t);
  }

public:
  /// Return the size (in bytes) of the bit vector.
  size_t getMemorySize() const { return getNumBitWords() * sizeof(BitWord); }
  size_t getBitCapacity() const { return getNumBitWords() * BITWORD_SIZE; }
};
} // namespace

static void computeStats(CASDB &CAS, ArrayRef<CASID> TopLevels) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner("llvm-cas-object-format: compute-stats: ");

  outs() << "Collecting object stats...\n";

  // In the first traversal, just collect a POT. Use NumPaths as a "Seen" list.
  struct NodeInfo {
    size_t NumPaths = 0;
    size_t NumParents = 0;
    bool Done = false;
  };
  DenseMap<CASID, NodeInfo> Nodes;
  struct WorklistItem {
    CASID ID;
    bool Visited;
    ObjectFileSchema *Schema = nullptr;
  };
  SmallVector<WorklistItem> Worklist;

  struct POTItem {
    CASID ID;
    ObjectFileSchema *Schema = nullptr;
  };
  SmallVector<POTItem> POT;
  auto push = [&](CASID ID, ObjectFileSchema *Schema = nullptr) {
    auto &Node = Nodes[ID];
    if (!Node.Done)
      Worklist.push_back({ID, false, Schema});
  };
  ObjectFileSchema ObjectsSchema(CAS);
  for (auto ID : TopLevels)
    push(ID);
  while (!Worklist.empty()) {
    CASID ID = Worklist.back().ID;
    if (Worklist.back().Visited) {
      Nodes[ID].Done = true;
      POT.push_back({ID, Worklist.back().Schema});
      Worklist.pop_back();
      continue;
    }
    if (Nodes.lookup(ID).Done) {
      Worklist.pop_back();
      continue;
    }

    Worklist.back().Visited = true;

    // FIXME: Maybe this should just assert?
    Optional<ObjectKind> Kind = CAS.getObjectKind(ID);
    assert(Kind);
    if (!Kind) {
      Worklist.pop_back();
      continue;
    }

    if (*Kind == ObjectKind::Blob)
      continue;

    if (*Kind == ObjectKind::Tree) {
      TreeRef Tree = ExitOnErr(CAS.getTree(ID));
      ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
        push(Entry.getID());
        return Error::success();
      }));
      continue;
    }

    assert(*Kind == ObjectKind::Node);
    NodeRef Node = ExitOnErr(CAS.getNode(ID));
    ObjectFileSchema *&Schema = Worklist.back().Schema;

    // Update the schema.
    if (!Schema) {
      if (ObjectsSchema.isRootNode(Node))
        Schema = &ObjectsSchema;
    } else if (!Schema->isNode(Node)) {
      Schema = nullptr;
    }

    ExitOnErr(Node.forEachReference([&](CASID ChildID) {
      push(ChildID, Schema);
      return Error::success();
    }));
  }

  // Visit POT in reverse (topological sort), computing Nodes and collecting
  // stats.
  struct ObjectKindInfo {
    size_t Count = 0;
    size_t NumPaths = 0;
    size_t NumChildren = 0;
    size_t NumParents = 0;
    size_t DataSize = 0;

    size_t getTotalSize(size_t NumHashBytes) const {
      return NumChildren * NumHashBytes + DataSize;
    }
  };
  StringMap<ObjectKindInfo> Stats;
  ObjectKindInfo Totals;
  DenseSet<StringRef> GeneratedNames;
  DenseSet<cas::CASID> SectionNames;
  DenseSet<cas::CASID> SymbolNames;
  DenseSet<cas::CASID> UndefinedSymbols;
  DenseSet<cas::CASID> ContentBlobs;
  size_t NumContentBlobs0To16B = 0;
  size_t NumAnonymousSymbols = 0;
  size_t NumTemplateSymbols = 0;
  size_t NumTemplateTargets = 0;
  size_t NumZeroFillBlocks = 0;
  size_t Num1TargetBlocks = 0;
  size_t Num2TargetBlocks = 0;
  for (auto ID : TopLevels)
    Nodes[ID].NumPaths = 1;
  SpecificBumpPtrAllocator<ExternalBitVector::BitWord> VectorAlloc;
  for (const POTItem &Item : llvm::reverse(POT)) {
    cas::CASID ID = Item.ID;
    size_t NumPaths = Nodes.lookup(ID).NumPaths;
    Optional<ObjectKind> Kind = CAS.getObjectKind(ID);
    assert(Kind);

    auto updateChild = [&](CASID Child) {
      ++Nodes[Child].NumParents;
      Nodes[Child].NumPaths += NumPaths;
    };

    size_t NumParents = Nodes.lookup(ID).NumParents;
    if (*Kind == ObjectKind::Blob && Item.Schema)
      Kind = ObjectKind::Node; // Hack because nodes with no references look
                               // like blobs.
    if (*Kind == ObjectKind::Blob) {
      auto &Info = Stats["builtin:blob"];
      ++Info.Count;
      Info.DataSize += ExitOnErr(CAS.getBlob(ID)).getData().size();
      Info.NumPaths += NumPaths;
      Info.NumParents += NumParents;
      Totals.NumPaths += NumPaths; // Count paths to leafs.
      continue;
    }

    if (*Kind == ObjectKind::Tree) {
      auto &Info = Stats["builtin:tree"];
      ++Info.Count;
      TreeRef Tree = ExitOnErr(CAS.getTree(ID));
      Info.NumChildren += Tree.size();
      Info.NumParents += NumParents;
      Info.NumPaths += NumPaths;
      if (!Tree.size())
        Totals.NumPaths += NumPaths; // Count paths to leafs.

      ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
        // FIXME: This is copied out of BuiltinCAS.cpp's makeTree.
        Info.DataSize += sizeof(uint64_t) + sizeof(uint32_t) +
                         alignTo(Entry.getName().size() + 1, Align(4));
        updateChild(Entry.getID());
        return Error::success();
      }));
      continue;
    }

    assert(*Kind == ObjectKind::Node);
    cas::NodeRef Node = ExitOnErr(CAS.getNode(ID));
    auto addNodeStats = [&](ObjectKindInfo &Info) {
      ++Info.Count;
      Info.NumChildren += Node.getNumReferences();
      Info.NumParents += NumParents;
      Info.NumPaths += NumPaths;
      Info.DataSize += Node.getData().size();
      if (!Node.getNumReferences())
        Totals.NumPaths += NumPaths; // Count paths to leafs.

      ExitOnErr(Node.forEachReference([&](CASID ChildID) {
        updateChild(ChildID);
        return Error::success();
      }));
    };

    // Handle nodes not in the schema.
    if (!Item.Schema) {
      addNodeStats(Stats["builtin:node"]);
      continue;
    }

    ObjectFileSchema &Schema = *Item.Schema;

    ObjectFormatNodeRef Object = ExitOnErr(Schema.getNode(Node));
    addNodeStats(Stats[Object.getKindString()]);

    // Check specific stats.
    if (Object.getKindString() == BlockRef::KindString) {
      if (Optional<BlockRef> Block =
              expectedToOptional(BlockRef::get(Object))) {
        if (Optional<TargetList> Targets =
                expectedToOptional(Block->getTargets())) {
          for (size_t I = 0, E = Targets->size(); I != E; ++I) {
            if (Optional<TargetRef> Target =
                    expectedToOptional(Targets->get(I))) {
              if (Optional<StringRef> Name =
                      expectedToOptional(Target->getNameString()))
                if (Name->startswith("cas.o:"))
                  GeneratedNames.insert(*Name);

              if (Target->getKind() == TargetRef::Symbol)
                if (Optional<SymbolRef> Symbol =
                        expectedToOptional(SymbolRef::get(Schema, *Target)))
                  NumTemplateTargets += Symbol->isSymbolTemplate();
            }
          }
          Num1TargetBlocks += Targets->size() == 1;
          Num2TargetBlocks += Targets->size() == 2;
        }
        if (Optional<BlockDataRef> Data =
                expectedToOptional(Block->getBlockData())) {
          if (Data->isZeroFill())
            ++NumZeroFillBlocks;
          if (Optional<cas::CASID> Content = Data->getContentID())
            if (ContentBlobs.insert(*Content).second)
              if (Optional<ContentRef> Blob =
                      expectedToOptional(ContentRef::get(Schema, *Content)))
                NumContentBlobs0To16B += Blob->getContent().size() <= 16;
        }
      }
    }
    if (Object.getKindString() == SymbolRef::KindString) {
      if (Optional<SymbolRef> Symbol =
              expectedToOptional(SymbolRef::get(Object))) {
        NumAnonymousSymbols += !Symbol->hasName();
        NumTemplateSymbols += Symbol->isSymbolTemplate();
        if (Optional<cas::CASID> Name = Symbol->getNameID()) {
          SymbolNames.insert(*Name);
          UndefinedSymbols.erase(*Name);
        }
      }
    }
    if (Object.getKindString() == NameListRef::KindString) {
      // FIXME: This is only valid because NameList is currently just used for
      // lists of symbols.
      if (Optional<NameListRef> List =
              expectedToOptional(NameListRef::get(Object))) {
        for (size_t I = 0, E = List->getNumNames(); I != E; ++I) {
          cas::CASID Name = List->getNameID(I);
          if (!SymbolNames.count(Name))
            UndefinedSymbols.insert(Name);
        }
      }
    }
    if (Object.getKindString() == SectionRef::KindString) {
      if (Optional<SectionRef> Section =
              expectedToOptional(SectionRef::get(Object))) {
        if (Optional<cas::CASID> Name = Section->getNameID())
          SectionNames.insert(*Name);
      }
    }
  }

  SmallVector<StringRef> Kinds;
  auto addToTotal = [&Totals](ObjectKindInfo Info) {
    Totals.Count += Info.Count;
    Totals.NumChildren += Info.NumChildren;
    Totals.NumParents += Info.NumParents;
    Totals.DataSize += Info.DataSize;
  };
  for (auto &I : Stats) {
    Kinds.push_back(I.first());
    addToTotal(I.second);
  }
  size_t NumHashBytes = TopLevels.front().getHash().size();
  llvm::sort(Kinds);

  outs() << "  => Note: 'Parents' counts incoming edges\n"
         << "  => Note: 'Children' counts outgoing edges (to sub-objects)\n"
         << "  => Note: number of bytes per Ref = " << NumHashBytes << "\n"
         << "  => Note: Cost = sizeof(Ref)*Children + Data\n";
  StringLiteral HeaderFormat = "{0,-22} {1,+10} {2,+7} {3,+10} {4,+7} {5,+10} "
                               "{6,+7} {7,+10} {8,+7} {9,+10} {10,+7}\n";
  StringLiteral Format = "{0,-22} {1,+10} {2,+7:P} {3,+10} {4,+7:P} {5,+10} "
                         "{6,+7:P} {7,+10} {8,+7:P} {9,+10} {10,+7:P}\n";
  outs() << llvm::formatv(HeaderFormat.begin(), "Kind", "Count", "", "Parents",
                          "", "Children", "", "Data (B)", "", "Cost (B)", "");
  outs() << llvm::formatv(HeaderFormat.begin(), "====", "=====", "",
                          "=======", "", "========", "", "========", "",
                          "========", "");
  auto printInfo = [Format, NumHashBytes, Totals](StringRef Kind,
                                                  ObjectKindInfo Info) {
    if (!Info.Count)
      return;
    auto getPercent = [](double N, double D) { return D ? N / D : 0.0; };
    size_t Size = Info.getTotalSize(NumHashBytes);
    outs() << llvm::formatv(
        Format.begin(), Kind, Info.Count, getPercent(Info.Count, Totals.Count),
        Info.NumParents, getPercent(Info.NumParents, Totals.NumParents),
        Info.NumChildren, getPercent(Info.NumChildren, Totals.NumChildren),
        Info.DataSize, getPercent(Info.DataSize, Totals.DataSize), Size,
        getPercent(Size, Totals.getTotalSize(NumHashBytes)));
  };
  for (StringRef Kind : Kinds)
    printInfo(Kind, Stats.lookup(Kind));
  printInfo("TOTAL", Totals);

  // Other stats.
  bool HasPrinted = false;
  auto printIfNotZero = [&HasPrinted](StringRef Name, size_t Num) {
    if (!Num)
      return;
    if (!HasPrinted)
      outs() << "\n";
    outs() << llvm::formatv("{0,-22} {1,+10}\n", Name, Num);
    HasPrinted = true;
  };

  printIfNotZero("num-generated-names", GeneratedNames.size());
  printIfNotZero("num-section-names", SectionNames.size());
  printIfNotZero("num-symbol-names",
                 SymbolNames.size() + UndefinedSymbols.size());
  printIfNotZero("num-undefined-symbols", UndefinedSymbols.size());
  printIfNotZero("num-content-0-to-16B", NumContentBlobs0To16B);
  printIfNotZero("num-anonymous-symbols", NumAnonymousSymbols);
  printIfNotZero("num-template-symbols", NumTemplateSymbols);
  printIfNotZero("num-template-targets", NumTemplateTargets);
  printIfNotZero("num-zero-fill-blocks", NumZeroFillBlocks);
  printIfNotZero("num-1-target-blocks", Num2TargetBlocks);
  printIfNotZero("num-2-target-blocks", Num1TargetBlocks);
}

static void dumpGraph(jitlink::LinkGraph &G, SharedStream &OS, StringRef Desc) {
  SmallString<1024> Data;
  raw_svector_ostream Dump(Data);
  G.dump(Dump);
  OS.applyLocked([&](raw_ostream &OS) { OS << Desc << ":\n" << Data; });
}

static CASID readFile(CASDB &CAS, StringRef InputFile, SharedStream &OS) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(("llvm-cas-object-format: " + InputFile + ": ").str());

  auto EmitDotOnReturn = llvm::make_scope_exit(
      [&]() { OS.applyLocked([&](raw_ostream &OS) { OS << "."; }); });

  auto ObjBuffer =
      ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(InputFile)));
  if (JustBlobs)
    return ExitOnErr(CAS.createBlob(ObjBuffer->getBuffer()));

  auto G = ExitOnErr(
      jitlink::createLinkGraphFromObject(ObjBuffer->getMemBufferRef()));

  if (SplitEHFrames &&
      G->getTargetTriple().getObjectFormat() == Triple::MachO &&
      G->getTargetTriple().getArch() == Triple::x86_64) {
    ExitOnErr(jitlink::createEHFrameSplitterPass_MachO_x86_64()(*G));
    ExitOnErr(jitlink::createEHFrameEdgeFixerPass_MachO_x86_64()(*G));
  } else {
    dbgs() << "Note: not fixing eh-frame sections\n";
  }

  if (!JustDsymutil.empty()) {
    // Create a map for each symbol in TEXT.
    jitlink::Section *Text = G->findSectionByName("__TEXT,__text");
    if (!Text)
      return ExitOnErr(CAS.createTree());

    auto MapFile = ExitOnErr(sys::fs::TempFile::create("/tmp/debug-%%%%%%%%.map"));
    auto DsymFile =
        ExitOnErr(sys::fs::TempFile::create(MapFile.TmpName + ".dwarf"));
    Optional<StringRef> Redirects[] = {
        StringRef(),
        None,
        None,
    };

    DenseSet<jitlink::Block *> Seen;
    HierarchicalTreeBuilder Builder;
    for (jitlink::Symbol *S : Text->symbols()) {
      if (!S->isDefined())
        continue;

      assert(S->hasName() && "Expected __TEXT,__text symbols to have names");
      jitlink::Block *B = &S->getBlock();
      if (!Seen.insert(B).second)
        continue;

      // Write map.
      {
        std::error_code EC;
        raw_fd_ostream OS(MapFile.TmpName, EC);
        ExitOnErr(errorCodeToError(EC));
        OS << "---\n";
        OS << "triple:          'x86_64-apple-darwin'\n";
        OS << "objects:\n";
        OS << "  - filename: " << InputFile << "\n";
        OS << "    symbols:\n";
        OS << "      - { sym: " << S->getName()
           << ", objAddr: 0x0, binAddr: 0x0, size: "
           << formatv("{0:x}", B->getSize()) << " }\n";
        OS << "...\n";
      }

      // Call dsymutil.
      StringRef Args[] = {
          JustDsymutil,
          "-y",
          "--accelerator",
          "None",
          "--flat",
          "--num-threads",
          "1",
          "-o",
          DsymFile.TmpName,
          MapFile.TmpName,
      };

      if (sys::ExecuteAndWait(JustDsymutil, Args, None, Redirects))
        ExitOnErr(
            createStringError(inconvertibleErrorCode(), "dsymutil failed"));

      // Read dsym.
      auto DwarfBuffer =
          ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(DsymFile.TmpName)));
      Builder.push(ExitOnErr(CAS.createBlob(DwarfBuffer->getBuffer())),
                   TreeEntry::Regular, S->getName());
    }
    ExitOnErr(MapFile.discard());
    ExitOnErr(DsymFile.discard());
    return ExitOnErr(Builder.create(CAS));
  }

  if (Dump)
    dumpGraph(*G, OS, "parse result");

  SmallString<256> DebugIngestOutput;
  Optional<raw_svector_ostream> DebugOS;
  if (DebugIngest)
    DebugOS.emplace(DebugIngestOutput);
  ObjectFileSchema Schema(CAS);
  auto CompileUnit = ExitOnErr(
      CompileUnitRef::create(Schema, *G, DebugIngest ? &*DebugOS : nullptr));

  if (DebugIngest)
    OS.applyLocked([&](raw_ostream &OS) { OS << DebugIngestOutput; });

  auto RoundTripG = ExitOnErr(CompileUnit.createLinkGraph(
      ObjBuffer->getBufferIdentifier(),
      ExitOnErr(jitlink::getGetEdgeKindNameFunction(G->getTargetTriple()))));

  if (Dump)
    dumpGraph(*RoundTripG, OS, "after round-trip");

  return CompileUnit;
}
