//===- Main.cpp - Top-Level TableGen implementation -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TableGen is a tool which can be used to build up a description of something,
// then invoke one or more "tablegen backends" to emit information about the
// description in some predefined format.  In practice, this is used by the LLVM
// code generators to automate generation of a code generator through a
// high-level description of the target.
//
//===----------------------------------------------------------------------===//

#include "llvm/TableGen/Main.h"
#include "TGParser.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CAS/CASFileSystem.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrefixMapper.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/ScanDependencies.h"
#include <algorithm>
#include <cstdio>
#include <system_error>
using namespace llvm;
using namespace llvm::tablegen;

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"),
               cl::init("-"));

static cl::opt<std::string>
DependFilename("d",
               cl::desc("Dependency filename"),
               cl::value_desc("filename"),
               cl::init(""));

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::list<std::string>
IncludeDirs("I", cl::desc("Directory of include files"),
            cl::value_desc("directory"), cl::Prefix);

static cl::list<std::string>
MacroNames("D", cl::desc("Name of the macro to be defined"),
            cl::value_desc("macro name"), cl::Prefix);

static cl::opt<bool>
WriteIfChanged("write-if-changed", cl::desc("Only write output if it changed"));

static cl::opt<bool> Depscan("depscan",
                             cl::desc("Use a CAS, depscanning, and caching"));
static cl::list<std::string>
    DepscanPrefixMap("depscan-prefix-map",
                     cl::desc("Prefix mapping for --depscan"));

static cl::opt<bool>
TimePhases("time-phases", cl::desc("Time phases of parser and backend"));

static int reportError(const char *ProgName, Error E) {
  if (!E)
    return 0;

  handleAllErrors(std::move(E), [ProgName](const ErrorInfoBase &EI) {
    errs() << ProgName << ": ";
    EI.log(errs());
  });
  errs().flush();
  return 1;
}

static SmallVectorImpl<char> *CapturedDepend = nullptr;
static std::string *CapturedOutput = nullptr;

static Error
createDependencyFileImpl(function_ref<void(raw_ostream &OS)> Write) {
  if (OutputFilename == "-")
    return createStringError(inconvertibleErrorCode(),
                             "the option -d must be used together with -o");

  std::error_code EC;
  ToolOutputFile DepOut(DependFilename, EC, sys::fs::OF_Text);
  if (EC)
    return createStringError(EC, "error opening " + DependFilename + ":" +
                                     EC.message());
  Write(DepOut.os());
  DepOut.keep();
  return Error::success();
}

static PrefixMapper *CreateDependencyFilePM = nullptr;

/// Create a dependency file for `-d` option.
///
/// This functionality is really only for the benefit of the build system.
/// It is similar to GCC's `-M*` family of options.
template <class StringSetT>
static Error createDependencyFile(const StringSetT &Dependencies) {
  return createDependencyFileImpl([&](raw_ostream &OS) {
    OS << OutputFilename << ":";
    for (StringRef Dep : Dependencies) {
      OS << ' '
         << (CreateDependencyFilePM ? CreateDependencyFilePM->map(Dep) : Dep);
    }
    OS << "\n";
  });
}

static Error createDependencyFile(const TGParser &Parser) {
  if (CapturedDepend) {
    for (StringRef Dep : Parser.getDependencies()) {
      CapturedDepend->append(Dep.begin(), Dep.end());
      CapturedDepend->push_back(0);
    }
  }
  return createDependencyFile(Parser.getDependencies());
}

namespace {
class AlreadyReportedError : public ErrorInfo<AlreadyReportedError> {
  virtual void anchor() override;

public:
  void log(raw_ostream &) const override {}
  std::error_code convertToErrorCode() const override {
    return inconvertibleErrorCode();
  }

  // Used by ErrorInfo::classID.
  static char ID;

  AlreadyReportedError() = default;
};
} // end namespace

void AlreadyReportedError::anchor() {}
char AlreadyReportedError::ID = 0;

static Error createOutputFile(StringRef Out) {
  bool WriteFile = true;
  if (WriteIfChanged) {
    // Only updates the real output file if there are any differences.
    // This prevents recompilation of all the files depending on it if there
    // aren't any.
    if (auto ExistingOrErr =
            MemoryBuffer::getFile(OutputFilename, /*IsText=*/true))
      if (std::move(ExistingOrErr.get())->getBuffer() == Out)
        WriteFile = false;
  }
  if (WriteFile) {
    std::error_code EC;
    ToolOutputFile OutFile(OutputFilename, EC, sys::fs::OF_Text);
    if (EC)
      return createStringError(EC, "error opening " + OutputFilename + ": " +
                                       EC.message());
    OutFile.os() << Out;
    if (ErrorsPrinted == 0)
      OutFile.keep();
  }
  return Error::success();
}

