//===- llvm-cas.cpp - CAS tool --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Optional.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASFileSystem.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/CAS/Utils.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrefixMapper.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <system_error>

using namespace llvm;
using namespace llvm::cas;

static cl::opt<bool> AllTrees("all-trees",
                              cl::desc("Print all trees, not just empty ones, for ls-tree-recursive"));

static int listTree(CASDB &CAS, CASID ID);
static int listTreeRecursively(CASDB &CAS, CASID ID);
static int listObjectReferences(CASDB &CAS, CASID ID);
static int catBlob(CASDB &CAS, CASID ID);
static int catNodeData(CASDB &CAS, CASID ID);
static int printKind(CASDB &CAS, CASID ID);
static int makeBlob(CASDB &CAS, StringRef DataPath);
static int makeNode(CASDB &CAS, ArrayRef<std::string> References, StringRef DataPath);
static int diffGraphs(CASDB &CAS, CASID LHS, CASID RHS);
static int traverseGraph(CASDB &CAS, CASID ID);
static int ingestFileSystem(CASDB &CAS, StringRef Path,
                            ArrayRef<MappedPrefix> Mappings);
static int getCASIDForFile(CASDB &CAS, CASID ID, StringRef Path);

int main(int Argc, char **Argv) {
  InitLLVM X(Argc, Argv);

  cl::list<std::string> Objects(cl::Positional, cl::desc("<object>..."));
  cl::opt<std::string> CASPath("cas", cl::desc("Path to CAS on disk."));
  cl::opt<std::string> DataPath("data",
                                cl::desc("Path to data or '-' for stdin."));
  cl::list<std::string> PrefixMap(
      "prefix-map", cl::desc("Prefix map for ingesting file system"));

  enum CommandKind {
    Invalid,
    PrintKind,
    CatBlob,
    CatNodeData,
    DiffGraphs,
    TraverseGraph,
    MakeBlob,
    MakeNode,
    ListTree,
    ListTreeRecursive,
    ListObjectReferences,
    IngestFileSystem,
    GetCASIDForFile,
  };
  cl::opt<CommandKind> Command(
      cl::desc("choose command action:"),
      cl::values(
          clEnumValN(PrintKind, "print-kind", "print kind"),
          clEnumValN(CatBlob, "cat-blob", "cat blob"),
          clEnumValN(CatNodeData, "cat-node-data", "cat node data"),
          clEnumValN(DiffGraphs, "diff-graphs", "diff graphs"),
          clEnumValN(TraverseGraph, "traverse-graph", "traverse graph"),
          clEnumValN(MakeBlob, "make-blob", "make blob"),
          clEnumValN(MakeNode, "make-node", "make node"),
          clEnumValN(ListTree, "ls-tree", "list tree"),
          clEnumValN(ListTreeRecursive, "ls-tree-recursive",
                     "list tree recursive"),
          clEnumValN(ListObjectReferences, "ls-node-refs", "list node refs"),
          clEnumValN(IngestFileSystem, "ingest", "ingest file system"),
          clEnumValN(GetCASIDForFile, "get-cas-id", "get cas id for file")),
      cl::init(CommandKind::Invalid));

  cl::ParseCommandLineOptions(Argc, Argv, "llvm-cas CAS tool\n");
  ExitOnError ExitOnErr("llvm-cas: ");

  if (Command == CommandKind::Invalid)
    ExitOnErr(createStringError(inconvertibleErrorCode(),
                                "no command action is specified"));

  if (CASPath.empty())
    ExitOnErr(createStringError(inconvertibleErrorCode(), "missing --path"));
  std::unique_ptr<CASDB> CAS = ExitOnErr(llvm::cas::createOnDiskCAS(CASPath));
  assert(CAS);

  if (Command == MakeBlob)
    return makeBlob(*CAS, DataPath);

  if (Command == MakeNode)
    return makeNode(*CAS, Objects, DataPath);

  if (Command == DiffGraphs) {
    ExitOnError CommandErr("llvm-cas: diff-graphs");

    if (Objects.size() != 2)
      CommandErr(
          createStringError(inconvertibleErrorCode(), "expected 2 objects"));

    CASID LHS = ExitOnErr(CAS->parseCASID(Objects[0]));
    CASID RHS = ExitOnErr(CAS->parseCASID(Objects[1]));
    return diffGraphs(*CAS, LHS, RHS);
  }

  if (Command == IngestFileSystem) {
    ExitOnError CommandErr("llvm-cas: ingest prefix-map:");
    SmallVector<MappedPrefix> Mappings;
    if (auto Err = MappedPrefix::transformJoined(PrefixMap, Mappings))
      CommandErr(std::move(Err));

    return ingestFileSystem(*CAS, DataPath, Mappings);
  }

  // Remaining commands need exactly one CAS object.
  if (Objects.empty())
    ExitOnErr(createStringError(inconvertibleErrorCode(),
                                "missing <object> to operate on"));
  if (Objects.size() > 1)
    ExitOnErr(createStringError(inconvertibleErrorCode(),
                                "too many <object>s, expected 1"));
  CASID ID = ExitOnErr(CAS->parseCASID(Objects.front()));

  if (Command == TraverseGraph)
    return traverseGraph(*CAS, ID);

  if (Command == ListTree)
    return listTree(*CAS, ID);

  if (Command == ListTreeRecursive)
    return listTreeRecursively(*CAS, ID);

  if (Command == ListObjectReferences)
    return listObjectReferences(*CAS, ID);

  if (Command == CatNodeData)
    return catNodeData(*CAS, ID);

  if (Command == PrintKind)
    return printKind(*CAS, ID);

  if (Command == GetCASIDForFile)
    return getCASIDForFile(*CAS, ID, DataPath);

  assert(Command == CatBlob);
  return catBlob(*CAS, ID);
}

