//===- OnDiskHashMappedTrie.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/OnDiskHashMappedTrie.h"
#include "HashMappedTrieIndexGenerator.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::cas;

namespace {
class OnDiskOffset {
public:
  explicit operator bool() const { return !isEmpty(); }
  bool isEmpty() const { return !Offset; }
  bool isData() const { return Offset > 0; }
  bool isSubtrie() const { return Offset < 0; }
  int64_t asData() const {
    assert(isData());
    return Offset;
  }
  int64_t asSubtrie() const {
    assert(isSubtrie());
    return -Offset;
  }

  int64_t getRawOffset() const { return Offset; }

  static OnDiskOffset getDataOffset(int64_t Offset) {
    return OnDiskOffset(Offset);
  }

  static OnDiskOffset getSubtrieOffset(int64_t Offset) {
    return OnDiskOffset(-Offset);
  }

  static OnDiskOffset getFromSlot(std::atomic<int64_t> &Slot) {
    return OnDiskOffset(Slot.load());
  }

  OnDiskOffset() = default;

private:
  friend struct OnDiskSubtrie;
  explicit OnDiskOffset(int64_t Offset) : Offset(Offset) {}
  int64_t Offset = 0;
};

struct OnDiskIndexHeader;
struct OnDiskSubtrie final {
  uint32_t StartBit;
  uint32_t NumBits;

  static int64_t getSlotsSize(uint32_t NumBits) {
    return sizeof(int64_t) * (1u << NumBits);
  }

  static int64_t getSize(uint32_t NumBits) {
    return sizeof(OnDiskSubtrie) + getSlotsSize(NumBits);
  }

  int64_t getSize() const { return getSize(NumBits); }

  MutableArrayRef<std::atomic<int64_t>> getSlots() {
    return makeMutableArrayRef(
        reinterpret_cast<std::atomic<int64_t> *>(this + 1), 1u << NumBits);
  }

  OnDiskOffset get(size_t I);

  /// Return None on success, or the existing offset on failure.
  Optional<OnDiskOffset> try_set(size_t I, OnDiskOffset New);

  /// Return None on success, or the existing offset on failure.
  Optional<OnDiskOffset> try_replace(size_t I, OnDiskOffset Expected,
                                     OnDiskOffset New);

  /// Only safe if the subtrie is empty.
  void reinitialize(uint32_t StartBit, uint32_t NumBits);

private:
  friend struct OnDiskIndexHeader;
  OnDiskSubtrie(uint32_t StartBit, uint32_t NumBits)
      : StartBit(StartBit), NumBits(NumBits) {
    auto Slots = getSlots();
    for (auto I = Slots.begin(), E = Slots.end(); I != E; ++I)
      new (I) std::atomic<int64_t>(0);
  }
};

struct OnDiskData final {
  unsigned HashSize : 16;
  unsigned HashPaddingSize : 8;
  unsigned MetadataPaddingSize : 8;
  unsigned MetadataSize : 31;
  unsigned IsEmbedded : 1;
  uint64_t DataSize;

  // Files 64KB and bigger can just be saved separately.
  //
  // FIXME: Should be configurable.
  static constexpr int64_t MaxEmbeddedDataSize = 64ull * 1024ull - 1ull;

  // FIXME: This is being used as a hack to disable an assertion. The problem
  // is that if there are LOTS of references it the number above can exceed
  // MaxEmbeddedDataSize, even if the data itself is tiny. The right fix is to
  // sometimes (always?) make references external too.
  static constexpr int64_t MinExternalDataSize = 4ull * 1024ull - 1ull;

  static uint64_t getHashOffset() { return sizeof(OnDiskData); }

  static uint64_t getPadding(uint64_t Size) {
    if (uint64_t Remainder = Size % 8)
      return 8 - Remainder;
    return 0;
  }

  static uint64_t getHashPaddingStart(uint64_t HashSize) {
    return sizeof(OnDiskData) + HashSize;
  }

  static uint64_t getSizeIfExternal(uint64_t HashSize, uint64_t MetadataSize) {
    return sizeof(OnDiskData) + HashSize + getPadding(HashSize) + MetadataSize +
           getPadding(MetadataSize);
  }

  static uint64_t getDataPadding(uint64_t DataSize) {
    // Always at least one zero for guaranteed null termination.
    return getPadding(DataSize + 1) + 1;
  }

