//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCCASPrinter.h"
#include "CASDWARFObject.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/FormatVariadic.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::mccasformats::v1;

namespace {
struct IndentGuard {
  constexpr static int IndentWidth = 2;
  IndentGuard(int &Indent) : Indent{Indent} { Indent += IndentWidth; }
  ~IndentGuard() { Indent -= IndentWidth; }
  int &Indent;
};

bool isDwarfSection(MCObjectProxy MCObj) {
  // Currently, the only way to detect debug sections is through the kind of its
  // children objects. TODO: find a better way to check this.
  // Dwarf Sections have >= 1 references.
  if (MCObj.getNumReferences() == 0)
    return false;

  ObjectRef FirstRef = MCObj.getReference(0);
  const MCSchema &Schema = MCObj.getSchema();
  Expected<MCObjectProxy> FirstMCRef = Schema.get(FirstRef);
  if (!FirstMCRef)
    return false;

  return FirstMCRef->getKindString().contains("debug");
}
} // namespace

MCCASPrinter::MCCASPrinter(PrinterOptions Options, ObjectStore &CAS,
                           raw_ostream &OS)
    : Options(Options), MCSchema(CAS), Indent{0}, OS(OS) {}

MCCASPrinter::~MCCASPrinter() { OS << "\n"; }

Expected<CASDWARFObject>
MCCASPrinter::discoverDwarfSections(cas::ObjectRef CASObj) {
  Expected<MCObjectProxy> MCObj = MCSchema.get(CASObj);
  if (!MCObj)
    return MCObj.takeError();
  CASDWARFObject DWARFObj(MCObj->getSchema());
  if (Options.DwarfDump || Options.DumpSameLinkageDifferentCU) {
    if (Error E = DWARFObj.discoverDwarfSections(*MCObj))
      return std::move(E);
    if (Error E = DWARFObj.discoverDebugInfoSection(*MCObj, OS))
      return std::move(E);
  }
  return DWARFObj;
}

Error MCCASPrinter::dumpSimilarCUs(CASDWARFObject &Obj) {
  return Obj.dumpSimilarCUs(MCSchema);
}

Error MCCASPrinter::printMCObject(ObjectRef CASObj, CASDWARFObject &Obj,
                                  std::string InputStr,
                                  DWARFContext *DWARFCtx) {
  // The object identifying the schema is not considered an MCObject, as such we
  // don't attempt to cast or print it.
  if (CASObj == MCSchema.getRootNodeTypeID())
    return Error::success();

  Expected<MCObjectProxy> MCObj = MCSchema.get(CASObj);
  if (!MCObj)
    return MCObj.takeError();
  return printMCObject(*MCObj, Obj, InputStr, DWARFCtx);
}

Error MCCASPrinter::printMCObject(MCObjectProxy MCObj, CASDWARFObject &Obj,
                                  std::string InputStr,
                                  DWARFContext *DWARFCtx) {
  // Initialize DWARFObj.
  std::unique_ptr<DWARFContext> DWARFContextHolder;
  if ((Options.DwarfDump || Options.DumpSameLinkageDifferentCU) && !DWARFCtx) {
    auto DWARFObj = std::make_unique<CASDWARFObject>(Obj);
    DWARFContextHolder = std::make_unique<DWARFContext>(std::move(DWARFObj));
    DWARFCtx = DWARFContextHolder.get();
  }

  // If only debug sections were requested, skip non-debug sections.
  if (Options.DwarfSectionsOnly && SectionRef::Cast(MCObj) &&
      !isDwarfSection(MCObj))
    return Error::success();

  // Print CAS Id.
  if (!Options.DumpSameLinkageDifferentCU) {
    OS.indent(Indent);
    OS << formatv("{0, -15} {1} \n", MCObj.getKindString(), MCObj.getID());
    if (Options.HexDump) {
      auto data = MCObj.getData();
      if (Options.HexDumpOneLine) {
        OS.indent(Indent);
        llvm::interleave(
            data.take_front(data.size()), OS,
            [this](unsigned char c) { OS << llvm::format_hex(c, 4); }, " ");
        OS << "\n";
      } else {
        while (!data.empty()) {
          OS.indent(Indent);
          llvm::interleave(
              data.take_front(8), OS,
              [this](unsigned char c) { OS << llvm::format_hex(c, 4); }, " ");
          OS << "\n";
          data = data.drop_front(data.size() < 8 ? data.size() : 8);
        }
      }
    }
  }

  // Dwarfdump.
  if (DWARFCtx) {
    IndentGuard Guard(Indent);
    if (Error Err = Obj.dump(OS, Indent, *DWARFCtx, MCObj, Options.ShowForm,
                             Options.Verbose,
                             Options.DumpSameLinkageDifferentCU, InputStr))
      return Err;
  }
  return printSimpleNested(MCObj, Obj, DWARFCtx, InputStr);
}

static Error printAbbrevOffsets(raw_ostream &OS,
                                DebugAbbrevOffsetsRef OffsetsRef) {
  DebugAbbrevOffsetsRefAdaptor Adaptor(OffsetsRef);
  Expected<SmallVector<size_t>> Offsets = Adaptor.decodeOffsets();
  if (!Offsets)
    return Offsets.takeError();
  llvm::interleaveComma(*Offsets, OS);
  OS << "\n";
  return Error::success();
}

Error MCCASPrinter::printSimpleNested(MCObjectProxy AssemblerRef,
                                      CASDWARFObject &Obj,
                                      DWARFContext *DWARFCtx,
                                      std::string InputStr) {
  IndentGuard Guard(Indent);

  if (auto AbbrevOffsetsRef = DebugAbbrevOffsetsRef::Cast(AssemblerRef);
      Options.DebugAbbrevOffsets && AbbrevOffsetsRef)
    if (auto E = printAbbrevOffsets(OS, *AbbrevOffsetsRef))
      return E;

  auto Data = AssemblerRef.getData();
  if (DebugAbbrevSectionRef::Cast(AssemblerRef) ||
      GroupRef::Cast(AssemblerRef) || SymbolTableRef::Cast(AssemblerRef) ||
      SectionRef::Cast(AssemblerRef) ||
      DebugLineSectionRef::Cast(AssemblerRef) || AtomRef::Cast(AssemblerRef)) {
    auto Refs = MCObjectProxy::decodeReferences(AssemblerRef, Data);
    if (!Refs)
      return Refs.takeError();
    for (auto Ref : *Refs) {
      if (Error E = printMCObject(Ref, Obj, InputStr, DWARFCtx))
        return E;
    }
    return Error::success();
  }
  return AssemblerRef.forEachReference([&](ObjectRef CASObj) {
    return printMCObject(CASObj, Obj, InputStr, DWARFCtx);
  });
}