void printTreeEntryKind(raw_ostream &OS, TreeEntry::EntryKind Kind) {
  switch (Kind) {
  case TreeEntry::Regular:
    OS << "file";
    break;
  case TreeEntry::Executable:
    OS << "exec";
    break;
  case TreeEntry::Symlink:
    OS << "syml";
    break;
  case TreeEntry::Tree:
    OS << "tree";
    break;
  }
}

void printTreeEntry(CASDB &CAS, raw_ostream &OS, TreeEntry::EntryKind Kind,
                    CASID ID, StringRef Name) {
  printTreeEntryKind(OS, Kind);
  OS << " ";
  cantFail(CAS.printCASID(OS, ID));
  OS << " " << Name;
  if (Kind == TreeEntry::Tree)
    OS << "/";
  OS << "\n";
}

void printTreeEntry(CASDB &CAS, raw_ostream &OS, const TreeEntry &Entry, StringRef Name) {
  printTreeEntry(CAS, OS, Entry.getKind(), Entry.getID(), Name);
}

void printTreeEntry(CASDB &CAS, raw_ostream &OS, const NamedTreeEntry &Entry) {
  printTreeEntry(CAS, OS, Entry, Entry.getName());
}

int listTree(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: ls-tree: ");

  TreeRef Tree = ExitOnErr(CAS.getTree(ID));
  ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
    printTreeEntry(CAS, llvm::outs(), Entry);
    return Error::success();
  }));

  return 0;
}

int listTreeRecursively(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: ls-tree-recursively: ");
  ExitOnErr(walkFileTreeRecursively(
      CAS, ID,
      [&](const NamedTreeEntry &Entry, Optional<TreeRef> Tree) -> Error {
        if (Entry.getKind() != TreeEntry::Tree) {
          printTreeEntry(CAS, llvm::outs(), Entry);
          return Error::success();
        }
        if (Tree->empty() || AllTrees)
          printTreeEntry(CAS, llvm::outs(), Entry);
        return Error::success();
      }));

  return 0;
}

int catBlob(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: cat-blob: ");
  llvm::outs() << *ExitOnErr(CAS.getBlob(ID));
  return 0;
}

