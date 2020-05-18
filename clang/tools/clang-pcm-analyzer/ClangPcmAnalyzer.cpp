//===- ClangPcmAnalyzer.cpp - Implementation of clang-pcm-analyzer --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "/Users/dang/VersionControlledDocuments/llvm-project/clang/lib/Serialization/ASTReaderInternals.h"
#include "clang/Basic/Version.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Serialization/ASTBitCodes.h"
#include "clang/Serialization/ASTRecordReader.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/LLVMBitCodes.h"
#include "llvm/Bitstream/BitCodes.h"
#include "llvm/Bitstream/BitstreamReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

using namespace llvm;
using namespace clang;
using namespace clang::serialization;

namespace {

cl::OptionCategory ClangPcmAnalyzerCategory("Tool options");

cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input PCM>"),
                                   cl::init("-"),
                                   llvm::cl::cat(ClangPcmAnalyzerCategory));

cl::opt<bool> ShowBinaryBlobs("show-binary-blobs",
                              cl::desc("Print binary blobs using hex escapes"),
                              cl::init(false),
                              llvm::cl::cat(ClangPcmAnalyzerCategory));

cl::opt<bool> ShowUnknownRecords("show-unknown-records",
                                 cl::desc("Print unknown record types"),
                                 cl::init(false),
                                 llvm::cl::cat(ClangPcmAnalyzerCategory));

cl::opt<bool> DontValidateVCS("no-validate-vcs",
                              cl::desc("Validate version control information"),
                              cl::init(false),
                              llvm::cl::cat(ClangPcmAnalyzerCategory));

template <typename... Ts>
Error createError(StringRef Message, const Ts &... Vals) {
  return createStringError(std::errc::illegal_byte_sequence, Message.data(),
                           Vals...);
}

Optional<const char *> getCodeName(unsigned CodeID, unsigned BlockID,
                                   const BitstreamBlockInfo &BlockInfo) {
  // Standard blocks for all bitcode files.
  if (BlockID < bitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == bitc::BLOCKINFO_BLOCK_ID) {
      switch (CodeID) {
      default:
        return None;
      case bitc::BLOCKINFO_CODE_SETBID:
        return "SETBID";
      case bitc::BLOCKINFO_CODE_BLOCKNAME:
        return "BLOCKNAME";
      case bitc::BLOCKINFO_CODE_SETRECORDNAME:
        return "SETRECORDNAME";
      }
    }
    return None;
  }

  if (const BitstreamBlockInfo::BlockInfo *Info =
          BlockInfo.getBlockInfo(BlockID)) {
    auto RNIt = std::find_if(Info->RecordNames.begin(), Info->RecordNames.end(),
                             [=](const std::pair<unsigned, std::string> &El) {
                               return El.first == CodeID;
                             });
    if (RNIt != Info->RecordNames.end())
      return RNIt->second.c_str();
  }

  return None;
}

bool canDecodeBlob(unsigned Code, unsigned BlockID) {
  return BlockID == bitc::METADATA_BLOCK_ID && Code == bitc::METADATA_STRINGS;
}

Error decodeMetadataStringsBlob(StringRef Indent, ArrayRef<uint64_t> Record,
                                StringRef Blob, raw_ostream &OS) {
  if (Blob.empty())
    return createError("Cannot decode empty blob.");

  if (Record.size() != 2)
    return createError(
        "Decoding metadata strings blob needs two record entries.");

  unsigned NumStrings = Record[0];
  unsigned StringsOffset = Record[1];
  OS << " num-strings = " << NumStrings << " {\n";

  StringRef Lengths = Blob.slice(0, StringsOffset);
  SimpleBitstreamCursor R(Lengths);
  StringRef Strings = Blob.drop_front(StringsOffset);
  do {
    if (R.AtEndOfStream())
      return createError("bad length");

    Expected<uint32_t> MaybeSize = R.ReadVBR(6);
    if (!MaybeSize)
      return MaybeSize.takeError();
    uint32_t Size = MaybeSize.get();
    if (Strings.size() < Size)
      return createError("truncated chars");

    OS << Indent << "    '";
    OS.write_escaped(Strings.slice(0, Size), /*hex=*/true);
    OS << "'\n";
    Strings = Strings.drop_front(Size);
  } while (--NumStrings);

  OS << Indent << "  }";
  return Error::success();
}

