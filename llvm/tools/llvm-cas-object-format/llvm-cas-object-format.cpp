//===- llvm-cas-object-format.cpp - Tool for the CAS object format --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/CAS/Utils.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/CASObjectFormats/FlatV1.h"
#include "llvm/CASObjectFormats/NestedV1.h"
#include "llvm/ExecutionEngine/JITLink/ELF_x86_64.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/MachO_x86_64.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GlobPattern.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casobjectformats;

#ifndef NDEBUG
// With assertions enabled do a check we get deterministic CASID for ingestion.
constexpr unsigned DefaultRepeats = 1;
#else
constexpr unsigned DefaultRepeats = 0;
#endif

cl::opt<int> NumThreads("num-threads", cl::desc("Num worker threads."));
cl::opt<bool> Dump("dump", cl::desc("Dump link-graph."));
cl::opt<bool>
    JustBlobs("just-blobs",
              cl::desc("Just ingest blobs, instead of breaking up files."));
cl::opt<std::string> CASIDOutput("casid-output",
                                 cl::desc("Write output as embedded CASID"));
cl::opt<bool> DebugIngest("debug-ingest",
                          cl::desc("Debug ingesting the object file."));
cl::opt<bool> SplitEHFrames("split-eh", cl::desc("Split eh-frame sections."),
                            cl::init(true));
cl::opt<std::string> CASPath("cas", cl::desc("Path to CAS on disk."));
cl::list<std::string> InputFiles(cl::Positional, cl::desc("Input object"));
cl::opt<bool> ComputeStats("object-stats",
                           cl::desc("Compute and print out stats."));
cl::opt<std::string> JustDsymutil("just-dsymutil",
                                  cl::desc("Just run dsymutil."));
cl::opt<std::string> CASGlob("cas-glob",
                             cl::desc("glob pattern for objects in CAS"));
cl::opt<bool> Silent("silent", cl::desc("only print final CAS ID"));
cl::opt<unsigned>
    NumRepeats("repeat",
               cl::desc("Repeat ingest action and check that we got same ID"),
               cl::init(DefaultRepeats));

cl::opt<std::string>
    IngestSchemaName("ingest-schema",
                     cl::desc("object file schema to ingest with"),
                     cl::init("nestedv1"));

enum InputKind {
  IngestFromFS,
  IngestFromCASTree,
  AnalysisCASTree,
};

cl::opt<InputKind> InputFileKind(
    cl::desc("choose input kind and action:"),
    cl::values(clEnumValN(IngestFromFS, "ingest-fs",
                          "ingest object files from file system (default)"),
               clEnumValN(IngestFromCASTree, "ingest-cas",
                          "ingest object files from cas tree"),
               clEnumValN(AnalysisCASTree, "analysis-only",
                          "analyze converted objects from cas tree")),
    cl::init(InputKind::IngestFromFS));

namespace {

class SharedStream {
public:
  SharedStream(raw_ostream &OS) : OS(OS) {}
  void applyLocked(llvm::function_ref<void(raw_ostream &OS)> Fn) {
    std::unique_lock<std::mutex> LockGuard(Lock);
    Fn(OS);
    OS.flush();
  }

private:
  std::mutex Lock;
  raw_ostream &OS;
};

} // namespace

static Expected<std::unique_ptr<SchemaBase>>
createSchema(CASDB &CAS, StringRef SchemaName) {
  if (SchemaName == "nestedv1")
    return std::make_unique<nestedv1::ObjectFileSchema>(CAS);
  if (SchemaName == "flatv1")
    return std::make_unique<flatv1::ObjectFileSchema>(CAS);
  return createStringError(inconvertibleErrorCode(),
                           "invalid schema '" + SchemaName + "'");
}

static CASID ingestFile(SchemaBase &Schema, StringRef InputFile,
                        MemoryBufferRef FileContent, SharedStream &OS);
static void computeStats(CASDB &CAS, ArrayRef<CASID> IDs);

