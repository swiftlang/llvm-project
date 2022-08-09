//===- llvm-cas-dump.cpp - Tool for printing MC CAS objects ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCCASPrinter.h"
#include "llvm/CAS/Utils.h"
#include "llvm/MC/CAS/MCCASObjectV1.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::mccasformats::v1;

cl::opt<std::string> CASPath("cas", cl::Required, cl::desc("Path to CAS."));
cl::list<std::string> InputStrings(cl::Positional,
                                   cl::desc("CAS ID of the object to print"));
cl::opt<bool> CASIDFile("casid-file", cl::desc("Treat inputs as CASID files"));
cl::opt<bool> DwarfSectionsOnly("dwarf-sections-only",
                                cl::desc("Only print DWARF related sections"));
cl::opt<bool> DwarfDump("dwarf-dump",
                        cl::desc("Print the contents of DWARF sections"));
cl::opt<bool> DebugAbbrevOffsets(
    "debug-abbrev-offsets",
    cl::desc("Print the contents of abbreviation offsets block"));

namespace {

/// If the input is a file (--casid-file), open the file given by `InputStr`
/// and get the ID from the file buffer.
/// Otherwise parse `InputStr` as a CASID.
CASID getCASIDFromInput(CASDB &CAS, StringRef InputStr) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner((InputStr + ": ").str());

  if (!CASIDFile)
    return ExitOnErr(CAS.parseID(InputStr));

  auto ObjBuffer =
      ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(InputStr)));
  return ExitOnErr(readCASIDBuffer(CAS, ObjBuffer->getMemBufferRef()));
}
} // namespace

int main(int argc, char *argv[]) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  cl::ParseCommandLineOptions(argc, argv);
  PrinterOptions Options = {DwarfSectionsOnly, DwarfDump, DebugAbbrevOffsets};

  std::unique_ptr<CASDB> CAS = ExitOnErr(createOnDiskCAS(CASPath));
  MCCASPrinter Printer(Options, *CAS, llvm::outs());

  for (StringRef InputStr : InputStrings) {
    auto ID = getCASIDFromInput(*CAS, InputStr);

    auto Ref = CAS->getReference(ID);
    if (!Ref) {
      llvm::errs() << "ID is invalid for this CAS\n";
      return 1;
    }

    ExitOnErr(Printer.printMCObject(*Ref));
  }
  return 0;
}
