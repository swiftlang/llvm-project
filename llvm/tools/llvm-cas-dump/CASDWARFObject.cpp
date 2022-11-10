//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CASDWARFObject.h"
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/DebugInfo/DWARF/DWARFCompileUnit.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFDataExtractor.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugAbbrev.h"
#include "llvm/DebugInfo/DWARF/DWARFDebugLine.h"
#include "llvm/Object/MachO.h"
#include "llvm/Support/BinaryStreamReader.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/FormatVariadic.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::mccasformats::v1;

std::unordered_map<std::string, std::unordered_set<std::string>>
    CASDWARFObject::MapOfLinkageNames =
        std::unordered_map<std::string, std::unordered_set<std::string>>();

// template <> struct DenseMapInfo<std::string> {
//     static inline std::string getEmptyKey() {
//       return std::string(
//           reinterpret_cast<const char *>(~static_cast<uintptr_t>(0)), 0);
//     }

//     static inline std::string getTombstoneKey() {
//       return std::string(
//           reinterpret_cast<const char *>(~static_cast<uintptr_t>(1)), 0);
//     }

//     static unsigned getHashValue(std::string Val);

//     static bool isEqual(std::string LHS, std::string RHS) {
//       if (RHS.data() == getEmptyKey().data())
//         return LHS.data() == getEmptyKey().data();
//       if (RHS.data() == getTombstoneKey().data())
//         return LHS.data() == getTombstoneKey().data();
//       return LHS == RHS;
//     }
//   };

// unsigned DenseMapInfo<std::string>::getHashValue(std::string  Val) {
// assert(Val.data() != getEmptyKey().data() &&
//        "Cannot hash the empty key!");
// assert(Val.data() != getTombstoneKey().data() &&
//        "Cannot hash the tombstone key!");
// return (unsigned)(hash_value(Val));
// }

namespace {
/// Parse the MachO header to extract details such as endianness.
/// Unfortunately object::MachOObjectfile() doesn't support parsing
/// incomplete files.
struct MachOHeaderParser {
  bool Is64Bit = true;
  bool IsLittleEndian = true;

  /// Stolen from MachOObjectfile.
  template <typename T>
  Expected<T> getStructOrErr(StringRef Data, const char *P) {
    // Don't read before the beginning or past the end of the file
    if (P < Data.begin() || P + sizeof(T) > Data.end())
      return make_error<llvm::object::GenericBinaryError>(
          "Structure read out-of-range");

    T Cmd;
    memcpy(&Cmd, P, sizeof(T));
    if (IsLittleEndian != sys::IsLittleEndianHost)
      MachO::swapStruct(Cmd);
    return Cmd;
  }

  /// Parse an mc::header.
  Error parse(StringRef Data) {
    // MachO 64-bit header.
    const char *P = Data.data();
    auto Header64 = getStructOrErr<MachO::mach_header_64>(Data, P);
    P += sizeof(MachO::mach_header_64);
    if (!Header64)
      return Header64.takeError();
    if (Header64->magic == MachO::MH_MAGIC_64) {
      Is64Bit = true;
      IsLittleEndian = true;
    } else {
      return make_error<object::GenericBinaryError>("Unsupported MachO format");
    }
    return Error::success();
  }
};
} // namespace

Error CASDWARFObject::discoverDebugInfoSection(ObjectRef CASObj,
                                               raw_ostream &OS) {
  if (CASObj == Schema.getRootNodeTypeID())
    return Error::success();
  Expected<MCObjectProxy> MCObj = Schema.get(CASObj);
  if (!MCObj)
    return MCObj.takeError();
  return discoverDebugInfoSection(*MCObj, OS);
}

Error CASDWARFObject::discoverDebugInfoSection(MCObjectProxy MCObj,
                                               raw_ostream &OS) {
  if (auto DbgInfoSecRef = DebugInfoSectionRef::Cast(MCObj)) {
    raw_svector_ostream OS(DebugInfoSection);
    MCCASReader Reader(OS, Target, MCObj.getSchema());
    auto Written = DbgInfoSecRef->materialize(
        Reader, arrayRefFromStringRef<char>(getAbbrevSection()),
        getSecOffsetVals(),
        getMapOfStringOffsets(), &OS);
    if (!Written)
      return Written.takeError();

    StringRef DebugInfoStringRef = toStringRef(DebugInfoSection);
    BinaryStreamReader BinaryReader(DebugInfoStringRef,
                                    IsLittleEndian ? support::endianness::little
                                                   : support::endianness::big);
    while (!BinaryReader.empty()) {
      uint64_t SectionStartOffset = BinaryReader.getOffset();
      Expected<size_t> Size =
          mccasformats::v1::getSizeFromDwarfHeaderAndSkip(BinaryReader);
      if (!Size)
        return Size.takeError();
      StringRef CUData = DebugInfoStringRef.substr(
          SectionStartOffset, BinaryReader.getOffset() - SectionStartOffset);
      CUDataVec.push_back(CUData);
    }

    return Error::success();
  }
  return MCObj.forEachReference(
      [&](ObjectRef CASObj) { return discoverDebugInfoSection(CASObj, OS); });
}