static Expected<std::unique_ptr<MemoryBuffer>>
openBuffer(StringRef DataPath) {
  if (DataPath.empty())
    return createStringError(inconvertibleErrorCode(), "--data missing");
  return errorOrToExpected(
      DataPath == "-" ? llvm::MemoryBuffer::getSTDIN()
                      : llvm::MemoryBuffer::getFile(DataPath));
}

int makeBlob(CASDB &CAS, StringRef DataPath) {
  ExitOnError ExitOnErr("llvm-cas: make-blob: ");
  std::unique_ptr<MemoryBuffer> Buffer =
      ExitOnErr(openBuffer(DataPath));

  BlobRef Blob = ExitOnErr(CAS.createBlob(Buffer->getBuffer()));
  ExitOnErr(CAS.printCASID(llvm::outs(), Blob));
  llvm::outs() << "\n";
  return 0;
}

int catNodeData(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: cat-node-data: ");
  llvm::outs() << ExitOnErr(CAS.getNode(ID)).getData();
  return 0;
}

int printKind(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: print-kind: ");
  Optional<ObjectKind> Kind = CAS.getObjectKind(ID);
  if (!Kind)
    ExitOnErr(createStringError(inconvertibleErrorCode(), "unknown object"));

  switch (*Kind) {
  case ObjectKind::Blob:
    llvm::outs() << "blob\n";
    break;
  case ObjectKind::Tree:
    llvm::outs() << "tree\n";
    break;
  case ObjectKind::Node:
    llvm::outs() << "node";
    break;
  }

  return 0;
}

int listObjectReferences(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: ls-node-refs: ");

  NodeRef Object = ExitOnErr(CAS.getNode(ID));
  ExitOnErr(Object.forEachReference([&](CASID ID) -> Error {
    if (Error E = CAS.printCASID(llvm::outs(), ID))
      return E;
    llvm::outs() << "\n";
    return Error::success();
  }));

  return 0;
}

static int makeNode(CASDB &CAS, ArrayRef<std::string> Objects, StringRef DataPath) {
  std::unique_ptr<MemoryBuffer> Data =
      ExitOnError("llvm-cas: make-node: data: ")(openBuffer(DataPath));

  SmallVector<CASID> IDs;
  for (StringRef Object : Objects) {
    ExitOnError ObjectErr("llvm-cas: make-node: ref: ");
    CASID ID = ObjectErr(CAS.parseCASID(Object));
    if (!CAS.isKnownObject(ID))
      ObjectErr(createStringError(inconvertibleErrorCode(),
                                  "unknown object '" + Object + "'"));
    IDs.push_back(ID);
  }

  ExitOnError ExitOnErr("llvm-cas: make-node: ");
  NodeRef Object = ExitOnErr(CAS.createNode(IDs, Data->getBuffer()));
  ExitOnErr(CAS.printCASID(llvm::outs(), Object));
  llvm::outs() << "\n";
  return 0;
}

namespace {
struct GraphInfo {
  SmallVector<cas::CASID> PostOrder;
  DenseSet<cas::CASID> Seen;
};
} // namespace

static GraphInfo traverseObjectGraph(CASDB &CAS, CASID TopLevel) {
  ExitOnError ExitOnErr("llvm-cas: traverse-node-graph: ");
  GraphInfo Info;

  SmallVector<std::pair<CASID, bool>> Worklist;
  auto push = [&](CASID ID) {
    if (Info.Seen.insert(ID).second)
      Worklist.push_back({ID, false});
  };
  push(TopLevel);
  while (!Worklist.empty()) {
    if (Worklist.back().second) {
      Info.PostOrder.push_back(Worklist.pop_back_val().first);
      continue;
    }
    Worklist.back().second = true;
    CASID ID = Worklist.back().first;
    Optional<ObjectKind> Kind = CAS.getObjectKind(ID);
    assert(Kind);

    if (*Kind == ObjectKind::Blob)
      continue;

    if (*Kind == ObjectKind::Tree) {
      TreeRef Tree = ExitOnErr(CAS.getTree(ID));
      ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
        push(Entry.getID());
        return Error::success();
      }));
      continue;
    }

    assert(*Kind == ObjectKind::Node);
    NodeRef Object = ExitOnErr(CAS.getNode(ID));
    ExitOnErr(Object.forEachReference([&](CASID ID) {
      push(ID);
      return Error::success();
    }));
  }

  return Info;
}

