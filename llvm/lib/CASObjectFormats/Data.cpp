//===- CASObjectFormats/Data.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CASObjectFormats/Data.h"
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/ExecutionEngine/JITLink/MemoryFlags.h"
#include "llvm/Support/EndianStream.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::data;

jitlink::MemProt data::decodeProtectionFlags(SectionProtectionFlags Perms) {
  return (Perms & Read ? jitlink::MemProt::Read : jitlink::MemProt::None) |
         (Perms & Write ? jitlink::MemProt::Write : jitlink::MemProt::None) |
         (Perms & Exec ? jitlink::MemProt::Exec : jitlink::MemProt::None);
}

SectionProtectionFlags data::encodeProtectionFlags(jitlink::MemProt Perms) {
  return SectionProtectionFlags(
      ((Perms & jitlink::MemProt::Read) != jitlink::MemProt::None ? Read : 0) |
      ((Perms & jitlink::MemProt::Write) != jitlink::MemProt::None ? Write
                                                                   : 0) |
      ((Perms & jitlink::MemProt::Exec) != jitlink::MemProt::None ? Exec : 0));
}

void FixupList::encode(ArrayRef<Fixup> Fixups, SmallVectorImpl<char> &Data) {
  // FIXME: Kinds should be numbered in a stable way, not just rely on
  // Edge::Kind.
  for (auto &F : Fixups) {
    Data.push_back(static_cast<unsigned char>(F.Kind));
    encoding::writeVBR8(uint64_t(F.Offset), Data);
  }
}

void FixupList::iterator::decode(bool IsInit) {
  if (Data.empty()) {
    assert((IsInit || F) && "past the end");
    F.reset();
    return;
  }

  unsigned char Kind = Data[0];
  Data = Data.drop_front();

  uint64_t Offset = 0;
  bool ConsumeFailed = errorToBool(encoding::consumeVBR8(Data, Offset));
  assert(!ConsumeFailed && "Cannot decode vbr8");
  (void)ConsumeFailed;

  F.emplace();
  F->Kind = Kind;
  F->Offset = Offset;
}

void BlockData::encode(uint64_t Size, uint64_t Alignment,
                       uint64_t AlignmentOffset, Optional<StringRef> Content,
                       ArrayRef<Fixup> Fixups, SmallVectorImpl<char> &Data) {
  assert(!Content || Size == Content->size() && "Mismatched content size");
  assert(Alignment && "Expected non-zero alignment");
  assert(isPowerOf2_64(Alignment) && "Expected alignment to be a power of 2");
  assert(AlignmentOffset < Alignment &&
         "Expected alignment offset to be less than alignment");

  // Value of 63 means alignment of 2^62, which is really big. We can steal some
  // bits.
  const unsigned EncodedAlignment = llvm::encode(Align(Alignment));
  assert(EncodedAlignment > 0 && "Expected alignment to be non-zero");
  assert(EncodedAlignment < (1 << 6) &&
         "Expected alignment to leave room for 2 bits");

  const bool HasAlignmentOffset = AlignmentOffset;
  const bool IsZeroFill = !Content;
  Data.push_back(EncodedAlignment |
                 (unsigned(HasAlignmentOffset) << HasAlignmentOffsetBit) |
                 (unsigned(IsZeroFill) << IsZeroFillBit));

  encoding::writeVBR8(Size, Data);
  if (Content)
    Data.append(Content->begin(), Content->end());
  FixupList::encode(Fixups, Data);

  if (!HasAlignmentOffset)
    return;

  raw_svector_ostream OS(Data);
  support::endian::Writer EW(OS, support::endianness::little);

  const size_t NumBytes = getNumAlignmentOffsetBytes(Alignment);
  switch (NumBytes) {
  default:
    llvm_unreachable("invalid alignment?");
  case 1:
    EW.write(uint8_t(AlignmentOffset));
    break;
  case 2:
    EW.write(uint16_t(AlignmentOffset));
    break;
  case 4:
    EW.write(uint32_t(AlignmentOffset));
    break;
  case 8:
    EW.write(uint64_t(AlignmentOffset));
    break;
  }
}