  static uint64_t getDataSizeWithPadding(uint64_t DataSize) {
    return DataSize + getDataPadding(DataSize);
  }

  static uint64_t getSizeIfEmbedded(ArrayRef<uint8_t> Hash, StringRef Metadata,
                                    StringRef Data) {
    return getSizeIfExternal(Hash.size(), Metadata.size()) +
           getDataSizeWithPadding(Data.size());
  }

  ArrayRef<uint8_t> getHash() const {
    return makeArrayRef(
        reinterpret_cast<const uint8_t *>(this) + getHashOffset(), HashSize);
  }

  bool isEmbedded() const { return IsEmbedded; }
  bool isExternal() const { return !isEmbedded(); }

  const char *getHashStart() const {
    return reinterpret_cast<const char *>(this) + sizeof(OnDiskData);
  }
  const char *getHashPaddingStart() const { return getHashStart() + HashSize; }
  const char *getMetadataStart() const {
    return getHashPaddingStart() + HashPaddingSize;
  }
  const char *getMetadataPaddingStart() const {
    return getMetadataStart() + MetadataSize;
  }
  const char *getDataStart() const {
    return getMetadataPaddingStart() + MetadataPaddingSize;
  }
  const char *getDataPaddingStart() const { return getDataStart() + DataSize; }
  StringRef getMetadata() const {
    return StringRef(getMetadataStart(), MetadataSize);
  }

  Optional<StringRef> getData() const {
    if (isExternal())
      return None;
    return StringRef(getDataStart(), DataSize);
  }

  using MappedContentReference = OnDiskHashMappedTrie::MappedContentReference;
  MappedContentReference getContent(StringRef DataPath) const;

  static void createExternalContent(StringRef DataPath, ArrayRef<uint8_t> Hash,
                                    StringRef Metadata, StringRef Data);

private:
  friend struct OnDiskDataHeader;
  struct ImplTag {};
  struct ExternalTag {};
  OnDiskData(ImplTag, bool IsEmbedded, ArrayRef<uint8_t> Hash,
             StringRef Metadata, uint64_t DataSize);
  OnDiskData(ExternalTag, ArrayRef<uint8_t> Hash, StringRef Metadata,
             uint64_t DataSize);
  OnDiskData(ArrayRef<uint8_t> Hash, StringRef Metadata, StringRef Data);
};

struct OnDiskHeaderBase {
  std::atomic<uint64_t> Magic;
  std::atomic<int64_t> EndOffset;

protected:
  int64_t push(llvm::function_ref<Error(uint64_t)> RequestFileSize,
               uint64_t Size);

  OnDiskHeaderBase(uint64_t Magic, int64_t EndOffset)
      : Magic(Magic), EndOffset(EndOffset) {}
};

struct OnDiskIndexHeader final : OnDiskHeaderBase {
  uint64_t NumRootBits;
  uint64_t NumSubtrieBits;
  uint64_t NumHashBits;
  OnDiskSubtrie Root;

  OnDiskSubtrie *getSubtrie(OnDiskOffset Slot) {
    assert(Slot.isSubtrie());
    assert(Slot.asSubtrie() < EndOffset.load());
    assert(Slot.asSubtrie() >= int64_t(sizeof(OnDiskIndexHeader)));
    return reinterpret_cast<OnDiskSubtrie *>(reinterpret_cast<char *>(this) +
                                             Slot.asSubtrie());
  }
  OnDiskOffset
  createSubtrie(llvm::function_ref<Error(uint64_t)> RequestFileSize,
                uint32_t StartBit, uint32_t NumBits);

  OnDiskIndexHeader(uint64_t NumRootBits, uint64_t NumSubtrieBits,
                    uint64_t NumHashBits);
};

struct OnDiskDataHeader final : OnDiskHeaderBase {
  OnDiskData *getData(OnDiskOffset Slot) {
    assert(Slot.isData());
    assert(Slot.asData() < EndOffset.load());
    assert(Slot.asData() >= int64_t(sizeof(OnDiskDataHeader)));
    return reinterpret_cast<OnDiskData *>(reinterpret_cast<char *>(this) +
                                          Slot.asData());
  }
  OnDiskOffset createData(StringRef DataPath,
                          llvm::function_ref<Error(uint64_t)> RequestFileSize,
                          ArrayRef<uint8_t> Hash, StringRef Metadata,
                          StringRef Data);