static void printDiffs(CASDB &CAS, const GraphInfo &Baseline,
                       const GraphInfo &New, StringRef NewName) {
  ExitOnError ExitOnErr("llvm-cas: diff-graphs: ");

  SmallString<128> PrintedID;
  for (cas::CASID ID : New.PostOrder) {
    if (Baseline.Seen.count(ID))
      continue;

    PrintedID.clear();
    raw_svector_ostream OS(PrintedID);
    ExitOnErr(CAS.printCASID(OS, ID));

    StringRef KindString;
    if (Optional<ObjectKind> Kind = CAS.getObjectKind(ID)) {
      switch (*Kind) {
      case ObjectKind::Blob:
        KindString = "blob";
        break;
      case ObjectKind::Tree:
        KindString = "tree";
        break;
      default:
        KindString = "node";
        break;
      }
    }

    outs() << llvm::formatv("{0}{1,-4} {2}\n", NewName, KindString, PrintedID);
  }
}

int diffGraphs(CASDB &CAS, CASID LHS, CASID RHS) {
  if (LHS == RHS)
    return 0;

  ExitOnError ExitOnErr("llvm-cas: diff-graphs: ");
  GraphInfo LHSInfo = traverseObjectGraph(CAS, LHS);
  GraphInfo RHSInfo = traverseObjectGraph(CAS, RHS);

  printDiffs(CAS, RHSInfo, LHSInfo, "- ");
  printDiffs(CAS, LHSInfo, RHSInfo, "+ ");
  return 0;
}

int traverseGraph(CASDB &CAS, CASID ID) {
  ExitOnError ExitOnErr("llvm-cas: traverse-graph: ");
  GraphInfo Info = traverseObjectGraph(CAS, ID);
  printDiffs(CAS, GraphInfo{}, Info, "");
  return 0;
}

static Error recursiveAccess(CachingOnDiskFileSystem &FS, StringRef Path) {
  auto ST = FS.status(Path);
  if (!ST)
    return errorCodeToError(ST.getError());

  if (ST->isDirectory()) {
    std::error_code EC;
    for (llvm::vfs::directory_iterator I = FS.dir_begin(Path, EC), IE;
         !EC && I != IE; I.increment(EC)) {
      auto Err = recursiveAccess(FS, I->path());
      if (Err)
        return Err;
    }
  }

  return Error::success();
}

int ingestFileSystem(CASDB &CAS, StringRef Path,
                     ArrayRef<MappedPrefix> Mappings) {
  ExitOnError ExitOnErr("llvm-cas: ingest: ");
  auto FS = createCachingOnDiskFileSystem(CAS);
  if (!FS)
    ExitOnErr(FS.takeError());

  (*FS)->trackNewAccesses();
  ExitOnErr(recursiveAccess(**FS, Path));

  BumpPtrAllocator Alloc;
  Optional<TreePathPrefixMapper> PM;
  if (!Mappings.empty()) {
    PM.emplace(*FS, Alloc);
    ExitOnErr(PM->addRange(Mappings));
    PM->sort();
  }

  auto Ref = (*FS)->createTreeFromNewAccesses(
      [&](StringRef Path) { return PM ? PM->mapOrOriginal(Path) : Path; });
  if (!Ref)
    ExitOnErr(Ref.takeError());

  ExitOnErr(CAS.printCASID(outs(), *Ref));
  return 0;
}

int getCASIDForFile(CASDB &CAS, CASID ID, StringRef Path) {
  ExitOnError ExitOnErr("llvm-cas: get-cas-id: ");
  auto FS = createCASFileSystem(CAS, ID);
  if (!FS)
    ExitOnErr(FS.takeError());

  auto FileID = (*FS)->getFileCASID(Path);
  if (!FileID)
    ExitOnErr(errorCodeToError(
        std::make_error_code(std::errc::no_such_file_or_directory)));

  ExitOnErr(CAS.printCASID(outs(), *FileID));
  return 0;
}