struct PCMDumpOptions {
  raw_ostream &OS;
  bool ShowUnknownRecords = false;
  bool ShowBinaryBlobs = false;

  void increaseIndentation() { CurrentIndentation += 2; }
  void decreaseIndentation() { CurrentIndentation -= 2; }
  raw_ostream &output() const { return OS.indent(CurrentIndentation); }

  PCMDumpOptions(raw_ostream &OS) : OS(OS) {}

private:
  unsigned CurrentIndentation = 0;
};

class PCMAnalyzer;
class HeaderFileInfoTrait {
  const char *FrameworkStrings;

public:
  struct key_type {
    off_t Size;
    time_t ModTime;
    StringRef Filename;
  };

  struct data_type {
    key_type Key;
    unsigned IsImport : 1;
    unsigned IsPragmaOnce : 1;
    unsigned DirInfo : 3;
    unsigned IndexHeaderMapHeader : 1;
    unsigned NumIncludes;
    unsigned ControllingMacroID;
    StringRef Framework;
    std::vector<std::pair<uint32_t, ModuleMap::ModuleHeaderRole>>
        ImportedHeaders;
  };

  using internal_key_type = key_type;
  using external_key_type = key_type;
  using internal_key_ref = const key_type &;
  using external_key_ref = const key_type &;
  using hash_value_type = unsigned;
  using offset_type = unsigned;

  HeaderFileInfoTrait(const char *FrameworkStrings)
      : FrameworkStrings(FrameworkStrings) {}

  static bool EqualKey(internal_key_ref A, internal_key_ref B) {
    if (A.Size != B.Size || (A.ModTime && B.ModTime && A.ModTime != B.ModTime))
      return false;

    if (A.Filename == B.Filename)
      return true;

    return false;
  }

  static hash_value_type ComputeHash(internal_key_ref IKey) {
    return llvm::hash_combine(IKey.Size, IKey.ModTime);
  }

  static internal_key_ref GetInternalKey(external_key_ref EKey) { return EKey; }

  static std::pair<offset_type, offset_type>
  ReadKeyDataLength(const unsigned char *&Buffer) {
    using namespace llvm::support;

    offset_type KeyLen =
        (offset_type)endian::readNext<uint16_t, little, unaligned>(Buffer);
    offset_type DataLen = (offset_type)*Buffer++;
    return std::make_pair(KeyLen, DataLen);
  }

  static internal_key_type ReadKey(const unsigned char *Buffer,
                                   offset_type KeyLen) {
    using namespace llvm::support;

    internal_key_type IKey;
    IKey.Size = off_t(endian::readNext<uint64_t, little, unaligned>(Buffer));
    IKey.ModTime =
        time_t(endian::readNext<uint64_t, little, unaligned>(Buffer));
    IKey.Filename = (const char *)Buffer;
    return IKey;
  }

  data_type ReadData(internal_key_ref Key, const unsigned char *Buffer,
                     unsigned DataLen) {
    using namespace llvm::support;

    const unsigned char *End = Buffer + DataLen;
    data_type HFIL;
    HFIL.Key = Key;
    unsigned Flags = *Buffer++;
    HFIL.IsImport |= (Flags >> 5) & 0x01;
    HFIL.IsPragmaOnce |= (Flags >> 4) & 0x01;
    HFIL.DirInfo = (Flags >> 1) & 0x07;
    HFIL.IndexHeaderMapHeader = Flags & 0x01;
    HFIL.NumIncludes = endian::readNext<uint16_t, little, unaligned>(Buffer);
    HFIL.ControllingMacroID =
        endian::readNext<uint32_t, little, unaligned>(Buffer);
    if (unsigned FrameworkOffset =
            endian::readNext<uint32_t, little, unaligned>(Buffer)) {
      // The framework offset is 1 greater than the actual offset,
      // since 0 is used as an indicator for "no framework name".
      StringRef FrameworkName(FrameworkStrings + FrameworkOffset - 1);
      HFIL.Framework = FrameworkName;
    }

    assert((End - Buffer) % 4 == 0 &&
           "Wrong data length in HeaderFileInfo deserialization");
    while (Buffer != End) {
      uint32_t LocalSMID =
          endian::readNext<uint32_t, little, unaligned>(Buffer);
      auto HeaderRole =
          static_cast<ModuleMap::ModuleHeaderRole>(LocalSMID & 0x11);
      LocalSMID >>= 2;

      HFIL.ImportedHeaders.emplace_back(LocalSMID, HeaderRole);
    }

    return HFIL;
  }
};