int main(int argc, char *argv[]) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  cl::ParseCommandLineOptions(argc, argv);

  if (!CASIDOutput.empty() && InputFiles.size() != 1) {
    errs() << "error: '-casid-output' requires only one input";
    return 1;
  }

  std::unique_ptr<CASDB> CAS =
      StringRef(CASPath).empty()
          ? createInMemoryCAS()
          : ExitOnErr(createOnDiskCAS(CASPath == "default-on-disk"
                                          ? getDefaultOnDiskCASPath()
                                          : CASPath));

  ThreadPoolStrategy PoolStrategy = hardware_concurrency();
  if (NumThreads != 0)
    PoolStrategy.ThreadsRequested = NumThreads;
  ThreadPool Pool(PoolStrategy);

  StringMap<CASID> Files;
  SmallVector<CASID> SummaryIDs;
  SharedStream OS(outs());
  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);
  std::mutex Lock;

  std::unique_ptr<SchemaBase> IngestSchema =
      ExitOnErr(createSchema(*CAS, IngestSchemaName));
  for (StringRef IF : InputFiles) {
    ExitOnError ExitOnErr;
    ExitOnErr.setBanner(("llvm-cas-object-format: " + IF + ": ").str());

    switch (InputFileKind) {
    case AnalysisCASTree: {
      auto ID = ExitOnErr(CAS->parseCASID(IF));
      SummaryIDs.emplace_back(ID);
      break;
    }

    case IngestFromFS: {
      Pool.async([&, IF, ExitOnErr]() {
        auto ObjBuffer =
            ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(IF)));
        CASID ID =
            ingestFile(*IngestSchema, IF, ObjBuffer->getMemBufferRef(), OS);
        std::unique_lock<std::mutex> LockGuard(Lock);
        assert(!Files.count(IF));
        Files.try_emplace(IF, ID);
      });
      break;
    }

    case IngestFromCASTree: {
      auto ID = ExitOnErr(CAS->parseCASID(IF));
      SmallVector<NamedTreeEntry> Stack;
      Stack.emplace_back(ID, TreeEntry::Tree, "/");
      Optional<GlobPattern> GlobP;
      if (!CASGlob.empty())
        GlobP.emplace(ExitOnErr(GlobPattern::create(CASGlob)));

      while (!Stack.empty()) {
        auto Node = Stack.pop_back_val();
        auto Name = Node.getName();
        if (Node.getKind() == TreeEntry::Regular) {
          if (GlobP && !GlobP->match(Name))
              continue;

          auto BlobContent = ExitOnErr(CAS->getBlob(Node.getID()));
          Pool.async([&, Name, BlobContent]() {
            auto ObjBuffer =
                MemoryBuffer::getMemBuffer(BlobContent.getData(), Name);
            CASID ID = ingestFile(*IngestSchema, Name,
                                  ObjBuffer->getMemBufferRef(), OS);
            std::unique_lock<std::mutex> LockGuard(Lock);
            assert(!Files.count(Name));
            Files.try_emplace(Name, ID);
          });
          continue;
        }

        if (Node.getKind() != TreeEntry::Tree)
          ExitOnErr(createStringError(inconvertibleErrorCode(),
                                      "unexpected CAS kind in the tree"));

        TreeRef Tree = ExitOnErr(CAS->getTree(Node.getID()));
        ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
          SmallString<128> PathStorage = Node.getName();
          sys::path::append(PathStorage, sys::path::Style::posix,
                            Entry.getName());
          if (Entry.getKind() == TreeEntry::Regular) {
            if (GlobP && !GlobP->match(PathStorage))
              return Error::success();
          }
          Stack.emplace_back(Entry.getID(), Entry.getKind(),
                             Saver.save(StringRef(PathStorage)));
          return Error::success();
        }));
      }
      break;
    }
    }
  }
  Pool.wait();

  if (!CASIDOutput.empty()) {
    assert(Files.size() == 1);
    auto File = Files.begin();
    SmallVector<char, 50> Contents;
    {
      raw_svector_ostream OS(Contents);
      writeCASIDBuffer(*CAS, File->second, OS);
    }
    std::unique_ptr<FileOutputBuffer> outBuf =
        ExitOnErr(FileOutputBuffer::create(CASIDOutput, Contents.size()));
    llvm::copy(Contents, outBuf->getBufferStart());
    ExitOnErr(outBuf->commit());
    return 0;
  }