Error CASDWARFObject::discoverDwarfSections(ObjectRef CASObj) {
  if (CASObj == Schema.getRootNodeTypeID())
    return Error::success();
  Expected<MCObjectProxy> MCObj = Schema.get(CASObj);
  if (!MCObj)
    return MCObj.takeError();
  return discoverDwarfSections(*MCObj);
}

Error CASDWARFObject::discoverDwarfSections(MCObjectProxy MCObj) {
  StringRef Data = MCObj.getData();
  if (auto MCAssRef = MCAssemblerRef::Cast(MCObj)) {
    StringRef Remaining = MCAssRef->getData();
    uint32_t NormalizedTripleSize;
    if (auto E = casobjectformats::encoding::consumeVBR8(Remaining,
                                                         NormalizedTripleSize))
      return E;
    auto TripleStr = Remaining.take_front(NormalizedTripleSize);
    Triple Target(TripleStr);
    this->Target = Target;
  }
  if (HeaderRef::Cast(MCObj)) {
    MachOHeaderParser P;
    if (Error Err = P.parse(MCObj.getData()))
      return Err;
    Is64Bit = P.Is64Bit;
    IsLittleEndian = P.IsLittleEndian;
  } else if (auto OffsetsRef = DebugAbbrevOffsetsRef::Cast(MCObj)) {
    DebugAbbrevOffsetsRefAdaptor Adaptor(*OffsetsRef);
    Expected<SmallVector<size_t>> DecodedOffsets = Adaptor.decodeOffsets();
    if (!DecodedOffsets)
      return DecodedOffsets.takeError();
    DebugAbbrevOffsets = std::move(*DecodedOffsets);
    // Reverse so that we can pop_back when assigning these to CURefs.
    std::reverse(DebugAbbrevOffsets.begin(), DebugAbbrevOffsets.end());
  } else if (auto LineRef = DebugLineRef::Cast(MCObj)) {
    SecOffsetVals.push_back(LineTableOffset);
    LineTableOffset += LineRef->getData().size();
  }
  if (DebugAbbrevRef::Cast(MCObj))
    append_range(DebugAbbrevSection, MCObj.getData());
  else if (DebugStrRef::Cast(MCObj)) {
    MapOfStringOffsets.try_emplace(MCObj.getRef(), DebugStringSection.size());
    DebugStringSection.append(Data.begin(), Data.end());
    DebugStringSection.push_back(0);
  } else if (DebugInfoCURef::Cast(MCObj))
    return Error::success();
  if (DebugAbbrevSectionRef::Cast(MCObj) || GroupRef::Cast(MCObj) ||
      SymbolTableRef::Cast(MCObj) || SectionRef::Cast(MCObj) ||
      DebugLineSectionRef::Cast(MCObj) || AtomRef::Cast(MCObj)) {
    auto Refs = MCObjectProxy::decodeReferences(MCObj, Data);
    if (!Refs)
      return Refs.takeError();
    for (auto Ref : *Refs) {
      if (Error E = discoverDwarfSections(Ref))
        return E;
    }
    return Error::success();
  }
  return MCObj.forEachReference(
      [this](ObjectRef CASObj) { return discoverDwarfSections(CASObj); });
}

static Optional<StringRef> getLinkageName(DWARFDie &CUDie) {
  if (CUDie.getTag() == dwarf::DW_TAG_subprogram) {
    auto Decl = CUDie.findRecursively({dwarf::DW_AT_declaration});
    if (!Decl) {
      StringRef LinkageName = CUDie.getLinkageName();
      if (LinkageName.size())
        return LinkageName;
    }
  }
  DWARFDie Child = CUDie.getFirstChild();
  while (Child) {
    auto Name = getLinkageName(Child);
    Child = Child.getSibling();
    if (Name)
      return Name;
  }
}

static void findStrpsInCompileUnit(DWARFDie &CUDie, raw_ostream &OS) {
  for (auto Attr : CUDie.attributes()) {
    if (Attr.Value.getForm() == llvm::dwarf::DW_FORM_strp) {
      Attr.Value.dump(OS);
      OS << "\n";
    }
  }
  DWARFDie Child = CUDie.getFirstChild();
  while (Child) {
    findStrpsInCompileUnit(Child, OS);
    Child = Child.getSibling();
  }
}

// void CASDWARFObject::addLinkageNameAndObjectRefToMap(DWARFDie &CUDie,
//                                                      MCObjectProxy MCObj,
//                                                      bool &LinkageFound,
//                                                      DWARFUnit &U,
//                                                      DIDumpOptions &DumpOpts)
//                                                      {
//   if (CUDie.getTag() == dwarf::DW_TAG_subprogram) {
//     auto Decl = CUDie.findRecursively({dwarf::DW_AT_declaration});
//     if (!Decl) {
//       StringRef LinkageName = CUDie.getLinkageName();
//       if (LinkageName.size()) {
//         if (MapOfLinkageNames.find(LinkageName.data()) ==
//             MapOfLinkageNames.end())
//           MapOfLinkageNames.try_emplace(LinkageName.data(),
//                                         std::unordered_set<std::string>());
//         SmallVector<char, 0> DumpContents;
//         raw_svector_ostream DumpStream(DumpContents);
//         U.dump(DumpStream, DumpOpts);
//         MapOfLinkageNames[LinkageName.data()].insert(DumpStream.str().data());
//         LinkageFound = true;
//       }
//     }
//   }
//   DWARFDie Child = CUDie.getFirstChild();
//   while (Child && !LinkageFound) {
//     addLinkageNameAndObjectRefToMap(Child, MCObj, LinkageFound, U, DumpOpts);
//     Child = Child.getSibling();
//   }
// }