using HeaderFileInfoLookupTable =
    llvm::OnDiskIterableChainedHashTable<HeaderFileInfoTrait>;

class PCMAnalyzer {
public:
  bool HasTimestamps = true;

private:
  BitstreamCursor Stream;
  BitstreamBlockInfo BlockInfo;

  using RecordData = SmallVector<uint64_t, 64>;
  using RecordDataImpl = SmallVectorImpl<uint64_t>;

public:
  PCMAnalyzer(StringRef Buffer) : Stream(Buffer) {}

  Error analyze(PCMDumpOptions O) {
    if (auto Err = doesntStartWithASTFileMagic())
      return Err;

    {
      SavedStreamPosition SavedPosition(Stream);
      if (Error Err = skipCursorToBlock(bitc::BLOCKINFO_BLOCK_ID))
        return Err;
      auto BlockInfoOrErr = Stream.ReadBlockInfoBlock(true);
      if (Error Err = BlockInfoOrErr.takeError())
        return Err;
      auto MaybeBlockInfo = BlockInfoOrErr.get();
      if (!MaybeBlockInfo.hasValue())
        return createError("could not get block info block");
      BlockInfo = *MaybeBlockInfo;
      Stream.setBlockInfo(&BlockInfo);
    }

    {
      SavedStreamPosition SavedPosition(Stream);
      if (Error Err = skipCursorToBlock(CONTROL_BLOCK_ID))
        return Err;

      if (Error Err = readControlBlock(O))
        return Err;
      }

    {
      SavedStreamPosition SavedPosition(Stream);
      if (Error Err = skipCursorToBlock(UNHASHED_CONTROL_BLOCK_ID))
        return Err;

      if (Error Err = readUnhashedControlBlock(O))
        return Err;
    }

    {
      SavedStreamPosition SavedPosition(Stream);
      if (Error Err = skipCursorToBlock(AST_BLOCK_ID))
        return Err;

      if (Error Err = readASTBlock(O))
        return Err;
    }
    return Error::success();
  }

private:
  Error doesntStartWithASTFileMagic() {
    if (!Stream.canSkipToPos(4))
      return createError("file too small to contain PCM file magic");
    for (unsigned C : {'C', 'P', 'C', 'H'})
      if (Expected<SimpleBitstreamCursor::word_t> Res = Stream.Read(8)) {
        if (Res.get() != C)
          return createError("file doesn't start with PCM file magic");
      } else
        return Res.takeError();
    return Error::success();
  }

  std::string readString(const RecordDataImpl &Record, unsigned &Idx) {
    unsigned Len = Record[Idx++];
    std::string Result(Record.data() + Idx, Record.data() + Idx + Len);
    Idx += Len;
    return Result;
  }

