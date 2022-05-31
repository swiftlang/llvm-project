//===-------- JITLink_DWARFRecordSectionSplitter.cpp - JITLink-------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ExecutionEngine/JITLink/DWARFRecordSectionSplitter.h"
#include "llvm/Support/BinaryStreamReader.h"

#define DEBUG_TYPE "jitlink"

namespace llvm {
namespace jitlink {

DWARFRecordSectionSplitter::DWARFRecordSectionSplitter(StringRef SectionName)
    : SectionName(SectionName) {}

Error DWARFRecordSectionSplitter::operator()(
    LinkGraph &G, std::vector<uint64_t> *AbbrevOffsets) {
  auto *Section = G.findSectionByName(SectionName);

  if (!Section) {
    LLVM_DEBUG({
      dbgs() << "DWARFRecordSectionSplitter: No " << SectionName
             << " section. Nothing to do\n";
    });
    return Error::success();
  }

  LLVM_DEBUG({
    dbgs() << "DWARFRecordSectionSplitter: Processing " << SectionName
           << "...\n";
  });

  std::vector<Block *> Blocks;
  for (auto *B : Section->blocks())
    Blocks.push_back(B);

  for (auto *B : Blocks) {
    auto &Block = *B;
    if (AbbrevOffsets && !AbbrevOffsets->size()) {
      if (auto Err = processDebugInfoBlock(G, Block, AbbrevOffsets))
        return Err;
    } else if (AbbrevOffsets && AbbrevOffsets->size()) {
      if (auto Err = processDebugAbbrevBlock(G, Block, AbbrevOffsets))
        return Err;
    } else {
      if (auto Err = processBlock(G, Block))
        return Err;
    }
  }

  return Error::success();
}

Error DWARFRecordSectionSplitter::processBlock(LinkGraph &G, Block &B) {
  LLVM_DEBUG(dbgs() << "  Processing block at " << B.getAddress() << "\n");

  // Section should not contain zero-fill blocks.
  if (B.isZeroFill())
    return make_error<JITLinkError>("Unexpected zero-fill block in " +
                                    SectionName + " section");

  if (B.getSize() == 0) {
    LLVM_DEBUG(dbgs() << "    Block is empty. Skipping.\n");
    return Error::success();
  }

  BinaryStreamReader BlockReader(
      StringRef(B.getContent().data(), B.getContent().size()),
      G.getEndianness());

  while (true) {
    uint64_t RecordStartOffset = BlockReader.getOffset();

    LLVM_DEBUG({
      dbgs() << "    Processing CFI record at "
             << formatv("{0:x16}", B.getAddress()) << "\n";
    });

    uint32_t Length;
    if (auto Err = BlockReader.readInteger(Length))
      return Err;
    if (Length != 0xffffffff) {
      if (auto Err = BlockReader.skip(Length))
        return Err;
    } else {
      uint64_t ExtendedLength;
      if (auto Err = BlockReader.readInteger(ExtendedLength))
        return Err;
      if (auto Err = BlockReader.skip(ExtendedLength))
        return Err;
    }

    // If this was the last block then there's nothing to split
    if (BlockReader.empty()) {
      LLVM_DEBUG(dbgs() << "      Extracted " << B << "\n");
      return Error::success();
    }

    uint64_t BlockSize = BlockReader.getOffset() - RecordStartOffset;
    auto &NewBlock = G.splitBlock(B, BlockSize);
    (void)NewBlock;
    LLVM_DEBUG(dbgs() << "      Extracted " << NewBlock << "\n");
  }
}

Error DWARFRecordSectionSplitter::processDebugInfoBlock(
    LinkGraph &G, Block &B, std::vector<uint64_t> *AbbrevOffsets) {
  LLVM_DEBUG(dbgs() << "  Processing block at " << B.getAddress() << "\n");

  // Section should not contain zero-fill blocks.
  if (B.isZeroFill())
    return make_error<JITLinkError>("Unexpected zero-fill block in " +
                                    SectionName + " section");

  if (B.getSize() == 0) {
    LLVM_DEBUG(dbgs() << "    Block is empty. Skipping.\n");
    return Error::success();
  }

  BinaryStreamReader BlockReader(
      StringRef(B.getContent().data(), B.getContent().size()),
      G.getEndianness());

  while (true) {
    uint64_t RecordStartOffset = BlockReader.getOffset();

    LLVM_DEBUG({
      dbgs() << "    Processing DWARF record at "
             << formatv("{0:x16}", B.getAddress()) << "\n";
    });

    uint32_t Length;
    if (auto Err = BlockReader.readInteger(Length))
      return Err;
    // If the block uses a 64-bit format, it will have the first 4 bytes be
    // 0xffffffff followed by an 8-byte length, otherwise, the first 4 bytes
    // will be the length, that is what we are checking for here
    if (Length != 0xffffffff) {
      uint32_t Offset;
      // Skip the DWARF version.
      if (auto Err = BlockReader.skip(2))
        return Err;
      // Read the offset into the abbreviation section and record it.
      if (auto Err = BlockReader.readInteger(Offset))
        return Err;
      AbbrevOffsets->push_back(Offset);
      // Skip the length of the block minus 6 bytes for the DWARF version and
      // abbreviation offset.
      if (auto Err = BlockReader.skip(Length - 6))
        return Err;
    } else {
      uint64_t ExtendedLength;
      if (auto Err = BlockReader.readInteger(ExtendedLength))
        return Err;
      uint64_t Offset;
      // Skip the DWARF version, unit type, and address size.
      if (auto Err = BlockReader.skip(4))
        return Err;
      // Read the offset into the abbreviation section and record it.
      if (auto Err = BlockReader.readInteger(Offset))
        return Err;
      AbbrevOffsets->push_back(Offset);
      // Skip the length of the block minus 10 bytes for the DWARF version, unit
      // type, and address size, and abbreviation offset.
      if (auto Err = BlockReader.skip(ExtendedLength - 12))
        return Err;
    }

    // If this was the last block then there's nothing to split
    if (BlockReader.empty()) {
      LLVM_DEBUG(dbgs() << "      Extracted " << B << "\n");
      return Error::success();
    }

    uint64_t BlockSize = BlockReader.getOffset() - RecordStartOffset;
    auto &NewBlock = G.splitBlock(B, BlockSize);
    (void)NewBlock;
    LLVM_DEBUG(dbgs() << "      Extracted " << NewBlock << "\n");
  }
}

Error DWARFRecordSectionSplitter::processDebugAbbrevBlock(
    LinkGraph &G, Block &B, std::vector<uint64_t> *AbbrevOffsets) {
  LLVM_DEBUG(dbgs() << "  Processing block at " << B.getAddress() << "\n");

  // Section should not contain zero-fill blocks.
  if (B.isZeroFill())
    return make_error<JITLinkError>("Unexpected zero-fill block in " +
                                    SectionName + " section");

  if (B.getSize() == 0) {
    LLVM_DEBUG(dbgs() << "    Block is empty. Skipping.\n");
    return Error::success();
  }

  BinaryStreamReader BlockReader(
      StringRef(B.getContent().data(), B.getContent().size()),
      G.getEndianness());

  // Calculate the size of an Abbreviation contribution by subtracting the
  // Abbreviation Offset of the next block from the previous one, this is why
  // this loop skips the 0th iteration.
  for (unsigned i = 1; i < AbbrevOffsets->size(); i++) {
    uint64_t RecordStartOffset = BlockReader.getOffset();

    LLVM_DEBUG({
      dbgs() << "    Processing Abbrev Block record at "
             << formatv("{0:x16}", B.getAddress()) << "\n";
    });

    if (auto Err =
            BlockReader.skip((*AbbrevOffsets)[i] - (*AbbrevOffsets)[i - 1]))
      return Err;

    uint64_t BlockSize = BlockReader.getOffset() - RecordStartOffset;
    if (BlockSize) {
      auto &NewBlock = G.splitBlock(B, BlockSize);
      (void)NewBlock;
      LLVM_DEBUG(dbgs() << "      Extracted " << NewBlock << "\n");
    }
  }
  LLVM_DEBUG(dbgs() << "      Extracted " << B << "\n");
  return Error::success();
}

} // namespace jitlink
} // namespace llvm