static Error
TableGenMainImpl(TableGenMainFn *MainFn,
                 std::unique_ptr<MemoryBuffer> MainFile = nullptr) {
  RecordKeeper Records;

  if (TimePhases)
    Records.startPhaseTiming();

  // Parse the input file.
  Records.startTimer("Parse, build records");
  if (!MainFile) {
    ErrorOr<std::unique_ptr<MemoryBuffer>> FileOrErr =
        MemoryBuffer::getFileOrSTDIN(InputFilename, /*IsText=*/true);
    if (std::error_code EC = FileOrErr.getError())
      return createMainFileError(InputFilename, EC);
    MainFile = std::move(*FileOrErr);
  }

  Records.saveInputFilename(InputFilename);

  // Tell SrcMgr about this buffer, which is what TGParser will pick up.
  SrcMgr.AddNewSourceBuffer(std::move(MainFile), SMLoc());

  // Record the location of the include directory so that the lexer can find
  // it later.
  SrcMgr.setIncludeDirs(IncludeDirs);

  TGParser Parser(SrcMgr, MacroNames, Records);

  if (Parser.ParseFile())
    return make_error<AlreadyReportedError>();
  Records.stopTimer();

  // Write output to memory.
  Records.startBackendTimer("Backend overall");
  std::string OutString;
  raw_string_ostream Out(OutString);
  unsigned status = MainFn(Out, Records);
  Records.stopBackendTimer();
  if (status)
    return make_error<AlreadyReportedError>();

  // Always write the depfile, even if the main output hasn't changed.
  // If it's missing, Ninja considers the output dirty.  If this was below
  // the early exit below and someone deleted the .inc.d file but not the .inc
  // file, tablegen would never write the depfile.
  if (!DependFilename.empty()) {
    if (Error E = createDependencyFile(Parser))
      return E;
  }

  Records.startTimer("Write output");
  if (Error E = createOutputFile(Out.str()))
    return E;
  if (CapturedOutput)
    *CapturedOutput = std::move(Out.str());

  Records.stopTimer();
  Records.stopPhaseTiming();

  if (ErrorsPrinted > 0)
    return createStringError(inconvertibleErrorCode(),
                             Twine(ErrorsPrinted) + " errors.");
  return Error::success();
}

int llvm::TableGenMain(const char *argv0, TableGenMainFn *MainFn) {
  return reportError(argv0, TableGenMainImpl(MainFn));
}

namespace {
struct TableGenCache {
  std::unique_ptr<cas::CASDB> CAS;
  Optional<cas::CASID> ExecutableID;
  Optional<cas::CASID> IncludesTreeID;
  Optional<cas::CASID> ActionID;
  Optional<cas::CASID> ResultID;
  Optional<cas::CASID> MainFileID;
  std::unique_ptr<MemoryBuffer> MainFile;
  Optional<std::string> OriginalInputFilename;

  SmallVector<MappedPrefix> PrefixMappings;
  BumpPtrAllocator Alloc;
  Optional<PrefixMapper> InversePM;

  TableGenCache() = default;
  ~TableGenCache() { CreateDependencyFilePM = nullptr; }

  Expected<cas::CASID> createExecutableBlob(StringRef Argv0);
  Expected<cas::CASID> createCommandLineBlob(ArrayRef<const char *> Args);
  Error createAction(ArrayRef<const char *> Args);

  Error lookupCachedResult(ArrayRef<const char *> Args);

  Error computeResult(TableGenMainFn *MainFn);
  Error replayResult();

  void createInversePrefixMap();
};
} // end namespace

void TableGenCache::createInversePrefixMap() {
  if (PrefixMappings.empty())
    return;

  InversePM.emplace(Alloc);
  InversePM->addInverseRange(PrefixMappings);
  CreateDependencyFilePM = &*InversePM;
}

