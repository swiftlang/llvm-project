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

MCCASPrinter::MCCASPrinter(PrinterOptions Options, CASDB &CAS, raw_ostream &OS)
    : Options(Options), MCSchema(CAS), Indent{0}, OS(OS) {}

MCCASPrinter::~MCCASPrinter() {}

Error MCCASPrinter::printMCObject(ObjectRef CASObj, DWARFContext *DWARFCtx) {
  // The object identifying the schema is not considered an MCObject, as such we
  // don't attempt to cast or print it.
  if (CASObj == MCSchema.getRootNodeTypeID())
    return Error::success();

  Expected<MCObjectProxy> MCObj = MCSchema.get(CASObj);
  if (!MCObj)
    return MCObj.takeError();
  return printMCObject(*MCObj, DWARFCtx);
}

Error MCCASPrinter::printMCObject(MCObjectProxy MCObj, DWARFContext *DWARFCtx) {
  // Initialize DWARFObj and scan and make a first pass through the sections.
  std::unique_ptr<DWARFContext> DWARFContextHolder;
  if (Options.DwarfDump && !DWARFCtx) {
    auto DWARFObj = std::make_unique<CASDWARFObject>(MCObj.getSchema());
    if (Error err = DWARFObj->discoverDwarfSections(MCObj))
      return err;
    DWARFContextHolder = std::make_unique<DWARFContext>(std::move(DWARFObj));
    DWARFCtx = DWARFContextHolder.get();
  }

  // If only debug sections were requested, skip non-debug sections.
  if (Options.DwarfSectionsOnly && SectionRef::Cast(MCObj) &&
      !isDwarfSection(MCObj))
    return Error::success();

  // Print CAS Id.
  OS.indent(Indent);
  OS << formatv("{0, -15} {1} \n", MCObj.getKindString(),
                MCSchema.CAS.getID(MCObj));

  // Dwarfdump.
  if (DWARFCtx) {
    IndentGuard Guard(Indent);
    auto *DWARFObj =
        static_cast<const CASDWARFObject *>(&DWARFCtx->getDWARFObj());
    if (Error Err = DWARFObj->dump(OS, Indent, *DWARFCtx, MCObj))
      return Err;
  }
  return printSimpleNested(MCObj, DWARFCtx);
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
                                      DWARFContext *DWARFCtx) {
  IndentGuard Guard(Indent);

  if (auto AbbrevOffsetsRef = DebugAbbrevOffsetsRef::Cast(AssemblerRef);
      Options.DebugAbbrevOffsets && AbbrevOffsetsRef)
    if (auto E = printAbbrevOffsets(OS, *AbbrevOffsetsRef))
      return E;

  return AssemblerRef.forEachReference(
      [&](ObjectRef CASObj) { return printMCObject(CASObj, DWARFCtx); });
}