Error CASDWARFObject::dump(raw_ostream &OS, int Indent, DWARFContext &DWARFCtx,
                           MCObjectProxy MCObj, bool ShowForm, bool Verbose,
                           bool DumpSameLinkageDifferentCU) {
  if (!DumpSameLinkageDifferentCU)
    OS.indent(Indent);
  DIDumpOptions DumpOpts;
  DumpOpts.ShowChildren = true;
  DumpOpts.ShowForm = ShowForm;
  DumpOpts.Verbose = Verbose;
  Error Err = Error::success();
  StringRef Data = MCObj.getData();
  if (Data.empty())
    return Err;
  if (DebugStrRef::Cast(MCObj) && !DumpSameLinkageDifferentCU) {
    // Dump __debug_str data.
    assert(Data.data()[Data.size()] == 0);
    DataExtractor StrData(StringRef(Data.data(), Data.size() + 1),
                          isLittleEndian(), 0);
    // This is almost identical with the DumpStrSection lambda in
    // DWARFContext.cpp
    uint64_t Offset = 0;
    uint64_t StrOffset = 0;
    while (StrData.isValidOffset(Offset)) {
      const char *CStr = StrData.getCStr(&Offset, &Err);
      if (Err)
        return Err;
      OS << format("0x%8.8" PRIx64 ": \"", StrOffset);
      OS.write_escaped(CStr);
      OS << "\"\n";
      StrOffset = Offset;
    }
  } else if (DebugLineRef::Cast(MCObj) && !DumpSameLinkageDifferentCU) {
    // Dump __debug_line data.
    uint64_t Address = 0;
    DWARFDataExtractor LineData(*this, {Data, Address}, isLittleEndian(), 0);
    DWARFDebugLine::SectionParser Parser(LineData, DWARFCtx,
                                         DWARFCtx.normal_units());
    while (!Parser.done()) {
      OS << "debug_line[" << format("0x%8.8" PRIx64, Parser.getOffset())
         << "]\n";
      Parser.parseNext(DumpOpts.WarningHandler, DumpOpts.WarningHandler, &OS,
                       DumpOpts.Verbose);
    }
  } else if (DebugInfoCURef::Cast(MCObj)) {
    // Dump __debug_info data.
    DWARFUnitVector UV;
    uint64_t Address = 0;
    DWARFSection Section = {CUDataVec[CompileUnitIndex++], Address};
    DWARFUnitHeader Header;
    DWARFDebugAbbrev Abbrev;

    Abbrev.extract(
        DataExtractor(getAbbrevSection(), isLittleEndian(), getAddressSize()));
    uint64_t offset_ptr = 0;
    Header.extract(
        DWARFCtx,
        DWARFDataExtractor(*this, Section, isLittleEndian(), getAddressSize()),
        &offset_ptr, DWARFSectionKind::DW_SECT_INFO);
    DWARFCompileUnit U(DWARFCtx, Section, Header, &Abbrev, &getRangesSection(),
                       &getLocSection(), getStrSection(),
                       getStrOffsetsSection(), &getAddrSection(),
                       getLocSection(), isLittleEndian(), false, UV);
    if (DumpSameLinkageDifferentCU) {
      if (DWARFDie CUDie = U.getUnitDIE(false)) {
        auto Name = getLinkageName(CUDie);
        if (!Name)
          return Error::success();
        SmallVector<char, 0> StrpData;
        raw_svector_ostream OS(StrpData);
        findStrpsInCompileUnit(CUDie, OS);
        if (MapOfLinkageNames.find(Name->data()) == MapOfLinkageNames.end())
          MapOfLinkageNames.try_emplace(Name->data(),
                                        std::unordered_set<std::string>());
        MapOfLinkageNames[Name->data()].insert(StrpData.data());
      }

    } else {
      U.dump(OS, DumpOpts);
    }
  }
  return Err;
}

Error CASDWARFObject::dumpSimilarCUs() {
  for (auto KeyValue : MapOfLinkageNames) {
    if (KeyValue.second.size() == 2) {
      for (auto ID : KeyValue.second) {
        llvm::outs() << KeyValue.first << " " << ID << "\n";
        break;
      }
    }
  }
  for (auto KeyValue : MapOfLinkageNames) {
    if (KeyValue.second.size() == 2) {
      int count = 0;
      for (auto ID : KeyValue.second) {
        if (count == 0) {
          count++;
          continue;
        }
        llvm::outs() << KeyValue.first << " " << ID << "\n";
      }
    }
  }
  return Error::success();
}