Expected<cas::CASID> TableGenCache::createExecutableBlob(StringRef Argv0) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> Buffer = MemoryBuffer::getFile(Argv0);
  if (!Buffer)
    return errorCodeToError(Buffer.getError());
  Expected<cas::BlobRef> Blob = CAS->createBlob((**Buffer).getBuffer());
  if (!Blob)
    return Blob.takeError();
  return *Blob;
}

Expected<cas::CASID>
TableGenCache::createCommandLineBlob(ArrayRef<const char *> Args) {
  SmallString<1024> CommandLine;

  // Use raw_svector_stream since it doesn't buffer.
  raw_svector_ostream OS(CommandLine);
  auto serializeArg = [&](StringRef Arg) {
    OS << Arg;
    CommandLine.push_back(0);
  };
  serializeArg(sys::path::filename(Args[0]));
  Args = Args.drop_front();

  // Serialize remapped includes.
  for (StringRef Dir : IncludeDirs) {
    serializeArg("-I");
    serializeArg(Dir);
  }

  // Serialize other args.
  while (!Args.empty()) {
    StringRef Arg = Args.front();
    // Skip known inputs, which are handled specially. Skip known outputs,
    // which don't affect the result. Skip prefix remapping.
    if (Arg == "-I" || Arg == "-d" || Arg == "-o" ||
        Arg == "--depscan-prefix-map") {
      Args = Args.drop_front(2);
      continue;
    }
    if (Arg.startswith("-I") || Arg.startswith("-d") || Arg.startswith("-o") ||
        Arg.startswith("--depscan-prefix-map=") || Arg == "--depscan") {
      Args = Args.drop_front();
      continue;
    }

    // Serialize the remapped input.
    if (OriginalInputFilename && *OriginalInputFilename == Args.front()) {
      serializeArg(InputFilename);
      Args = Args.drop_front();
      continue;
    }

    serializeArg(Args.front());
    Args = Args.drop_front();
  }

  Expected<cas::BlobRef> Blob = CAS->createBlob(CommandLine);
  if (!Blob)
    return Blob.takeError();
  return *Blob;
}

Error TableGenCache::createAction(ArrayRef<const char *> Args) {
  Expected<cas::CASID> CommandLineID = createCommandLineBlob(Args);
  if (!CommandLineID)
    return CommandLineID.takeError();

  cas::HierarchicalTreeBuilder Builder;
  Builder.push(*IncludesTreeID, cas::TreeEntry::Tree, "includes");
  Builder.push(*MainFileID, cas::TreeEntry::Regular, "input");
  Builder.push(*ExecutableID, cas::TreeEntry::Regular, "executable");
  Builder.push(*CommandLineID, cas::TreeEntry::Regular, "command-line");
  Expected<cas::TreeRef> Tree = Builder.create(*CAS);
  if (!Tree)
    return Tree.takeError();
  ActionID = *Tree;
  return Error::success();
}

Error TableGenCache::lookupCachedResult(ArrayRef<const char *> Args) {
  if (Error E =
          cas::createOnDiskCAS(cas::getDefaultOnDiskCASPath()).moveInto(CAS))
    return E;

  if (Error E = createExecutableBlob(Args[0]).moveInto(ExecutableID))
    return E;

  OriginalInputFilename = InputFilename.getValue();
  Expected<ScanIncludesResult> Scan = scanIncludesAndRemap(
      *CAS, *ExecutableID, InputFilename, *&IncludeDirs, PrefixMappings);
  if (!Scan)
    return Scan.takeError();

  // Save the results of the scan.
  IncludesTreeID = Scan->IncludesTree;
  MainFileID = Scan->MainBlob;
  MainFile =
      MemoryBuffer::getMemBuffer(Scan->MainBlob.getData(), InputFilename);

  if (Error E = createAction(Args))
    return E;

  // Not an error for the result to be missing.
  ResultID = expectedToOptional(CAS->getCachedResult(*ActionID));
  return Error::success();
}

namespace {
struct CapturedDiagnostics {
  std::string Diags;
  raw_string_ostream OS;

  static void diagnose(const SMDiagnostic &Diag, void *This_) {
    auto *This = reinterpret_cast<CapturedDiagnostics *>(This_);

    // Print live.
    SrcMgr.setDiagHandler(nullptr);
    SrcMgr.PrintMessage(errs(), Diag);

    // Capture.
    //
    // FIXME: Serialize the diagnostic semantically to replay it with colours
    // and to refer to the sources already in the CAS.
    SrcMgr.PrintMessage(This->OS, Diag);

    // Put back the handler.
    SrcMgr.setDiagHandler(&CapturedDiagnostics::diagnose, This);
  }