Error BlockData::decode(uint64_t &Size, uint64_t &Alignment,
                        uint64_t &AlignmentOffset, Optional<StringRef> &Content,
                        FixupList &Fixups) const {
  // Reset everything to start.
  Size = Alignment = AlignmentOffset = 0;
  Content = None;
  Fixups = FixupList("");

  // First byte.
  Alignment = getAlignment();
  const bool IsZeroFill = isZeroFill();
  const bool HasAlignmentOffset = hasAlignmentOffset();

  // Remaining.
  StringRef Remaining = Data.drop_front(AfterHeader);
  if (Error E = consumeSize(Remaining, Size))
    return E;
  if (!IsZeroFill)
    if (Error E = consumeContent(Remaining, Size, Content))
      return E;
  if (HasAlignmentOffset) {
    AlignmentOffset = decodeAlignmentOffset();
    Remaining = Remaining.drop_back(getNumAlignmentOffsetBytes(Alignment));
  }
  Fixups = FixupList(Remaining);
  return Error::success();
}

Error BlockData::consumeSize(StringRef &Remaining, uint64_t &Size) {
  return encoding::consumeVBR8(Remaining, Size);
}

uint64_t BlockData::consumeSizeFatal(StringRef &Remaining) {
  uint64_t Size;
  if (Error E = encoding::consumeVBR8(Remaining, Size))
    report_fatal_error(std::move(E));
  return Size;
}

Error BlockData::consumeContent(StringRef &Remaining, uint64_t Size,
                                Optional<StringRef> &Content) {
  if (Remaining.size() < Size)
    return createStringError(inconvertibleErrorCode(), "corrupt block data");
  Content = Remaining.take_front(Size);
  Remaining = Remaining.drop_front(Size);
  return Error::success();
}

Optional<StringRef> BlockData::getContent() const {
  if (isZeroFill())
    return None;

  StringRef Remaining = Data.drop_front(AfterHeader);
  uint64_t Size = consumeSizeFatal(Remaining);
  Optional<StringRef> Content;
  if (Error E = consumeContent(Remaining, Size, Content))
    report_fatal_error(std::move(E));
  return Content;
}

FixupList BlockData::getFixups() const {
  StringRef Remaining = Data.drop_front(AfterHeader);
  uint64_t Size = consumeSizeFatal(Remaining);
  if (!isZeroFill()) {
    assert(Remaining.size() >= Size && "Expected content");
    Remaining = Remaining.drop_front(Size);
  }
  if (hasAlignmentOffset()) {
    size_t NumBytes = getNumAlignmentOffsetBytes(getAlignment());
    assert(Remaining.size() >= NumBytes && "Expected content");
    Remaining = Remaining.drop_back(NumBytes);
  }
  return FixupList(Remaining);
}

uint64_t BlockData::decodeAlignmentOffset() const {
  assert(hasAlignmentOffset() && "Expected to have an alignment offset");

  using namespace llvm::support;
  const size_t NumBytes = getNumAlignmentOffsetBytes(getAlignment());
  assert(NumBytes < Data.size() && "Alignment offset too big?");
  const char *Start = Data.end() - NumBytes;
  switch (NumBytes) {
  default:
    llvm_unreachable("invalid alignment?");
  case 1:
    return endian::read<uint8_t, endianness::little, unaligned>(Start);
  case 2:
    return endian::read<uint16_t, endianness::little, unaligned>(Start);
  case 4:
    return endian::read<uint32_t, endianness::little, unaligned>(Start);
  case 8:
    return endian::read<uint64_t, endianness::little, unaligned>(Start);
  }
}

void TargetInfoList::encode(ArrayRef<TargetInfo> TIs,
                            SmallVectorImpl<char> &Data) {
  for (const TargetInfo &TI : TIs) {
    assert(TI.Index < TIs.size() && "More targets than edges?");
    encoding::writeVBR8(int64_t(TI.Addend), Data);
    encoding::writeVBR8(uint32_t(TI.Index), Data);
  }
}

void TargetInfoList::iterator::decode(bool IsInit) {
  if (Data.empty()) {
    assert((IsInit || TI) && "past the end");
    TI.reset();
    return;
  }

  int64_t Addend = 0;
  bool ConsumeFailed = errorToBool(encoding::consumeVBR8(Data, Addend));
  assert(!ConsumeFailed && "Cannot decode addend");
  (void)ConsumeFailed;

  uint32_t Index = 0;
  ConsumeFailed = errorToBool(encoding::consumeVBR8(Data, Index));
  assert(!ConsumeFailed && "Cannot decode index");
  (void)ConsumeFailed;

  TI.emplace();
  TI->Addend = Addend;
  TI->Index = Index;
}