  Error outputUnknownRecord(const PCMDumpOptions &O, unsigned CodeID,
                            unsigned BlockID, const BitstreamEntry &Entry,
                            const RecordDataImpl &Record, StringRef Blob) {
    Optional<const char *> CodeName = getCodeName(CodeID, BlockID, BlockInfo);
    raw_ostream &LineOut = O.output()
                           << "<" << (CodeName ? *CodeName : "UnknownRecord");

    const BitCodeAbbrev *Abbrev = nullptr;
    if (Entry.ID != bitc::UNABBREV_RECORD) {
      Abbrev = Stream.getAbbrev(Entry.ID);
      LineOut << " abbrevid=" << Entry.ID;
    }

    for (unsigned I = 0, E = Record.size(); I != E; ++I) {
      LineOut << " op" << I << "=" << (int64_t)Record[I];
    }

    LineOut << ">";

    if (Abbrev) {
      for (unsigned I = 1, E = Abbrev->getNumOperandInfos(); I != E; ++I) {
        const BitCodeAbbrevOp &Op = Abbrev->getOperandInfo(I);
        if (!Op.isEncoding() || Op.getEncoding() != BitCodeAbbrevOp::Array)
          continue;
        if (I + 2 == E)
          return createError("Array op not second to last");
        std::string Str;
        bool ArrayIsPrintable = true;
        for (unsigned J = I - 1, JE = Record.size(); J != JE; ++J) {
          if (!isPrint(static_cast<unsigned char>(Record[J]))) {
            ArrayIsPrintable = false;
            break;
          }
          Str += (char)Record[J];
        }
        if (ArrayIsPrintable)
          LineOut << " record string = '" << Str << "'";
        break;
      }
    }

    if (Blob.data()) {
      if (canDecodeBlob(CodeID, BlockID)) {
        if (Error E = decodeMetadataStringsBlob(" '", Record, Blob, LineOut))
          return E;
      } else {
        LineOut << " blob data = ";
        if (O.ShowBinaryBlobs) {
          LineOut << "'";
          LineOut.write_escaped(Blob, /*hex=*/true) << "'";
        } else {
          bool BlobIsPrintable = true;
          for (unsigned i = 0, e = Blob.size(); i != e; ++i)
            if (!isPrint(static_cast<unsigned char>(Blob[i]))) {
              BlobIsPrintable = false;
              break;
            }

          if (BlobIsPrintable)
            LineOut << "'" << Blob << "'";
          else
            LineOut << "unprintable, " << Blob.size() << " bytes.";
        }
      }
    }

    LineOut << "\n";
    return Error::success();
  }

  Error skipCursorToBlock(unsigned BlockID) {
    while (true) {
      Expected<BitstreamEntry> EntryOrErr = Stream.advance();
      if (Error E = EntryOrErr.takeError()) {
        return E;
      }

      BitstreamEntry Entry = EntryOrErr.get();

      switch (Entry.Kind) {
      case BitstreamEntry::Error:
      case BitstreamEntry::EndBlock:
        return createError("Could not find block with ID %s",
                           BlockInfo.getBlockInfo(BlockID)->Name.c_str());

      case BitstreamEntry::Record:
        // Ignore top-level records.
        if (Expected<unsigned> Skipped = Stream.skipRecord(Entry.ID))
          break;
        else
          return Skipped.takeError();

      case BitstreamEntry::SubBlock:
        if (Entry.ID == BlockID) {
          return Error::success();
        }

        if (Error Err = Stream.SkipBlock())
          return Err;
      }
    }
  }