#define MSG(E)                                                                 \
  if (!Silent) {                                                               \
    outs() << E;                                                               \
  }
  MSG("\n");
  if (!Files.empty()) {
    MSG("Making summary object...\n");
    HierarchicalTreeBuilder Builder;
    for (auto &File : Files)
      Builder.push(File.second, TreeEntry::Regular, File.first());
    CASID SummaryID = ExitOnErr(Builder.create(*CAS));
    SummaryIDs.emplace_back(SummaryID);
    MSG("summary tree: ");
    ExitOnErr(CAS->printCASID(outs(), SummaryID));
    MSG("\n");
  }

  if (ComputeStats)
    computeStats(*CAS, SummaryIDs);

  return 0;
}

namespace {
struct NodeInfo {
  size_t NumPaths = 0;
  size_t NumParents = 0;
  bool Done = false;
};

struct POTItem {
  CASID ID;
  const SchemaBase *Schema = nullptr;
};

struct ObjectKindInfo {
  size_t Count = 0;
  size_t NumPaths = 0;
  size_t NumChildren = 0;
  size_t NumParents = 0;
  size_t DataSize = 0;

  size_t getTotalSize(size_t NumHashBytes) const {
    return NumChildren * NumHashBytes + DataSize;
  }
};

struct StatCollector {
  CASDB &CAS;

  using POTItemHandler = unique_function<void(
      ExitOnError &, function_ref<void(ObjectKindInfo &)>, cas::NodeRef)>;

  // FIXME: Utilize \p SchemaPool.
  nestedv1::ObjectFileSchema NestedV1Schema;
  flatv1::ObjectFileSchema FlatV1Schema;
  SmallVector<std::pair<const SchemaBase *, POTItemHandler>> Schemas;

  StatCollector(CASDB &CAS)
      : CAS(CAS), NestedV1Schema(CAS), FlatV1Schema(CAS) {
    Schemas.push_back(std::make_pair(
        &NestedV1Schema,
        [&](ExitOnError &ExitOnErr,
            function_ref<void(ObjectKindInfo & Info)> addNodeStats,
            cas::NodeRef Node) {
          visitPOTItemNestedV1(ExitOnErr, NestedV1Schema, addNodeStats, Node);
        }));
    Schemas.push_back(std::make_pair(
        &FlatV1Schema,
        [&](ExitOnError &ExitOnErr,
            function_ref<void(ObjectKindInfo & Info)> addNodeStats,
            cas::NodeRef Node) {
          visitPOTItemFlatV1(ExitOnErr, FlatV1Schema, addNodeStats, Node);
        }));
  }

  DenseMap<CASID, NodeInfo> Nodes;

  StringMap<ObjectKindInfo> Stats;
  ObjectKindInfo Totals;
  DenseSet<StringRef> GeneratedNames;
  DenseSet<cas::CASID> SectionNames;
  DenseSet<cas::CASID> SymbolNames;
  DenseSet<cas::CASID> UndefinedSymbols;
  DenseSet<cas::CASID> ContentBlobs;
  size_t NumAnonymousSymbols = 0;
  size_t NumTemplateSymbols = 0;
  size_t NumTemplateTargets = 0;
  size_t NumZeroFillBlocks = 0;
  size_t Num1TargetBlocks = 0;
  size_t Num2TargetBlocks = 0;

  void visitPOT(ExitOnError &ExitOnErr, ArrayRef<CASID> TopLevels,
                ArrayRef<POTItem> POT);
  void visitPOTItem(ExitOnError &ExitOnErr, const POTItem &Item);
  void visitPOTItemNestedV1(
      ExitOnError &ExitOnErr, nestedv1::ObjectFileSchema &Schema,
      function_ref<void(ObjectKindInfo &Info)> addNodeStats, cas::NodeRef Node);
  void visitPOTItemFlatV1(
      ExitOnError &ExitOnErr, flatv1::ObjectFileSchema &Schema,
      function_ref<void(ObjectKindInfo &Info)> addNodeStats, cas::NodeRef Node);
  void printToOuts(ArrayRef<CASID> TopLevels);
};
} // end namespace

