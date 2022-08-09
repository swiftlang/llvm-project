//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCCASPrinter.h"
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

  if (FirstMCRef->getKindString() == "reloc_addend") {
    // The FirstRef is a RelocAddendRef, check the second reference if it
    // exists.
    if (MCObj.getNumReferences() < 2)
      return false;
    ObjectRef SecondRef = MCObj.getReference(1);
    Expected<MCObjectProxy> SecondMCRef = Schema.get(SecondRef);
    if (!SecondMCRef)
      return false;
    return SecondMCRef->getKindString().contains("debug");
  }
  return FirstMCRef->getKindString().contains("debug");
}
} // namespace

MCCASPrinter::MCCASPrinter(PrinterOptions Options, CASDB &CAS, raw_ostream &OS)
    : Options(Options), MCSchema(CAS), Indent{0}, OS(OS) {}

Error MCCASPrinter::printMCObject(ObjectRef CASObj) {
  // The object identifying the schema is not considered an MCObject, as such we
  // don't attempt to cast or print it.
  if (CASObj == MCSchema.getRootNodeTypeID())
    return Error::success();

  Expected<MCObjectProxy> MCObj = MCSchema.get(CASObj);
  if (!MCObj)
    return MCObj.takeError();
  return printMCObject(*MCObj);
}

Error MCCASPrinter::printMCObject(MCObjectProxy MCObj) {
  // If only debug sections were requested, skip non-debug sections.
  if (Options.DwarfSectionsOnly && SectionRef::Cast(MCObj) &&
      !isDwarfSection(MCObj))
    return Error::success();

  OS.indent(Indent);
  OS << formatv("{0, -15} {1} \n", MCObj.getKindString(),
                MCSchema.CAS.getID(MCObj));

  return printSimpleNested(MCObj);
}

Error MCCASPrinter::printSimpleNested(MCObjectProxy AssemblerRef) {
  IndentGuard Guard(Indent);
  return AssemblerRef.forEachReference(
      [this](ObjectRef CASObj) { return printMCObject(CASObj); });
}