  Error readASTBlock(PCMDumpOptions &O) {
    if (Error Err = Stream.EnterSubBlock(CONTROL_BLOCK_ID))
      return Err;

    O.output() << "<AST_BLOCK>"
               << "\n";
    O.increaseIndentation();
    RecordData Record;

    while (true) {
      Expected<BitstreamEntry> EntryOrErr = Stream.advance();
      if (Error Err = EntryOrErr.takeError())
        return Err;

      BitstreamEntry Entry = EntryOrErr.get();

      switch (Entry.Kind) {
      case BitstreamEntry::Error:
        return createError("malformed AST block record in PCM file");
      case BitstreamEntry::EndBlock:
        O.decreaseIndentation();
        O.output() << "</AST_BLOCK>\n";
        Stream.ReadBlockEnd();
        return Error::success();

      case BitstreamEntry::SubBlock:
        switch (Entry.ID) {
        default:
          if (Error Err = Stream.SkipBlock())
            return Err;
          break;
        }
        continue;

      case BitstreamEntry::Record:
        break;
      }

      Record.clear();
      StringRef Blob;

      Expected<unsigned> RecordTypeOrErr =
          Stream.readRecord(Entry.ID, Record, &Blob);
      if (auto Err = RecordTypeOrErr.takeError())
        return Err;

      unsigned RecordType = RecordTypeOrErr.get();

      switch (static_cast<ASTRecordTypes>(RecordType)) {
      case HEADER_SEARCH_TABLE: {
        const char *HeaderFileInfoTableData = Blob.data();
        if (Record[0]) {
          auto HFILT = HeaderFileInfoLookupTable::Create(
              (const unsigned char *)HeaderFileInfoTableData + Record[0],
              (const unsigned char *)HeaderFileInfoTableData + sizeof(uint32_t),
              (const unsigned char *)HeaderFileInfoTableData,
              HeaderFileInfoTrait(HeaderFileInfoTableData + Record[2]));

          O.output() << "HEADER_SEARCH_TABLE:\n";
          O.increaseIndentation();
          for (const auto &HFIL : HFILT->data()) {
            auto Key = HFIL.Key;
            raw_ostream &LineOut = O.output()
                                   << "(Filename: " << Key.Filename
                                   << ", Modtime: " << Key.ModTime
                                   << ", Size: " << Key.Size << "): ";
            LineOut << "(IsImport: " << HFIL.IsImport << ", IsPragmaOnce"
                    << HFIL.IsPragmaOnce << ", DirInfo: " << HFIL.DirInfo
                    << ", IndexHeaderMapheader: " << HFIL.IndexHeaderMapHeader
                    << ", NumIncludes: " << HFIL.NumIncludes
                    << ", ControllingMacroID: " << HFIL.ControllingMacroID
                    << ", Framework: " << HFIL.Framework
                    << ", ImportedHeaders:";
            for (const auto &ImportedHeader : HFIL.ImportedHeaders) {
              LineOut << " ("
                      << "ID: " << ImportedHeader.first << ", Role: ";
              switch (static_cast<ModuleMap::ModuleHeaderRole>(
                  ImportedHeader.second)) {
              case ModuleMap::NormalHeader:
                LineOut << "NormalHeader";
                break;
              case ModuleMap::PrivateHeader:
                LineOut << "PrivateHeader";
                break;
              case ModuleMap::TextualHeader:
                LineOut << "TextualHeader";
                break;
              }
            LineOut << ")";
            }
            LineOut << ")\n";
          }
          O.decreaseIndentation();
        }
        break;
      }

      default: {
        if (O.ShowUnknownRecords)
          if (Error Err = outputUnknownRecord(O, RecordType, AST_BLOCK_ID,
                                              Entry, Record, Blob))
            return Err;
        break;
      }
      }
    }

    return Error::success();
  }