void StatCollector::visitPOT(ExitOnError &ExitOnErr, ArrayRef<CASID> TopLevels,
                             ArrayRef<POTItem> POT) {
  for (auto ID : TopLevels)
    Nodes[ID].NumPaths = 1;

  // Visit POT in reverse (topological sort), computing Nodes and collecting
  // stats.
  for (const POTItem &Item : llvm::reverse(POT))
    visitPOTItem(ExitOnErr, Item);
}

void StatCollector::visitPOTItem(ExitOnError &ExitOnErr, const POTItem &Item) {
  cas::CASID ID = Item.ID;
  size_t NumPaths = Nodes.lookup(ID).NumPaths;
  Optional<ObjectKind> Kind = CAS.getObjectKind(ID);
  assert(Kind);

  auto updateChild = [&](CASID Child) {
    ++Nodes[Child].NumParents;
    Nodes[Child].NumPaths += NumPaths;
  };

  size_t NumParents = Nodes.lookup(ID).NumParents;
  if (*Kind == ObjectKind::Blob && Item.Schema)
    Kind = ObjectKind::Node; // Hack because nodes with no references look
                             // like blobs.
  if (*Kind == ObjectKind::Blob) {
    auto &Info = Stats["builtin:blob"];
    ++Info.Count;
    Info.DataSize += ExitOnErr(CAS.getBlob(ID)).getData().size();
    Info.NumPaths += NumPaths;
    Info.NumParents += NumParents;
    Totals.NumPaths += NumPaths; // Count paths to leafs.
    return;
  }

  if (*Kind == ObjectKind::Tree) {
    auto &Info = Stats["builtin:tree"];
    ++Info.Count;
    TreeRef Tree = ExitOnErr(CAS.getTree(ID));
    Info.NumChildren += Tree.size();
    Info.NumParents += NumParents;
    Info.NumPaths += NumPaths;
    if (!Tree.size())
      Totals.NumPaths += NumPaths; // Count paths to leafs.

    ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
      // FIXME: This is copied out of BuiltinCAS.cpp's makeTree.
      Info.DataSize += sizeof(uint64_t) + sizeof(uint32_t) +
                       alignTo(Entry.getName().size() + 1, Align(4));
      updateChild(Entry.getID());
      return Error::success();
    }));
    return;
  }

  assert(*Kind == ObjectKind::Node);
  cas::NodeRef Node = ExitOnErr(CAS.getNode(ID));
  auto addNodeStats = [&](ObjectKindInfo &Info) {
    ++Info.Count;
    Info.NumChildren += Node.getNumReferences();
    Info.NumParents += NumParents;
    Info.NumPaths += NumPaths;
    Info.DataSize += Node.getData().size();
    if (!Node.getNumReferences())
      Totals.NumPaths += NumPaths; // Count paths to leafs.

    ExitOnErr(Node.forEachReference([&](CASID ChildID) {
      updateChild(ChildID);
      return Error::success();
    }));
  };

  // Handle nodes not in the schema.
  if (!Item.Schema) {
    addNodeStats(Stats["builtin:node"]);
    return;
  }

  for (auto &S : Schemas) {
    if (Item.Schema != S.first)
      continue;
    S.second(ExitOnErr, addNodeStats, Node);
    return;
  }
  llvm_unreachable("schema not found");
}