  OnDiskDataHeader();
};
} // namespace

static void appendHash(ArrayRef<uint8_t> RawHash, SmallVectorImpl<char> &Dest) {
  assert(Dest.empty());
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

OnDiskData::OnDiskData(ImplTag, bool IsEmbedded, ArrayRef<uint8_t> Hash,
                       StringRef Metadata, uint64_t DataSize)
    : HashSize(Hash.size()), HashPaddingSize(getPadding(Hash.size())),
      MetadataPaddingSize(getPadding(Metadata.size())),
      MetadataSize(Metadata.size()), IsEmbedded(IsEmbedded),
      DataSize(DataSize) {
  assert(Metadata.size() < (1u << 31) && "Too much metadata");
  ::memcpy(const_cast<char *>(getHashStart()), Hash.begin(), HashSize);
  ::memcpy(const_cast<char *>(getMetadataStart()), Metadata.begin(),
           MetadataSize);

  // Fill padding with 0s in case the filesystem doesn't guarantee it
  // (Windows).
  ::memset(const_cast<char *>(getHashPaddingStart()), 0, HashPaddingSize);
  ::memset(const_cast<char *>(getMetadataPaddingStart()), 0,
           MetadataPaddingSize);
}

OnDiskData::OnDiskData(ExternalTag, ArrayRef<uint8_t> Hash, StringRef Metadata,
                       uint64_t DataSize)
    : OnDiskData(ImplTag(), /*IsEmbedded=*/false, Hash, Metadata, DataSize) {}

OnDiskData::OnDiskData(ArrayRef<uint8_t> Hash, StringRef Metadata,
                       StringRef Data)
    : OnDiskData(ImplTag(), /*IsEmbedded=*/true, Hash, Metadata, Data.size()) {
  ::memcpy(const_cast<char *>(getDataStart()), Data.begin(), DataSize);

  // Fill padding with 0s in case the filesystem doesn't guarantee it
  // (Windows).
  ::memset(const_cast<char *>(getDataPaddingStart()), 0,
           getDataPadding(DataSize));
}

int64_t
OnDiskHeaderBase::push(llvm::function_ref<Error(uint64_t)> RequestFileSize,
                       uint64_t SizeDelta) {
  int64_t CurrentEnd = EndOffset.load();
  assert(CurrentEnd >= int64_t(sizeof(OnDiskHeaderBase)));
  assert(CurrentEnd % 8 == 0);

  int64_t NewEnd;
  do {
    NewEnd = CurrentEnd + SizeDelta;
    if (Error E = RequestFileSize(NewEnd))
      report_fatal_error(std::move(E));
  } while (!EndOffset.compare_exchange_weak(CurrentEnd, NewEnd));
  assert(EndOffset.load() >= int64_t(CurrentEnd + SizeDelta));

  return CurrentEnd;
}

OnDiskOffset OnDiskIndexHeader::createSubtrie(
    llvm::function_ref<Error(uint64_t)> RequestFileSize, uint32_t StartBit,
    uint32_t NumBits) {
  int64_t Size = OnDiskSubtrie::getSize(NumBits);
  auto Offset = OnDiskOffset::getSubtrieOffset(push(RequestFileSize, Size));
  new (getSubtrie(Offset)) OnDiskSubtrie(StartBit, NumBits);
  return Offset;
}

namespace {
/// Copy of \a sys::fs::TempFile that skips RemoveOnSignal, which is too
/// expensive to register/unregister at this rate.
///
/// FIXME: Add a TempFileManager that maintains a thread-safe list of open temp
/// files and has a signal handler registerd that removes them all.
class TempFile {
  bool Done = false;
  TempFile(StringRef Name, int FD) : TmpName(std::string(Name)), FD(FD) {}

public:
  /// This creates a temporary file with createUniqueFile and schedules it for
  /// deletion with sys::RemoveFileOnSignal.
  static Expected<TempFile> create(const Twine &Model);
  TempFile(TempFile &&Other) { *this = std::move(Other); }
  TempFile &operator=(TempFile &&Other) {
    TmpName = std::move(Other.TmpName);
    FD = Other.FD;
    Other.Done = true;
    Other.FD = -1;
    return *this;
  }

  // Name of the temporary file.
  std::string TmpName;

  // The open file descriptor.
  int FD = -1;

  // Keep this with the given name.
  Error keep(const Twine &Name);

  // Delete the file.
  Error discard();

  // This checks that keep or delete was called.
  ~TempFile() { assert(Done); }
};
} // namespace

Error TempFile::discard() {
  Done = true;
  if (FD != -1)
    if (std::error_code EC = sys::fs::closeFile(FD))
      return errorCodeToError(EC);
  FD = -1;

  // Always try to close and remove.
  std::error_code RemoveEC;
  if (!TmpName.empty())
    if (std::error_code EC = sys::fs::remove(TmpName))
      return errorCodeToError(EC);
  TmpName = "";

  return Error::success();
}

Error TempFile::keep(const Twine &Name) {
  assert(!Done);
  Done = true;
  // Always try to close and rename.
  std::error_code RenameEC = sys::fs::rename(TmpName, Name);

  if (!RenameEC)
    TmpName = "";

  if (std::error_code EC = sys::fs::closeFile(FD))
    return errorCodeToError(EC);
  FD = -1;

  return errorCodeToError(RenameEC);
}

Expected<TempFile> TempFile::create(const Twine &Model) {
  int FD;
  SmallString<128> ResultPath;
  if (std::error_code EC = sys::fs::createUniqueFile(Model, FD, ResultPath))
    return errorCodeToError(EC);

  TempFile Ret(ResultPath, FD);
  return std::move(Ret);
}

OnDiskHashMappedTrie::MappedContentReference
OnDiskData::getContent(StringRef DataPath) const {
  if (isEmbedded())
    return MappedContentReference(getMetadata(), *getData());

  SmallString<128> HashString;
  appendHash(getHash(), HashString);
  SmallString<256> ContentPath = sys::path::parent_path(DataPath);
  sys::path::append(ContentPath, HashString);

  Expected<sys::fs::file_t> ContentFD =
      sys::fs::openNativeFileForRead(ContentPath, sys::fs::OF_None);
  if (!ContentFD)
    report_fatal_error(ContentFD.takeError());
  auto CloseOnReturn =
      llvm::make_scope_exit([&ContentFD]() { sys::fs::closeFile(*ContentFD); });

  // Load with the padding to guarantee null termination.
  int64_t ExternalSize = getDataSizeWithPadding(DataSize);
  sys::fs::mapped_file_region Map;
  {
    std::error_code EC;
    Map = sys::fs::mapped_file_region(
        sys::fs::convertFDToNativeFile(*ContentFD),
        sys::fs::mapped_file_region::readonly, ExternalSize, 0, EC);
    if (EC)
      report_fatal_error(createFileError(ContentPath, EC));
  }

  StringRef Data(Map.data(), DataSize);
  return MappedContentReference(getMetadata(), Data, std::move(Map));
}

void OnDiskData::createExternalContent(StringRef DataPath,
                                       ArrayRef<uint8_t> Hash,
                                       StringRef Metadata, StringRef Data) {
  assert(Data.size() >= MinExternalDataSize &&
         "Expected a bigger file for external content...");

  SmallString<256> ContentPath = sys::path::parent_path(DataPath);
  SmallString<128> HashString;
  appendHash(Hash, HashString);
  sys::path::append(ContentPath, HashString);

  int64_t FileSize = OnDiskData::getDataSizeWithPadding(Data.size());

  if (sys::fs::exists(ContentPath))
    return;

  Expected<TempFile> File = TempFile::create(ContentPath + ".%%%%%%");
  if (!File)
    report_fatal_error(File.takeError());

  if (auto EC =
          sys::fs::resize_file_before_mapping_readwrite(File->FD, FileSize))
    report_fatal_error(createFileError(File->TmpName, EC));

  {
    std::error_code EC;
    sys::fs::mapped_file_region MappedFile(
        sys::fs::convertFDToNativeFile(File->FD),
        sys::fs::mapped_file_region::readwrite, FileSize, 0, EC);
    if (EC)
      report_fatal_error(createFileError(File->TmpName, EC));

    ::memcpy(MappedFile.data(), Data.begin(), Data.size());
    ::memset(MappedFile.data() + Data.size(), 0,
             OnDiskData::getDataPadding(Data.size()));
  }

  if (Error E = File->keep(ContentPath))
    report_fatal_error(std::move(E));
}

OnDiskOffset OnDiskDataHeader::createData(
    StringRef DataPath, llvm::function_ref<Error(uint64_t)> RequestFileSize,
    ArrayRef<uint8_t> Hash, StringRef Metadata, StringRef Data) {
  int64_t Size = OnDiskData::getSizeIfEmbedded(Hash, Metadata, Data);

  // FIXME: This logic isn't right. References should be stored externally if
  // there are enough that we'll go over the max embedded data size limit.
  // Right now only the data is ever made external.... but we could blow out
  // the main mmap if there are lots of references.
  if (Size <= OnDiskData::MaxEmbeddedDataSize ||
      Data.size() < OnDiskData::MinExternalDataSize) {
    auto Offset = OnDiskOffset::getDataOffset(push(RequestFileSize, Size));
    new (getData(Offset)) OnDiskData(Hash, Metadata, Data);
    return Offset;
  }

  OnDiskData::createExternalContent(DataPath, Hash, Metadata, Data);
  Size = OnDiskData::getSizeIfExternal(Hash.size(), Metadata.size());
  auto Offset = OnDiskOffset::getDataOffset(push(RequestFileSize, Size));
  new (getData(Offset))
      OnDiskData(OnDiskData::ExternalTag(), Hash, Metadata, Data.size());
  return Offset;
}

OnDiskOffset OnDiskSubtrie::get(size_t I) {
  return OnDiskOffset(getSlots()[I].load());
}

Optional<OnDiskOffset> OnDiskSubtrie::try_set(size_t I, OnDiskOffset New) {
  return try_replace(I, OnDiskOffset(), New);
}

Optional<OnDiskOffset>
OnDiskSubtrie::try_replace(size_t I, OnDiskOffset Expected, OnDiskOffset New) {
  assert(New);
  int64_t Old = Expected.Offset;
  if (!getSlots()[I].compare_exchange_strong(Old, New.Offset)) {
    assert(Old != Expected.Offset);
    return OnDiskOffset(Old);
  }
  // Success.
  return None;
}

OnDiskHashMappedTrie::LookupResult
OnDiskHashMappedTrie::lookup(ArrayRef<uint8_t> Hash) const {
  assert(!Hash.empty() && "Uninitialized hash");
  auto *IndexHeader = reinterpret_cast<OnDiskIndexHeader *>(Index->Map.data());
  auto *DataHeader = reinterpret_cast<OnDiskDataHeader *>(Data->Map.data());
  assert(IndexHeader->NumHashBits == Hash.size() * sizeof(uint8_t));

  OnDiskSubtrie *S = &IndexHeader->Root;
  IndexGenerator IndexGen{IndexHeader->NumRootBits, IndexHeader->NumSubtrieBits,
                          Hash};
  size_t Index = IndexGen.next();
  for (;;) {
    // Try to set the content.
    OnDiskOffset Existing = S->get(Index);
    if (!Existing)
      return LookupResult(S, Index, *IndexGen.StartBit);

    // Check for an exact match.
    if (Existing.isData()) {
      OnDiskData *ExistingData = DataHeader->getData(Existing);
      return ExistingData->getHash() == Hash
                 ? LookupResult(ExistingData->getContent(Data->Path))
                 : LookupResult(S, Index, *IndexGen.StartBit);
    }

    Index = IndexGen.next();
    S = IndexHeader->getSubtrie(Existing);
  }
}

/// Only safe if the subtrie is empty.
void OnDiskSubtrie::reinitialize(uint32_t StartBit, uint32_t NumBits) {
  assert(StartBit > this->StartBit);
  assert(NumBits <= this->NumBits);
  // Ideally would also assert that all slots are empty, but that's expensive.

  this->StartBit = StartBit;
  this->NumBits = NumBits;
}

Error OnDiskHashMappedTrie::MappedFileInfo::requestFileSize(uint64_t Size) {
  // Common case.
  if (Size <= OnDiskSize.load())
    return Error::success();

  // Synchronize with other threads.
  std::lock_guard<std::mutex> Lock(Mutex);
  uint64_t OldSize = OnDiskSize.load();
  if (Size <= OldSize)
    return Error::success();

  // Increase sizes by doubling up to 8MB, and then limit the over-allocation
  // to 4MB.
  static constexpr uint64_t MaxIncrement = 4ull * 1024ull * 1024ull;
  uint64_t NewSize;
  if (Size < MaxIncrement)
    NewSize = NextPowerOf2(Size);
  else
    NewSize = alignTo(Size, MaxIncrement);

  if (NewSize > Map.size())
    NewSize = Map.size();
  if (NewSize < Size)
    return errorCodeToError(std::make_error_code(std::errc::not_enough_memory));

  // Synchronize with other processes.
  if (std::error_code EC = sys::fs::lockFile(FD))
    return errorCodeToError(EC);
  auto Unlock = make_scope_exit([&]() { sys::fs::unlockFile(FD); });

  sys::fs::file_status Status;
  if (std::error_code EC = sys::fs::status(FD, Status))
    return errorCodeToError(EC);
  if (Status.getSize() >= Size) {
    // If there's enough space just go for it.
    OnDiskSize = Size;
    return Error::success();
  }

  if (std::error_code EC = sys::fs::resize_file(FD, NewSize))
    return errorCodeToError(EC);
  OnDiskSize = Size;
  return Error::success();
}

OnDiskHashMappedTrie::MappedContentReference
OnDiskHashMappedTrie::insert(LookupResult Hint, ArrayRef<uint8_t> Hash,
                             StringRef Metadata, StringRef Content) {
  assert(!Hash.empty() && "Uninitialized hash");
  auto *IndexHeader = reinterpret_cast<OnDiskIndexHeader *>(Index->Map.data());
  auto *DataHeader = reinterpret_cast<OnDiskDataHeader *>(Data->Map.data());
  assert(IndexHeader->NumHashBits == Hash.size() * sizeof(uint8_t));

  OnDiskSubtrie *S = &IndexHeader->Root;
  IndexGenerator IndexGen{IndexHeader->NumRootBits, IndexHeader->NumSubtrieBits,
                          Hash};
  size_t Index;
  if (Hint.isHint()) {
    S = static_cast<OnDiskSubtrie *>(const_cast<void *>(Hint.S));
    Index = IndexGen.hint(Hint.I, Hint.B);
  } else {
    Index = IndexGen.next();
  }

  Optional<OnDiskOffset> NewData;
  Optional<OnDiskOffset> NewSubtrie;
  for (;;) {
    // To minimize leaks, first check the data, only allocating an on-disk
    // record if it's not already in the map.
    OnDiskOffset Existing = S->get(Index);

    // Try to set it, if it's empty.
    if (!Existing) {
      if (!NewData)
        NewData = DataHeader->createData(
            Data->Path,
            [this](uint64_t Size) { return Data->requestFileSize(Size); }, Hash,
            Metadata, Content);
      assert(NewData->asData() < DataHeader->EndOffset.load());
      Optional<OnDiskOffset> Race = S->try_set(Index, *NewData);
      if (!Race) // Success!
        return DataHeader->getData(*NewData)->getContent(Data->Path);
      assert(*Race && "Expected non-empty, or else why the race?");
      Existing = *Race;
    }

    if (Existing.isSubtrie()) {
      S = IndexHeader->getSubtrie(Existing);
      Index = IndexGen.next();
      continue;
    }

    // Check for an exact match.
    OnDiskData *ExistingData = DataHeader->getData(Existing);
    if (ExistingData->getHash() == Hash)
      return ExistingData->getContent(Data->Path); // Already there!

    // Sink the existing content as long as the indexes match.
    for (;;) {
      size_t NextIndex = IndexGen.next();
      size_t NewIndexForExistingContent =
          IndexGen.getCollidingBits(ExistingData->getHash());

      OnDiskSubtrie *NewS;
      if (!NewSubtrie) {
        // Allocate a new, empty subtrie.
        NewSubtrie = IndexHeader->createSubtrie(
            [this](uint64_t Size) {
              return this->Index->requestFileSize(Size);
            },
            S->StartBit + S->NumBits, IndexGen.getNumBits());
        NewS = IndexHeader->getSubtrie(*NewSubtrie);
      } else {
        // Reinitialize the subtrie that's still around from an earlier race.
        NewS = IndexHeader->getSubtrie(*NewSubtrie);
        NewS->reinitialize(S->StartBit + S->NumBits, IndexGen.getNumBits());
      }

      auto &NewSlot = NewS->getSlots()[NewIndexForExistingContent];
      NewSlot.store(Existing.getRawOffset());

      assert(NewSubtrie->asSubtrie() < IndexHeader->EndOffset.load());
      Optional<OnDiskOffset> Race =
          S->try_replace(Index, Existing, *NewSubtrie);
      if (Race) {
        if (Race->isData())
          assert(Race->asData() == Existing.asData());
        assert(Race->isSubtrie());
        // Use the other subtrie from the competing thread or process, and wipe
        // out the new slot so this subtrie can potentially be reused if we
        // need to make another.
        S = IndexHeader->getSubtrie(*Race);
        NewSlot.store(OnDiskOffset().getRawOffset(), std::memory_order_relaxed);
      } else {
        // Success!
        S = NewS;
        NewSubtrie = None;
      }

      Index = NextIndex;

      // Found the difference.
      if (NextIndex != NewIndexForExistingContent)
        break;
    }
  }
}

OnDiskHashMappedTrie::MappedFileInfo::MappedFileInfo(StringRef Path,
                                                     sys::fs::file_t FD,
                                                     size_t MapSize,
                                                     std::error_code &EC)
    : Path(Path.str()), FD(FD),
      Map(FD, sys::fs::mapped_file_region::readwrite, MapSize, 0, EC) {}

OnDiskIndexHeader::OnDiskIndexHeader(uint64_t NumRootBits,
                                     uint64_t NumSubtrieBits,
                                     uint64_t NumHashBits)
    : OnDiskHeaderBase(12345678ull,
                       sizeof(OnDiskIndexHeader) +
                           OnDiskSubtrie::getSlotsSize(NumRootBits)),
      NumRootBits(NumRootBits), NumSubtrieBits(NumSubtrieBits),
      NumHashBits(NumHashBits), Root(0, NumRootBits) {}

OnDiskDataHeader::OnDiskDataHeader()
    : OnDiskHeaderBase(87654321ull, sizeof(OnDiskDataHeader)) {}

/// Open / resize / map in a file on-disk.
///
/// FIXME: This isn't portable. Windows will resize the file to match the map
/// size, which means immediately creating a very large file. Instead, maybe
/// we can increase the mapped region size on windows after creation, or
/// default to a more reasonable size.
///
/// The effect we want is:
///
/// 1. Reserve virtual memory large enough for the max file size (1GB).
/// 2. If it doesn't exist, give the file an initial smaller size (1MB).
/// 3. Map the file into memory.
/// 4. Assign the file to the reserved virtual memory.
/// 5. Increase the file size and update the mapping when necessary.
///
/// Here's the current implementation for Unix:
///
/// 1. [Automatic as part of 3.]
/// 2. Call ::ftruncate to 1MB (sys::fs::resize_file).
/// 3. Call ::mmap with 1GB (sys::fs::mapped_file_region).
/// 4. [Automatic as part of 3.]
/// 5. Call ::ftruncate with the new size.
///
/// On Windows, I *think* this can be implemented with:
///
/// 1. Call VirtualAlloc2 to reserve 1GB of virtual memory.
/// 2. [Automatic as part of 3.]
/// 3. Call CreateFileMapping to with 1MB, or existing size.
/// 4. Call MapViewOfFileN to place it in the reserved memory.
/// 5. Repeat step (3) with the new size and step (4).
Error OnDiskHashMappedTrie::MappedFileInfo::open(
    Optional<MappedFileInfo> &MFI, StringRef Path, size_t InitialSize,
    size_t MaxSize, function_ref<void(char *)> NewFileConstructor) {
  Expected<sys::fs::file_t> FD = sys::fs::openNativeFileForReadWrite(
      Path, sys::fs::CD_OpenAlways, sys::fs::OF_None);
  if (!FD)
    return FD.takeError();

  {
    std::error_code EC;
    MFI.emplace(Path, *FD, MaxSize, EC);
    if (EC)
      return createFileError(Path, EC);
  }

  // Lock the file so we can initialize it.
  if (std::error_code EC = sys::fs::lockFile(*FD))
    return createFileError(Path, EC);
  auto Unlock = make_scope_exit([&]() { sys::fs::unlockFile(*FD); });

  sys::fs::file_status Status;
  if (std::error_code EC = sys::fs::status(*FD, Status))
    return errorCodeToError(EC);
  if (Status.getSize() >= InitialSize) {
    MFI->OnDiskSize = Status.getSize();
    return Error::success();
  }

  if (std::error_code EC = sys::fs::resize_file(*FD, InitialSize))
    return errorCodeToError(EC);
  MFI->OnDiskSize = InitialSize;
  NewFileConstructor(MFI->Map.data());
  return Error::success();
}

Expected<std::shared_ptr<OnDiskHashMappedTrie>>
OnDiskHashMappedTrie::create(const Twine &PathTwine, size_t NumHashBits,
                             uint64_t MaxMapSize,
                             Optional<size_t> InitialNumRootBits,
                             Optional<size_t> InitialNumSubtrieBits) {
  struct TrieMapNode {
    std::mutex Mutex;
    std::weak_ptr<OnDiskHashMappedTrie> Trie;
  };
  static std::mutex Mutex;
  static StringMap<TrieMapNode> Tries;

  static constexpr size_t DefaultNumRootBits = 10;
  static constexpr size_t DefaultNumSubtrieBits = 6;

  if (!InitialNumRootBits)
    InitialNumRootBits = DefaultNumRootBits;
  if (!InitialNumSubtrieBits)
    InitialNumSubtrieBits = DefaultNumSubtrieBits;

  SmallString<128> Path, DataPath, IndexPath;
  PathTwine.toVector(Path);
  DataPath = Path;
  IndexPath = Path;
  sys::path::append(DataPath, "data");
  sys::path::append(IndexPath, "index");

  TrieMapNode *MapNode;
  {
    std::lock_guard<std::mutex> Lock(Mutex);
    MapNode = &Tries[Path];
  }

  if (std::shared_ptr<OnDiskHashMappedTrie> Trie = MapNode->Trie.lock())
    return Trie;

  // Construct a new trie.
  std::lock_guard<std::mutex> Lock(MapNode->Mutex);

  if (std::error_code EC = sys::fs::create_directory(Path))
    return errorCodeToError(EC);

  Expected<sys::fs::file_t> DataFD = sys::fs::openNativeFileForReadWrite(
      DataPath, sys::fs::CD_OpenAlways, sys::fs::OF_None);
  if (!DataFD)
    return DataFD.takeError();
  Expected<sys::fs::file_t> IndexFD = sys::fs::openNativeFileForReadWrite(
      IndexPath, sys::fs::CD_OpenAlways, sys::fs::OF_None);
  if (!IndexFD)
    return IndexFD.takeError();

  static_assert(sizeof(size_t) == sizeof(uint64_t), "64-bit only");

  static constexpr uint64_t GB = 1024ull * 1024ull * 1024ull;
  static constexpr uint64_t MB = 1024ull * 1024ull;

  // Max out the index at 4GB. Take the limit directly for data.
  const uint64_t MaxIndexSize = std::min(4 * GB, MaxMapSize);
  const uint64_t MaxDataSize = MaxMapSize;

  std::shared_ptr<OnDiskHashMappedTrie> Trie(
      new OnDiskHashMappedTrie(), [](OnDiskHashMappedTrie *T) { delete T; });

  // Open / create / initialize files on disk.
  if (Error E = MappedFileInfo::open(
          Trie->Index, IndexPath,
          /*InitialSize=*/MB, MaxIndexSize, [&](char *FileData) {
            new (FileData) OnDiskIndexHeader(
                *InitialNumRootBits, *InitialNumSubtrieBits, NumHashBits);
          }))
    return std::move(E);
  if (Error E = MappedFileInfo::open(
          Trie->Data, DataPath,
          /*InitialSize=*/MB, MaxDataSize,
          [&](char *FileData) { new (FileData) OnDiskDataHeader(); }))
    return std::move(E);

  // Success.
  MapNode->Trie = Trie;
  return Trie;
}

OnDiskHashMappedTrie::~OnDiskHashMappedTrie() = default;

OnDiskHashMappedTrie::MappedFileInfo::~MappedFileInfo() {
  if (FD)
    sys::fs::closeFile(FD);
}