  Error readControlBlock(PCMDumpOptions &O) {
    if (Error Err = Stream.EnterSubBlock(CONTROL_BLOCK_ID))
      return Err;

    O.output() << "<CONTROL_BLOCK>"
               << "\n";
    O.increaseIndentation();
    RecordData Record;

    while (true) {
      Expected<BitstreamEntry> EntryOrErr = Stream.advance();
      if (Error Err = EntryOrErr.takeError())
        return Err;

      BitstreamEntry Entry = EntryOrErr.get();

      switch (Entry.Kind) {
      case BitstreamEntry::Error:
        return createError("malformed control block record in PCM file");
      case BitstreamEntry::EndBlock:
        O.decreaseIndentation();
        O.output() << "</CONTROL_BLOCK>\n";
        Stream.ReadBlockEnd();
        return Error::success();

      case BitstreamEntry::SubBlock:
        switch (Entry.ID) {
        default:
          if (Error Err = Stream.SkipBlock())
            return Err;
          break;
        }
        continue;

      case BitstreamEntry::Record:
        break;
      }

      Record.clear();
      StringRef Blob;

      Expected<unsigned> RecordTypeOrErr =
          Stream.readRecord(Entry.ID, Record, &Blob);
      if (auto Err = RecordTypeOrErr.takeError())
        return Err;

      unsigned RecordType = RecordTypeOrErr.get();

      switch (static_cast<ControlRecordTypes>(RecordType)) {
      case METADATA: {
        const std::string &CurBranch = getClangFullRepositoryVersion();
        StringRef ASTBranch = Blob;
        if (!DontValidateVCS && StringRef(CurBranch) != ASTBranch)
          return createError("Incompatible clang versions got %s expected % s ",
                             ASTBranch.data(), CurBranch.data());

        HasTimestamps = Record[5];
        O.output() << "BRANCH_NAME: " << ASTBranch << "\n";
        O.output() << "VERISON_MAJOR: " << Record[0] << "\n";
        O.output() << "VERISON_MINOR: " << Record[1] << "\n";
        O.output() << "CLANG_VERSION_MAJOR: " << Record[2] << "\n";
        O.output() << "CLANG_VERSION_MINOR: " << Record[3] << "\n";
        O.output() << "IS_RELOCATABLE: " << Record[4] << "\n";
        O.output() << "HAS_TIMESTAMPS: " << Record[5] << "\n";
        O.output() << "HAS_COMPILER_ERRORS: " << Record[7] << "\n";
        break;
      }

      case MODULE_NAME: {
        StringRef ModuleName = Blob;
        O.output() << "MODULE_NAME: " << ModuleName << "\n";
        break;
      }

      case MODULE_DIRECTORY: {
        O.output() << "MODULE_DIRECTORY: " << Blob << "\n";
        break;
      }

      case MODULE_MAP_FILE: {
        unsigned Idx = 0;
        std::string ModMapPath = readString(Record, Idx);
        unsigned NumAdditionalModMaps = 0;
        std::vector<std::string> AdditionalModMapsPaths;
        if (Idx < Record.size()) {
          NumAdditionalModMaps = Record[Idx++];
          AdditionalModMapsPaths.reserve(NumAdditionalModMaps);
          for (; NumAdditionalModMaps > 0; --NumAdditionalModMaps)
            AdditionalModMapsPaths.emplace_back(readString(Record, Idx));
        }
        raw_ostream &LineOut = O.output() << "MODULE_MAP_FILE: " << ModMapPath;
        if (AdditionalModMapsPaths.size() > 0)
          for (const auto &AdditionalModMap : AdditionalModMapsPaths)
            LineOut << "; " << AdditionalModMap;
        LineOut << "\n";
        break;
      }

      case ORIGINAL_FILE_ID: {
        O.output() << "ORIGINAL_FILE_ID: " << Record[0] << "\n";
        break;
      }

      case ORIGINAL_FILE: {
        O.output() << "ORIGINAL_FILE: " << Blob << "\n";
        break;
      }

      case ORIGINAL_PCH_DIR: {
        O.output() << "ORIGINAL_PCH_DIR: " << Blob << "\n";
        break;
      }

      default: {
        if (O.ShowUnknownRecords)
          if (Error Err = outputUnknownRecord(O, RecordType, CONTROL_BLOCK_ID,
                                              Entry, Record, Blob))
            return Err;
          break;
      }
      }
    }
  }

  Error readUnhashedControlBlock(PCMDumpOptions &O) { return Error::success(); }
};

Expected<std::unique_ptr<MemoryBuffer>> openPCMFile(StringRef Path) {
  auto MemBufOrErr = errorOrToExpected(MemoryBuffer::getFileOrSTDIN(Path));
  if (Error E = MemBufOrErr.takeError())
    return std::move(E);

  auto MemBuf = std::move(*MemBufOrErr);

  if (MemBuf->getBufferSize() & 3)
    return createStringError(
        std::errc::illegal_byte_sequence,
        "Bitcode streams should ne a multiple of 4 bytes in length");

  return std::move(MemBuf);
}

} // end anonymous namespace

int main(int argc, const char **argv) {
  InitLLVM X(argc, argv);
  llvm::cl::HideUnrelatedOptions(ClangPcmAnalyzerCategory);
  if (!cl::ParseCommandLineOptions(
          argc, argv, "clang-pcm-analyzer precompiled module analyzer\n"))
    return 1;

  ExitOnError ExitOnErr("clang-pcm-analyzer: ");

  auto MB = ExitOnErr(openPCMFile(InputFilename));
  PCMAnalyzer PCMA(MB->getBuffer());

  PCMDumpOptions Opts(outs());
  Opts.ShowBinaryBlobs = ShowBinaryBlobs;
  Opts.ShowUnknownRecords = ShowUnknownRecords;

  ExitOnErr(PCMA.analyze(Opts));
}