void StatCollector::visitPOTItemNestedV1(
    ExitOnError &ExitOnErr, nestedv1::ObjectFileSchema &Schema,
    function_ref<void(ObjectKindInfo &Info)> addNodeStats, cas::NodeRef Node) {
  using namespace llvm::casobjectformats::nestedv1;
  ObjectFormatNodeRef Object = ExitOnErr(Schema.getNode(Node));
  addNodeStats(Stats[Object.getKindString()]);

  // Check specific stats.
  if (Object.getKindString() == BlockRef::KindString) {
    if (Optional<BlockRef> Block = expectedToOptional(BlockRef::get(Object))) {
      if (Optional<TargetList> Targets =
              expectedToOptional(Block->getTargets())) {
        for (size_t I = 0, E = Targets->size(); I != E; ++I) {
          if (Optional<TargetRef> Target =
                  expectedToOptional(Targets->get(I))) {
            if (Optional<StringRef> Name =
                    expectedToOptional(Target->getNameString()))
              if (Name->startswith("cas.o:"))
                GeneratedNames.insert(*Name);

            if (Target->getKind() == TargetRef::Symbol)
              if (Optional<SymbolRef> Symbol =
                      expectedToOptional(SymbolRef::get(Schema, *Target)))
                NumTemplateTargets += Symbol->isSymbolTemplate();
          }
        }
        Num1TargetBlocks += Targets->size() == 1;
        Num2TargetBlocks += Targets->size() == 2;
      }
      if (Optional<BlockDataRef> Data =
              expectedToOptional(Block->getBlockData())) {
        if (Data->isZeroFill())
          ++NumZeroFillBlocks;
      }
    }
  }
  if (Object.getKindString() == SymbolRef::KindString) {
    if (Optional<SymbolRef> Symbol =
            expectedToOptional(SymbolRef::get(Object))) {
      NumAnonymousSymbols += !Symbol->hasName();
      NumTemplateSymbols += Symbol->isSymbolTemplate();
      if (Optional<cas::CASID> Name = Symbol->getNameID()) {
        SymbolNames.insert(*Name);
        UndefinedSymbols.erase(*Name);
      }
    }
  }
  if (Object.getKindString() == NameListRef::KindString) {
    // FIXME: This is only valid because NameList is currently just used for
    // lists of symbols.
    if (Optional<NameListRef> List =
            expectedToOptional(NameListRef::get(Object))) {
      for (size_t I = 0, E = List->getNumNames(); I != E; ++I) {
        cas::CASID Name = List->getNameID(I);
        if (!SymbolNames.count(Name))
          UndefinedSymbols.insert(Name);
      }
    }
  }
  if (Object.getKindString() == SectionRef::KindString) {
    if (Optional<SectionRef> Section =
            expectedToOptional(SectionRef::get(Object))) {
      if (Optional<cas::CASID> Name = Section->getNameID())
        SectionNames.insert(*Name);
    }
  }
}

void StatCollector::visitPOTItemFlatV1(
    ExitOnError &ExitOnErr, flatv1::ObjectFileSchema &Schema,
    function_ref<void(ObjectKindInfo &Info)> addNodeStats, cas::NodeRef Node) {
  using namespace llvm::casobjectformats::flatv1;
  ObjectFormatNodeRef Object = ExitOnErr(Schema.getNode(Node));
  addNodeStats(Stats[Object.getKindString()]);
}

static void computeStats(CASDB &CAS, ArrayRef<CASID> TopLevels) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner("llvm-cas-object-format: compute-stats: ");

  outs() << "Collecting object stats...\n";

  // In the first traversal, just collect a POT. Use NumPaths as a "Seen" list.
  StatCollector Collector(CAS);
  auto &Nodes = Collector.Nodes;
  struct WorklistItem {
    CASID ID;
    bool Visited;
    StringRef Path;
    const SchemaBase *Schema = nullptr;
  };
  SmallVector<WorklistItem> Worklist;
  SmallVector<POTItem> POT;
  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);
  Optional<GlobPattern> GlobP;
  if (!CASGlob.empty())
    GlobP.emplace(ExitOnErr(GlobPattern::create(CASGlob)));

  auto push = [&](CASID ID, StringRef Path,
                  const SchemaBase *Schema = nullptr) {
    auto &Node = Nodes[ID];
    if (!Node.Done)
      Worklist.push_back({ID, false, Path, Schema});
  };
  for (auto ID : TopLevels)
    push(ID, "/");
  while (!Worklist.empty()) {
    CASID ID = Worklist.back().ID;
    auto Name = Worklist.back().Path;
    if (Worklist.back().Visited) {
      Nodes[ID].Done = true;
      POT.push_back({ID, Worklist.back().Schema});
      Worklist.pop_back();
      continue;
    }
    if (Nodes.lookup(ID).Done) {
      Worklist.pop_back();
      continue;
    }

    Worklist.back().Visited = true;

    // FIXME: Maybe this should just assert?
    Optional<ObjectKind> Kind = CAS.getObjectKind(ID);
    assert(Kind);
    if (!Kind) {
      Worklist.pop_back();
      continue;
    }

    if (*Kind == ObjectKind::Blob)
      continue;

    if (*Kind == ObjectKind::Tree) {
      TreeRef Tree = ExitOnErr(CAS.getTree(ID));
      ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
        SmallString<128> PathStorage = Name;
        sys::path::append(PathStorage, sys::path::Style::posix,
                          Entry.getName());
        if (Entry.getKind() == TreeEntry::Regular) {
          if (GlobP && !GlobP->match(PathStorage))
            return Error::success();
        }
        push(Entry.getID(), Saver.save(StringRef(PathStorage)));
        return Error::success();
      }));
      continue;
    }

    assert(*Kind == ObjectKind::Node);
    NodeRef Node = ExitOnErr(CAS.getNode(ID));
    const SchemaBase *&Schema = Worklist.back().Schema;

    // Update the schema.
    if (!Schema) {
      for (auto &S : Collector.Schemas)
        if (S.first->isRootNode(Node))
          Schema = S.first;
    } else if (!Schema->isNode(Node)) {
      Schema = nullptr;
    }

    ExitOnErr(Node.forEachReference([&, Schema](CASID ChildID) {
      push(ChildID, "", Schema);
      return Error::success();
    }));
  }

  Collector.visitPOT(ExitOnErr, TopLevels, POT);
  Collector.printToOuts(TopLevels);
}