  CapturedDiagnostics() : OS(Diags) {}
};
} // end namespace

Error TableGenCache::computeResult(TableGenMainFn *MainFn) {
  if (ResultID)
    return replayResult();

  std::string CapturedStderr;
  std::string OutputStorage;
  SmallString<256> DependStorage;

  CapturedDiagnostics Diags;
  if (ActionID) {
    assert(IncludesTreeID && "Expected an input tree...");
    if (auto FS = cas::createCASFileSystem(*CAS, *IncludesTreeID))
      SrcMgr.setFileSystem(std::move(*FS));
    else
      return FS.takeError();

    CapturedOutput = &OutputStorage;
    CapturedDepend = &DependStorage;
    SrcMgr.setDiagHandler(&CapturedDiagnostics::diagnose, &Diags);
  }
  auto ResetCaptures = make_scope_exit([&]() {
    CapturedOutput = nullptr;
    CapturedDepend = nullptr;
  });

  // Don't cache failed builds for now.
  if (Error E = TableGenMainImpl(MainFn, std::move(MainFile)))
    return E;

  // Caching not turned on.
  if (!ActionID)
    return Error::success();

  cas::HierarchicalTreeBuilder Builder;
  auto addFile = [&](StringRef Name, StringRef Data) -> Error {
    Expected<cas::CASID> ID = CAS->createBlob(Data);
    if (!ID)
      return ID.takeError();
    Builder.push(*ID, cas::TreeEntry::Regular, Name);
    return Error::success();
  };

  if (Error E = addFile("output", *CapturedOutput))
    return E;
  if (!DependFilename.empty())
    if (Error E = addFile("depend", DependStorage))
      return E;
  if (!Diags.OS.str().empty())
    if (Error E = addFile("stderr", Diags.OS.str()))
      return E;

  Expected<cas::TreeRef> Tree = Builder.create(*CAS);
  if (!Tree)
    return Tree.takeError();
  return CAS->putCachedResult(*ActionID, *Tree);
}

Error TableGenCache::replayResult() {
  assert(ResultID && "Need a result!");

  Expected<cas::TreeRef> Tree = CAS->getTree(*ResultID);
  if (!Tree)
    return Tree.takeError();

  auto getBlob = [&](StringRef Name, Optional<cas::BlobRef> &Blob) -> Error {
    Optional<cas::NamedTreeEntry> Entry = Tree->lookup(Name);
    if (!Entry)
      return Error::success();
    Expected<cas::BlobRef> ExpectedBlob = CAS->getBlob(Entry->getID());
    if (!ExpectedBlob)
      return ExpectedBlob.takeError();
    Blob = *ExpectedBlob;
    return Error::success();
  };
  Optional<cas::BlobRef> Output;
  Optional<cas::BlobRef> Depend;
  Optional<cas::BlobRef> Stderr;
  if (Error E = getBlob("output", Output))
    return E;
  if (Error E = getBlob("depend", Depend))
    return E;
  if (Error E = getBlob("stderr", Stderr))
    return E;
  if (!Output)
    return createStringError(inconvertibleErrorCode(),
                             "cached result has no output");

  if (Depend) {
    SmallVector<StringRef> Dependencies;
    splitFlattenedStrings(Depend->getData(), Dependencies);
    if (Error E = createDependencyFile(Dependencies))
      return E;
  }

  // FIXME: This should replay diagnostics semantically, adding colours as
  // appropriate. Currently it's low-fidelity.
  if (Stderr)
    errs() << Stderr->getData();

  if (Error E = createOutputFile(Output->getData()))
    return E;

  return Error::success();
}

int llvm::TableGenMain(ArrayRef<const char *> Args, TableGenMainFn *MainFn) {
  assert(!Args.empty() && "Missing argv0!");
  if (!Depscan)
    return TableGenMain(Args[0], MainFn);

  TableGenCache Cache;
  if (Error E =
          MappedPrefix::transformJoined(DepscanPrefixMap, Cache.PrefixMappings))
    return reportError(Args[0], std::move(E));
  Cache.createInversePrefixMap();

  if (Error E = Cache.lookupCachedResult(Args))
    return reportError(Args[0], std::move(E));

  Error E = Cache.computeResult(MainFn);
  return reportError(Args[0], std::move(E));
}