void StatCollector::printToOuts(ArrayRef<CASID> TopLevels) {
  SmallVector<StringRef> Kinds;
  auto addToTotal = [&Totals = this->Totals](ObjectKindInfo Info) {
    Totals.Count += Info.Count;
    Totals.NumChildren += Info.NumChildren;
    Totals.NumParents += Info.NumParents;
    Totals.DataSize += Info.DataSize;
  };
  for (auto &I : Stats) {
    Kinds.push_back(I.first());
    addToTotal(I.second);
  }
  llvm::sort(Kinds);

  size_t NumHashBytes = TopLevels.front().getHash().size();
  outs() << "  => Note: 'Parents' counts incoming edges\n"
         << "  => Note: 'Children' counts outgoing edges (to sub-objects)\n"
         << "  => Note: number of bytes per Ref = " << NumHashBytes << "\n"
         << "  => Note: Cost = sizeof(Ref)*Children + Data\n";
  StringLiteral HeaderFormat = "{0,-22} {1,+10} {2,+7} {3,+10} {4,+7} {5,+10} "
                               "{6,+7} {7,+10} {8,+7} {9,+10} {10,+7}\n";
  StringLiteral Format = "{0,-22} {1,+10} {2,+7:P} {3,+10} {4,+7:P} {5,+10} "
                         "{6,+7:P} {7,+10} {8,+7:P} {9,+10} {10,+7:P}\n";
  outs() << llvm::formatv(HeaderFormat.begin(), "Kind", "Count", "", "Parents",
                          "", "Children", "", "Data (B)", "", "Cost (B)", "");
  outs() << llvm::formatv(HeaderFormat.begin(), "====", "=====", "",
                          "=======", "", "========", "", "========", "",
                          "========", "");
  auto printInfo = [Format, NumHashBytes, &Totals = this->Totals](
                       StringRef Kind, ObjectKindInfo Info) {
    if (!Info.Count)
      return;
    auto getPercent = [](double N, double D) { return D ? N / D : 0.0; };
    size_t Size = Info.getTotalSize(NumHashBytes);
    outs() << llvm::formatv(
        Format.begin(), Kind, Info.Count, getPercent(Info.Count, Totals.Count),
        Info.NumParents, getPercent(Info.NumParents, Totals.NumParents),
        Info.NumChildren, getPercent(Info.NumChildren, Totals.NumChildren),
        Info.DataSize, getPercent(Info.DataSize, Totals.DataSize), Size,
        getPercent(Size, Totals.getTotalSize(NumHashBytes)));
  };
  for (StringRef Kind : Kinds)
    printInfo(Kind, Stats.lookup(Kind));
  printInfo("TOTAL", Totals);

  // Other stats.
  bool HasPrinted = false;
  auto printIfNotZero = [&HasPrinted](StringRef Name, size_t Num) {
    if (!Num)
      return;
    if (!HasPrinted)
      outs() << "\n";
    outs() << llvm::formatv("{0,-22} {1,+10}\n", Name, Num);
    HasPrinted = true;
  };

  printIfNotZero("num-generated-names", GeneratedNames.size());
  printIfNotZero("num-section-names", SectionNames.size());
  printIfNotZero("num-symbol-names",
                 SymbolNames.size() + UndefinedSymbols.size());
  printIfNotZero("num-undefined-symbols", UndefinedSymbols.size());
  printIfNotZero("num-anonymous-symbols", NumAnonymousSymbols);
  printIfNotZero("num-template-symbols", NumTemplateSymbols);
  printIfNotZero("num-template-targets", NumTemplateTargets);
  printIfNotZero("num-zero-fill-blocks", NumZeroFillBlocks);
  printIfNotZero("num-1-target-blocks", Num2TargetBlocks);
  printIfNotZero("num-2-target-blocks", Num1TargetBlocks);
}

static void dumpGraph(jitlink::LinkGraph &G, SharedStream &OS, StringRef Desc) {
  SmallString<1024> Data;
  raw_svector_ostream Dump(Data);
  G.dump(Dump);
  OS.applyLocked([&](raw_ostream &OS) { OS << Desc << ":\n" << Data; });
}

/// LinkGraph creates an anonymous symbol that covers the full EH frame section
/// block and no edge points to it. The EH frame splitter may end up creating an
/// identical symbol (that has an edge pointed to it) which can result in
/// non-deterministic behavior in \p FlatV1 schema ingestion, because the
/// symbols cannot be ordered in a deterministic manner. \p
/// removeRedundantEHFrameSymbol() removes this redundant symbol before we run
/// the EH frame splitter.
/// FIXME: Should this be taken care of by the EH frame splitter?
static void removeRedundantEHFrameSymbol(jitlink::LinkGraph &G) {
  // FIXME: Consider implementing removeRedundantEHFrameSymbol as a jitlink
  // pass. Note this section name is machO specific.
  auto *EHFrame = G.findSectionByName("__TEXT,__eh_frame");
  if (!EHFrame)
    return;
  for (jitlink::Symbol *Sym : G.defined_symbols()) {
    if (&Sym->getBlock().getSection() == EHFrame) {
      G.removeDefinedSymbol(*Sym);
      break;
    }
  }
}

static Error createSplitEHFramePasses(jitlink::LinkGraph &G) {
  if (G.getTargetTriple().getObjectFormat() == Triple::MachO &&
      G.getTargetTriple().getArch() == Triple::x86_64) {
    removeRedundantEHFrameSymbol(G);
    if (auto E = jitlink::createEHFrameSplitterPass_MachO_x86_64()(G))
      return E;
    if (auto E = jitlink::createEHFrameEdgeFixerPass_MachO_x86_64()(G))
      return E;
  } else if (G.getTargetTriple().getObjectFormat() == Triple::ELF &&
             G.getTargetTriple().getArch() == Triple::x86_64) {
    // FIXME: Is removeRedundantEHFrameSymbol() necessary for ELF as well?
    if (auto E = jitlink::createEHFrameSplitterPass_ELF_x86_64()(G))
      return E;
    if (auto E = jitlink::createEHFrameEdgeFixerPass_ELF_x86_64()(G))
      return E;
  } else
    dbgs() << "Note: not fixing eh-frame sections\n";

  return Error::success();
}

static CASID ingestFile(SchemaBase &Schema, StringRef InputFile,
                        MemoryBufferRef FileContent, SharedStream &OS) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(("llvm-cas-object-format: " + InputFile + ": ").str());

  auto EmitDotOnReturn = llvm::make_scope_exit([&]() {
    if (!Silent)
      OS.applyLocked([&](raw_ostream &OS) { OS << "."; });
  });

  auto &CAS = Schema.CAS;
  if (JustBlobs)
    return ExitOnErr(CAS.createBlob(FileContent.getBuffer()));

  auto createLinkGraph =
      [&ExitOnErr](
          MemoryBufferRef FileContent) -> std::unique_ptr<jitlink::LinkGraph> {
    auto G = ExitOnErr(jitlink::createLinkGraphFromObject(FileContent));

    if (SplitEHFrames)
      ExitOnErr(createSplitEHFramePasses(*G));
    return G;
  };

  auto G = createLinkGraph(FileContent);

  if (!JustDsymutil.empty()) {
    // Create a map for each symbol in TEXT.
    jitlink::Section *Text = G->findSectionByName("__TEXT,__text");
    if (!Text)
      return ExitOnErr(CAS.createTree());

    auto MapFile = ExitOnErr(sys::fs::TempFile::create("/tmp/debug-%%%%%%%%.map"));
    auto DsymFile =
        ExitOnErr(sys::fs::TempFile::create(MapFile.TmpName + ".dwarf"));
    Optional<StringRef> Redirects[] = {
        StringRef(),
        None,
        None,
    };

    DenseSet<jitlink::Block *> Seen;
    HierarchicalTreeBuilder Builder;
    for (jitlink::Symbol *S : Text->symbols()) {
      if (!S->isDefined())
        continue;

      assert(S->hasName() && "Expected __TEXT,__text symbols to have names");
      jitlink::Block *B = &S->getBlock();
      if (!Seen.insert(B).second)
        continue;

      // Write map.
      {
        std::error_code EC;
        raw_fd_ostream OS(MapFile.TmpName, EC);
        ExitOnErr(errorCodeToError(EC));
        OS << "---\n";
        OS << "triple:          'x86_64-apple-darwin'\n";
        OS << "objects:\n";
        OS << "  - filename: " << InputFile << "\n";
        OS << "    symbols:\n";
        OS << "      - { sym: " << S->getName()
           << ", objAddr: 0x0, binAddr: 0x0, size: "
           << formatv("{0:x}", B->getSize()) << " }\n";
        OS << "...\n";
      }

      // Call dsymutil.
      StringRef Args[] = {
          JustDsymutil,
          "-y",
          "--accelerator",
          "None",
          "--flat",
          "--num-threads",
          "1",
          "-o",
          DsymFile.TmpName,
          MapFile.TmpName,
      };

      if (sys::ExecuteAndWait(JustDsymutil, Args, None, Redirects))
        ExitOnErr(
            createStringError(inconvertibleErrorCode(), "dsymutil failed"));

      // Read dsym.
      auto DwarfBuffer =
          ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(DsymFile.TmpName)));
      Builder.push(ExitOnErr(CAS.createBlob(DwarfBuffer->getBuffer())),
                   TreeEntry::Regular, S->getName());
    }
    ExitOnErr(MapFile.discard());
    ExitOnErr(DsymFile.discard());
    return ExitOnErr(Builder.create(CAS));
  }

  if (Dump)
    dumpGraph(*G, OS, "parse result");

  SmallString<256> DebugIngestOutput;
  Optional<raw_svector_ostream> DebugOS;
  if (DebugIngest)
    DebugOS.emplace(DebugIngestOutput);
  auto CompileUnit = ExitOnErr(
      Schema.createFromLinkGraph(*G, DebugIngest ? &*DebugOS : nullptr));
  for (unsigned I = 0; I != NumRepeats; ++I) {
    auto newG = createLinkGraph(FileContent);
    auto NewCompileUnit = ExitOnErr(Schema.createFromLinkGraph(*newG));
    if (NewCompileUnit.getID() != CompileUnit.getID()) {
      errs() << "error: got different CASID while repeating ingestion, ";
      ExitOnErr(CAS.printCASID(errs(), CompileUnit));
      errs() << " and ";
      ExitOnErr(CAS.printCASID(errs(), NewCompileUnit));
      exit(1);
    }
  }

  if (DebugIngest)
    OS.applyLocked([&](raw_ostream &OS) { OS << DebugIngestOutput; });

  auto RoundTripG = ExitOnErr(Schema.createLinkGraph(
      CompileUnit, FileContent.getBufferIdentifier(),
      ExitOnErr(jitlink::getGetEdgeKindNameFunction(G->getTargetTriple()))));

  if (Dump)
    dumpGraph(*RoundTripG, OS, "after round-trip");

  return CompileUnit;
}
