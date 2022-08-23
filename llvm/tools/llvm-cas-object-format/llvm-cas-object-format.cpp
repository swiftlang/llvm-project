//===- llvm-cas-object-format.cpp - Tool for the CAS object format --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/CAS/TreeSchema.h"
#include "llvm/CAS/Utils.h"
#include "llvm/CASObjectFormats/CASObjectReader.h"
#include "llvm/CASObjectFormats/FlatV1.h"
#include "llvm/CASObjectFormats/LinkGraph.h"
#include "llvm/CASObjectFormats/NestedV1.h"
#include "llvm/CASObjectFormats/Utils.h"
#include "llvm/ExecutionEngine/JITLink/ELF_x86_64.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/MachO.h"
#include "llvm/ExecutionEngine/JITLink/MachO_x86_64.h"
#include "llvm/MC/CAS/MCCASObjectV1.h"
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
#include <memory>

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casobjectformats;

#ifndef NDEBUG
    // With assertions enabled do a check we get deterministic CASID for
    // ingestion.
    constexpr unsigned DefaultRepeats = 1;
#else
    constexpr unsigned DefaultRepeats = 0;
#endif

cl::opt<int> NumThreads("num-threads", cl::desc("Num worker threads."));
cl::opt<bool> Dump("dump", cl::desc("Dump link-graph."));
cl::opt<bool>
    JustBlobs("just-blobs",
              cl::desc("Just ingest blobs, instead of breaking up files."));
cl::opt<bool> CASIDFile("casid-file",
                        cl::desc("Input is CASID file, just ingest CASID."));
cl::opt<std::string> CASIDOutput("casid-output",
                                 cl::desc("Write output as embedded CASID"));
cl::opt<bool> DebugIngest("debug-ingest",
                          cl::desc("Debug ingesting the object file."));
cl::opt<bool> SplitEHFrames("split-eh", cl::desc("Split eh-frame sections."),
                            cl::init(true));
cl::opt<std::string> CASPath("cas", cl::desc("Path to CAS on disk."));
cl::list<std::string> InputFiles(cl::Positional, cl::desc("Input object"));
cl::opt<std::string>
    ComputeStats("object-stats",
                 cl::desc("Compute and print out stats. Use '-' to print to "
                          "stdout, otherwise provide path"));
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
cl::opt<std::string> OutputPrefix("output-prefix",
                                  cl::desc("output object file prefix"),
                                  cl::init("."));

enum InputKind {
  IngestFromFS,
  IngestFromCASTree,
  AnalysisCASTree,
  PrintCASTree,
  PrintCASObject,
  MaterializeObjects,
};

cl::opt<InputKind> InputFileKind(
    cl::desc("choose input kind and action:"),
    cl::values(clEnumValN(IngestFromFS, "ingest-fs",
                          "ingest object files from file system (default)"),
               clEnumValN(IngestFromCASTree, "ingest-cas",
                          "ingest object files from cas tree"),
               clEnumValN(AnalysisCASTree, "analysis-only",
                          "analyze converted objects from cas tree"),
               clEnumValN(PrintCASTree, "print-cas-tree",
                          "print cas object from cas tree ID"),
               clEnumValN(MaterializeObjects, "materialize-objects",
                          "materialize objects from CAS tree"),
               clEnumValN(PrintCASObject, "print-cas-object",
                          "print cas object from cas object ID")),
    cl::init(InputKind::IngestFromFS));

cl::opt<bool> omitCASID(
    "omit-cas-id",
    cl::desc(
        "Don't print cas ID when using print-cas-object or print-cas-tree"));

enum FormatType {
  Pretty,
  CSV,
};

cl::opt<FormatType> ObjectStatsFormat(
    "object-stats-format", cl::desc("choose object stats format:"),
    cl::values(
        clEnumValN(Pretty, "pretty",
                   "object stats formatted in a readable format (default)"),
        clEnumValN(CSV, "csv", "object stats formatted in a CSV format")),
    cl::init(FormatType::Pretty));

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

static Expected<std::unique_ptr<ObjectFormatSchemaBase>>
createSchema(CASDB &CAS, StringRef SchemaName) {
  if (SchemaName == "nestedv1")
    return std::make_unique<nestedv1::ObjectFileSchema>(CAS);
  if (SchemaName == "flatv1")
    return std::make_unique<flatv1::ObjectFileSchema>(CAS);
  return createStringError(inconvertibleErrorCode(),
                           "invalid schema '" + SchemaName + "'");
}

static ObjectRef ingestFile(ObjectFormatSchemaBase &Schema, StringRef InputFile,
                            MemoryBufferRef FileContent, SharedStream &OS);
static void computeStats(CASDB &CAS, ArrayRef<ObjectProxy> IDs,
                         raw_ostream &StatOS);
static Error printCASObjectOrTree(ObjectFormatSchemaPool &Pool, ObjectProxy ID,
                                  bool omitCASID);
static Error printCASObject(ObjectFormatSchemaPool &Pool, ObjectProxy ID,
                            bool omitCASID);
static Error materializeObjectsFromCASTree(CASDB &CAS, ObjectProxy ID);

int main(int argc, char *argv[]) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  cl::ParseCommandLineOptions(argc, argv);

  if (!CASIDOutput.empty() && InputFiles.size() != 1) {
    errs() << "error: '-casid-output' requires only one input";
    return 1;
  }

  std::unique_ptr<CASDB> CAS;
  if (StringRef(CASPath).empty())
    CAS = createInMemoryCAS();
  else if (CASPath == "auto")
    CAS = ExitOnErr(createOnDiskCAS(getDefaultOnDiskCASPath()));
  else
    CAS = ExitOnErr(createOnDiskCAS(CASPath));

  ThreadPoolStrategy PoolStrategy = hardware_concurrency();
  if (NumThreads != 0)
    PoolStrategy.ThreadsRequested = NumThreads;
  ThreadPool Pool(PoolStrategy);

  ObjectFormatSchemaPool SchemaPool(*CAS);
  StringMap<ObjectRef> Files;
  SmallVector<ObjectProxy> SummaryIDs;
  SharedStream OS(outs());
  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);
  std::mutex Lock;

  std::unique_ptr<ObjectFormatSchemaBase> IngestSchema =
      ExitOnErr(createSchema(*CAS, IngestSchemaName));
  for (StringRef IF : InputFiles) {
    ExitOnError ExitOnErr;
    ExitOnErr.setBanner(("llvm-cas-object-format: " + IF + ": ").str());

    switch (InputFileKind) {
    case AnalysisCASTree: {
      auto ID = ExitOnErr(CAS->parseID(IF));
      auto Proxy = ExitOnErr(CAS->getProxy(ID));
      SummaryIDs.emplace_back(Proxy);
      break;
    }

    case PrintCASTree: {
      auto ID = ExitOnErr(CAS->parseID(IF));
      auto Proxy = ExitOnErr(CAS->getProxy(ID));
      ExitOnErr(printCASObjectOrTree(SchemaPool, Proxy, omitCASID));
      break;
    }

    case PrintCASObject: {
      auto ID = ExitOnErr(CAS->parseID(IF));
      auto Proxy = ExitOnErr(CAS->getProxy(ID));
      ExitOnErr(printCASObject(SchemaPool, Proxy, omitCASID));
      break;
    }

    case MaterializeObjects: {
      auto ID = ExitOnErr(CAS->parseID(IF));
      auto Proxy = ExitOnErr(CAS->getProxy(ID));
      ExitOnErr(materializeObjectsFromCASTree(*CAS, Proxy));
      return 0;
    }

    case IngestFromFS: {
      Pool.async([&, IF, ExitOnErr]() {
        auto ObjBuffer =
            ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(IF)));
        ObjectRef ID =
            ingestFile(*IngestSchema, IF, ObjBuffer->getMemBufferRef(), OS);
        std::unique_lock<std::mutex> LockGuard(Lock);
        assert(!Files.count(IF));
        Files.try_emplace(IF, ID);
      });
      break;
    }

    case IngestFromCASTree: {
      auto ID = ExitOnErr(CAS->parseID(IF));
      auto Root = ExitOnErr(CAS->getProxy(ID));
      TreeSchema Schema(*CAS);
      SmallVector<NamedTreeEntry> Stack;
      Stack.emplace_back(CAS->getReference(Root), TreeEntry::Tree, "/");
      Optional<GlobPattern> GlobP;
      if (!CASGlob.empty())
        GlobP.emplace(ExitOnErr(GlobPattern::create(CASGlob)));

      while (!Stack.empty()) {
        auto Node = Stack.pop_back_val();
        auto Name = Node.getName();
        if (Node.getKind() == TreeEntry::Regular) {
          if (GlobP && !GlobP->match(Name))
              continue;

          auto BlobContent = ExitOnErr(CAS->getProxy(Node.getRef()));
          Pool.async([&, Name, BlobContent]() {
            auto ObjBuffer =
                MemoryBuffer::getMemBuffer(BlobContent.getData(), Name);
            ObjectRef ID = ingestFile(*IngestSchema, Name,
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

        ObjectProxy TreeN = ExitOnErr(CAS->getProxy(Node.getRef()));
        TreeProxy Tree = ExitOnErr(Schema.load(TreeN));
        ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
          SmallString<128> PathStorage = Node.getName();
          sys::path::append(PathStorage, sys::path::Style::posix,
                            Entry.getName());
          if (Entry.getKind() == TreeEntry::Regular) {
            if (GlobP && !GlobP->match(PathStorage))
              return Error::success();
          }
          Stack.emplace_back(Entry.getRef(), Entry.getKind(),
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
      writeCASIDBuffer(CAS->getID(File->second), OS);
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
    for (auto &File : Files) {
      Builder.push(File.second, TreeEntry::Regular, File.first());
    }
    ObjectProxy SummaryRef = ExitOnErr(CAS->getProxy(Builder.create(*CAS)));
    SummaryIDs.emplace_back(SummaryRef);
    MSG("summary tree: ");
    outs() << CAS->getID(SummaryRef) << "\n";
  }

  if (ComputeStats.empty() || SummaryIDs.empty()) {
    return 0;
  }

  bool PrintToStdout = false;
  if (StringRef(ComputeStats) == "-")
    PrintToStdout = true;

  raw_ostream *StatOS = &outs();
  Optional<raw_fd_ostream> StatsFile;

  if (!PrintToStdout) {
    std::error_code EC;
    StatsFile.emplace(ComputeStats, EC, sys::fs::OF_None);
    ExitOnErr(errorCodeToError(EC));
    StatOS = &*StatsFile;
  }

  computeStats(*CAS, SummaryIDs, *StatOS);

  return 0;
}

namespace {
struct NodeInfo {
  size_t NumPaths = 0;
  size_t NumParents = 0;
  bool Done = false;
};

struct POTItem {
  ObjectRef ID;
  const NodeSchema *Schema = nullptr;
};

struct ObjectKindInfo {
  size_t Count = 0;
  size_t NumPaths = 0;
  size_t NumChildren = 0;
  size_t NumParents = 0;
  size_t DataSize = 0;

  size_t getTotalSize(size_t NumHashBytes) const {
    return Count * NumHashBytes + NumChildren * sizeof(void*) + DataSize;
  }
};

struct StatCollector {
  CASDB &CAS;

  using POTItemHandler = unique_function<void(
      ExitOnError &, function_ref<void(ObjectKindInfo &)>, cas::ObjectProxy)>;

  // FIXME: Utilize \p SchemaPool.
  nestedv1::ObjectFileSchema NestedV1Schema;
  flatv1::ObjectFileSchema FlatV1Schema;
  llvm::mccasformats::v1::MCSchema MCCASV1Schema;
  SmallVector<std::pair<const NodeSchema*, POTItemHandler>>
      Schemas;

  StatCollector(CASDB &CAS)
      : CAS(CAS), NestedV1Schema(CAS), FlatV1Schema(CAS), MCCASV1Schema(CAS) {
    Schemas.push_back(std::make_pair(
        &NestedV1Schema,
        [&](ExitOnError &ExitOnErr,
            function_ref<void(ObjectKindInfo & Info)> addNodeStats,
            cas::ObjectProxy Node) {
          visitPOTItemNestedV1(ExitOnErr, NestedV1Schema, addNodeStats, Node);
        }));
    Schemas.push_back(std::make_pair(
        &FlatV1Schema,
        [&](ExitOnError &ExitOnErr,
            function_ref<void(ObjectKindInfo & Info)> addNodeStats,
            cas::ObjectProxy Node) {
          visitPOTItemFlatV1(ExitOnErr, FlatV1Schema, addNodeStats, Node);
        }));
    Schemas.push_back(std::make_pair(
        &MCCASV1Schema,
        [&](ExitOnError &ExitOnErr,
            function_ref<void(ObjectKindInfo & Info)> addNodeStats,
            cas::ObjectProxy Node) {
          visitPOTItemMCCASV1(ExitOnErr, MCCASV1Schema, addNodeStats, Node);
        }));
  }

  DenseMap<ObjectRef, NodeInfo> Nodes;

  StringMap<ObjectKindInfo> Stats;
  ObjectKindInfo Totals;
  DenseSet<StringRef> GeneratedNames;
  DenseSet<cas::ObjectRef> SectionNames;
  DenseSet<cas::ObjectRef> SymbolNames;
  DenseSet<cas::ObjectRef> UndefinedSymbols;
  DenseSet<cas::ObjectRef> ContentBlobs;
  size_t NumAnonymousSymbols = 0;
  size_t NumTemplateSymbols = 0;
  size_t NumTemplateTargets = 0;
  size_t NumZeroFillBlocks = 0;
  size_t Num1TargetBlocks = 0;
  size_t Num2TargetBlocks = 0;
  size_t NumTinyObjects = 0;
  size_t SecRefSize = 0;
  size_t AtomRefSize = 0;

  void visitPOT(ExitOnError &ExitOnErr, ArrayRef<ObjectProxy> TopLevels,
                ArrayRef<POTItem> POT);
  void visitPOTItem(ExitOnError &ExitOnErr, const POTItem &Item);
  void
  visitPOTItemNestedV1(ExitOnError &ExitOnErr,
                       nestedv1::ObjectFileSchema &Schema,
                       function_ref<void(ObjectKindInfo &Info)> addNodeStats,
                       cas::ObjectProxy Node);
  void visitPOTItemFlatV1(ExitOnError &ExitOnErr,
                          flatv1::ObjectFileSchema &Schema,
                          function_ref<void(ObjectKindInfo &Info)> addNodeStats,
                          cas::ObjectProxy Node);
  void
  visitPOTItemMCCASV1(ExitOnError &ExitOnErr,
                      llvm::mccasformats::v1::MCSchema &Schema,
                      function_ref<void(ObjectKindInfo &Info)> addNodeStats,
                      cas::ObjectProxy Node);
  void printToOuts(ArrayRef<ObjectProxy> TopLevels, raw_ostream &StatOS);
};
} // end namespace

void StatCollector::visitPOT(ExitOnError &ExitOnErr,
                             ArrayRef<ObjectProxy> TopLevels,
                             ArrayRef<POTItem> POT) {
  for (auto ID : TopLevels)
    Nodes[ID.getRef()].NumPaths = 1;

  // Visit POT in reverse (topological sort), computing Nodes and collecting
  // stats.
  for (const POTItem &Item : llvm::reverse(POT))
    visitPOTItem(ExitOnErr, Item);
}

void StatCollector::visitPOTItem(ExitOnError &ExitOnErr, const POTItem &Item) {
  cas::ObjectRef ID = Item.ID;
  size_t NumPaths = Nodes.lookup(ID).NumPaths;
  Optional<ObjectHandle> Object = ExitOnErr(CAS.load(ID));
  assert(Object);

  auto updateChild = [&](ObjectRef Child) {
    ++Nodes[Child].NumParents;
    Nodes[Child].NumPaths += NumPaths;
  };

  size_t NumParents = Nodes.lookup(ID).NumParents;
  TreeSchema Schema(CAS);
  if (Schema.isNode(*Object)) {
    auto &Info = Stats["builtin:tree"];
    ++Info.Count;
    TreeProxy Tree = ExitOnErr(Schema.load(*Object));
    Info.NumChildren += Tree.size();
    Info.NumParents += NumParents;
    Info.NumPaths += NumPaths;
    if (!Tree.size())
      Totals.NumPaths += NumPaths; // Count paths to leafs.

    ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
      // FIXME: This is copied out of BuiltinCAS.cpp's makeTree.
      Info.DataSize += sizeof(uint64_t) + sizeof(uint32_t) +
                       alignTo(Entry.getName().size() + 1, Align(4));
      updateChild(Entry.getRef());
      return Error::success();
    }));
    return;
  }

  ObjectProxy Node = ObjectProxy::load(CAS, *Object);
  auto addNodeStats = [&](ObjectKindInfo &Info) {
    ++Info.Count;
    Info.NumChildren += Node.getNumReferences();
    Info.NumParents += NumParents;
    Info.NumPaths += NumPaths;
    Info.DataSize += Node.getData().size();
    if (!Node.getNumReferences())
      Totals.NumPaths += NumPaths; // Count paths to leafs.

    ExitOnErr(Node.forEachReference([&](ObjectRef Child) {
      updateChild(Child);
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
    function_ref<void(ObjectKindInfo &Info)> addNodeStats,
    cas::ObjectProxy Node) {
  using namespace llvm::casobjectformats::nestedv1;
  ObjectFormatObjectProxy Object = ExitOnErr(Schema.get(Node.getRef()));
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
      if (Optional<cas::ObjectRef> Name = Symbol->getNameID()) {
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
        cas::ObjectRef Name = List->getNameID(I);
        if (!SymbolNames.count(Name))
          UndefinedSymbols.insert(Name);
      }
    }
  }
  if (Object.getKindString() == SectionRef::KindString) {
    if (Optional<SectionRef> Section =
            expectedToOptional(SectionRef::get(Object))) {
      if (Optional<cas::ObjectRef> Name = Section->getNameID())
        SectionNames.insert(*Name);
    }
  }
}

void StatCollector::visitPOTItemFlatV1(
    ExitOnError &ExitOnErr, flatv1::ObjectFileSchema &Schema,
    function_ref<void(ObjectKindInfo &Info)> addNodeStats,
    cas::ObjectProxy Node) {
  using namespace llvm::casobjectformats::flatv1;
  ObjectFormatObjectProxy Object = ExitOnErr(Schema.get(Node.getRef()));
  addNodeStats(Stats[Object.getKindString()]);
}

void StatCollector::visitPOTItemMCCASV1(
    ExitOnError &ExitOnErr, llvm::mccasformats::v1::MCSchema &Schema,
    function_ref<void(ObjectKindInfo &Info)> addNodeStats,
    cas::ObjectProxy Node) {
  using namespace llvm::casobjectformats::flatv1;
  auto Object = ExitOnErr(Schema.get(Node.getRef()));
  addNodeStats(Stats[Object.getKindString()]);

  if (Object.getNumReferences() == 0 &&
      Object.getData().size() <
          (Object.getID().getHash().size() + sizeof(void *)))
    ++NumTinyObjects;

  if (Object.getKindString() ==
      llvm::mccasformats::v1::SectionRef::KindString) {
    if (!Object.getData().empty())
      SecRefSize += Object.getData().size();
  }
  if (Object.getKindString() ==
      llvm::mccasformats::v1::AtomRef::KindString) {
    if (!Object.getData().empty())
      AtomRefSize += Object.getData().size();
  }
}

static void computeStats(CASDB &CAS, ArrayRef<ObjectProxy> TopLevels,
                         raw_ostream &StatOS) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner("llvm-cas-object-format: compute-stats: ");

  MSG("Collecting object stats...\n");

  // In the first traversal, just collect a POT. Use NumPaths as a "Seen" list.
  StatCollector Collector(CAS);
  auto &Nodes = Collector.Nodes;
  struct WorklistItem {
    ObjectRef ID;
    bool Visited;
    StringRef Path;
    const NodeSchema *Schema = nullptr;
  };
  SmallVector<WorklistItem> Worklist;
  SmallVector<POTItem> POT;
  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);
  Optional<GlobPattern> GlobP;
  if (!CASGlob.empty())
    GlobP.emplace(ExitOnErr(GlobPattern::create(CASGlob)));

  auto push = [&](ObjectRef ID, StringRef Path,
                  const NodeSchema *Schema = nullptr) {
    auto &Node = Nodes[ID];
    if (!Node.Done)
      Worklist.push_back({ID, false, Path, Schema});
  };
  for (auto ID : TopLevels)
    push(ID.getRef(), "/");
  while (!Worklist.empty()) {
    ObjectRef ID = Worklist.back().ID;
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
    Optional<ObjectHandle> Object = ExitOnErr(CAS.load(ID));
    assert(Object);
    if (!Object) {
      Worklist.pop_back();
      continue;
    }

    TreeSchema TSchema(CAS);
    if (TSchema.isNode(*Object)) {
      TreeProxy Tree = ExitOnErr(TSchema.load(*Object));
      ExitOnErr(Tree.forEachEntry([&](const NamedTreeEntry &Entry) {
        SmallString<128> PathStorage = Name;
        sys::path::append(PathStorage, sys::path::Style::posix,
                          Entry.getName());
        if (Entry.getKind() == TreeEntry::Regular) {
          if (GlobP && !GlobP->match(PathStorage))
            return Error::success();
        }
        push(Entry.getRef(), Saver.save(StringRef(PathStorage)));
        return Error::success();
      }));
      continue;
    }

    ObjectProxy Node = ObjectProxy::load(CAS, *Object);
    const NodeSchema *&Schema = Worklist.back().Schema;

    // Update the schema.
    if (!Schema) {
      for (auto &S : Collector.Schemas)
        if (S.first->isRootNode(Node))
          Schema = S.first;
    } else if (!Schema->isNode(Node)) {
      Schema = nullptr;
    }

    ExitOnErr(Node.forEachReference([&, Schema](ObjectRef Child) {
      push(Child, "", Schema);
      return Error::success();
    }));
  }

  Collector.visitPOT(ExitOnErr, TopLevels, POT);
  Collector.printToOuts(TopLevels, StatOS);
}

void StatCollector::printToOuts(ArrayRef<ObjectProxy> TopLevels,
                                raw_ostream &StatOS) {

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

  CASID FirstID = CAS.getID(TopLevels.front());
  size_t NumHashBytes = FirstID.getHash().size();
  if (ObjectStatsFormat == FormatType::Pretty) {
    StatOS
        << "  => Note: 'Parents' counts incoming edges\n"
        << "  => Note: 'Children' counts outgoing edges (to sub-objects)\n"
        << "  => Note: HashSize = " << NumHashBytes << "B\n"
        << "  => Note: PtrSize  = " << sizeof(void *) << "B\n"
        << "  => Note: Cost     = Count*HashSize + PtrSize*Children + Data\n";
  }
  StringLiteral HeaderFormatPretty =
      "{0,-22} {1,+10} {2,+7} {3,+10} {4,+7} {5,+10} "
      "{6,+7} {7,+10} {8,+7} {9,+10} {10,+7}\n";
  StringLiteral FormatPretty =
      "{0,-22} {1,+10} {2,+7:P} {3,+10} {4,+7:P} {5,+10} "
      "{6,+7:P} {7,+10} {8,+7:P} {9,+10} {10,+7:P}\n";
  StringLiteral FormatCSV = "{0}, {1}, {3}, {5}, {7}, {9}\n";

  StringLiteral HeaderFormat =
      ObjectStatsFormat == FormatType::Pretty ? HeaderFormatPretty : FormatCSV;
  StringLiteral Format =
      ObjectStatsFormat == FormatType::Pretty ? FormatPretty : FormatCSV;

  StatOS << llvm::formatv(HeaderFormat.begin(), "Kind", "Count", "", "Parents",
                          "", "Children", "", "Data (B)", "", "Cost (B)", "");
  if (ObjectStatsFormat == FormatType::Pretty) {
    StatOS << llvm::formatv(HeaderFormat.begin(), "====", "=====", "",
                            "=======", "", "========", "", "========", "",
                            "========", "");
  }

  auto printInfo = [&](StringRef Kind, ObjectKindInfo Info) {
    if (!Info.Count)
      return;
    auto getPercent = [](double N, double D) { return D ? N / D : 0.0; };
    size_t Size = Info.getTotalSize(NumHashBytes);
    StatOS << llvm::formatv(
        Format.begin(), Kind, Info.Count, getPercent(Info.Count, Totals.Count),
        Info.NumParents, getPercent(Info.NumParents, Totals.NumParents),
        Info.NumChildren, getPercent(Info.NumChildren, Totals.NumChildren),
        Info.DataSize, getPercent(Info.DataSize, Totals.DataSize), Size,
        getPercent(Size, Totals.getTotalSize(NumHashBytes)));
  };
  for (StringRef Kind : Kinds)
    printInfo(Kind, Stats.lookup(Kind));
  printInfo("TOTAL", Totals);

  StringLiteral OtherStatsPretty = "{0,-22} {1,+10}\n";
  StringLiteral OtherStatsCSV = "{0}, {1}\n";

  StringLiteral OtherStats = ObjectStatsFormat == FormatType::Pretty
                                 ? OtherStatsPretty
                                 : OtherStatsCSV;
  // Other stats.
  bool HasPrinted = false;
  auto printIfNotZero = [&](StringRef Name, size_t Num) {
    if (!Num)
      return;
    if (!HasPrinted)
      StatOS << "\n";
    StatOS << llvm::formatv(OtherStats.begin(), Name, Num);
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
  printIfNotZero("num-tiny-objects", NumTinyObjects);
  printIfNotZero("sec-ref-size", SecRefSize);
  printIfNotZero("atom-ref-size", AtomRefSize);
}

static Error printCASObject(ObjectFormatSchemaPool &Pool, ObjectProxy ID,
                            bool omitCASID) {
  auto Reader = Pool.createObjectReader(ID);
  if (Error E = Reader.takeError())
    return E;
  return printCASObject(**Reader, outs(), omitCASID);
}

static Error printCASObjectOrTree(ObjectFormatSchemaPool &Pool, ObjectProxy ID,
                                  bool omitCASID) {
  TreeSchema Schema(Pool.getCAS());
  if (!Schema.isNode(ID)) {
    // Not a tree.
    return printCASObject(Pool, ID, omitCASID);
  }

  return Schema.walkFileTreeRecursively(
      Pool.getCAS(), ID,
      [&](const NamedTreeEntry &entry, Optional<TreeProxy>) -> Error {
        if (entry.getKind() == TreeEntry::Tree)
          return Error::success();
        if (entry.getKind() != TreeEntry::Regular) {
          return createStringError(inconvertibleErrorCode(),
                                   "found non-regular entry: " +
                                       entry.getName());
        }
        auto Proxy = Pool.getCAS().getProxy(entry.getRef());
        if (!Proxy)
          return Proxy.takeError();
        return printCASObject(Pool, *Proxy, omitCASID);
      });
}

static Error materializeObjectsFromCASTree(CASDB &CAS, ObjectProxy ID) {
  TreeSchema Schema(CAS);
  return Schema.walkFileTreeRecursively(
      CAS, ID, [&](const NamedTreeEntry &Entry, Optional<TreeProxy>) -> Error {
        if (Entry.getKind() != TreeEntry::Regular &&
            Entry.getKind() != TreeEntry::Tree) {
          return createStringError(inconvertibleErrorCode(),
                                   "found non-regular entry: " +
                                       Entry.getName());
        }
        SmallString<256> OutputPath(OutputPrefix);
        StringRef ObjFileName = Entry.getName();
        ObjFileName.consume_back(".casid");
        llvm::sys::path::append(OutputPath, ObjFileName);

        if (Entry.getKind() == TreeEntry::Tree) {
          // Check the path exists, if not, create the directory.
          if (!llvm::sys::fs::exists(OutputPath)) {
            if (auto EC = llvm::sys::fs::create_directory(OutputPath))
              return errorCodeToError(EC);
          }
          return Error::success();
        }
        auto ObjRoot = CAS.getProxy(Entry.getRef());
        if (!ObjRoot)
          return ObjRoot.takeError();

        SmallString<50> ContentsStorage;
        raw_svector_ostream ObjOS(ContentsStorage);
        auto Schema = std::make_unique<llvm::mccasformats::v1::MCSchema>(CAS);
        if (Error E = Schema->serializeObjectFile(*ObjRoot, ObjOS))
          return E;
        std::unique_ptr<llvm::FileOutputBuffer> Output;
        if (Error E = llvm::FileOutputBuffer::create(OutputPath,
                                                     ContentsStorage.size())
                          .moveInto(Output))
          return E;
        llvm::copy(ContentsStorage, Output->getBufferStart());
        return Output->commit();
      });
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
static void removeRedundantSectionSymbol(jitlink::LinkGraph &G,
                                         StringRef SectionName) {
  // FIXME: Consider implementing removeRedundantEHFrameSymbol as a jitlink
  // pass. Note this section name is machO specific.
  auto *Section = G.findSectionByName(SectionName);
  if (!Section)
    return;
  for (jitlink::Symbol *Sym : G.defined_symbols()) {
    if (&Sym->getBlock().getSection() == Section) {
      G.removeDefinedSymbol(*Sym);
      break;
    }
  }
}

static Error createSplitEHFramePasses(jitlink::LinkGraph &G) {
  if (G.getTargetTriple().getObjectFormat() == Triple::MachO &&
      G.getTargetTriple().getArch() == Triple::x86_64) {
    removeRedundantSectionSymbol(G, "__TEXT,__eh_frame");
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

static Error SplitDebugLine(jitlink::LinkGraph &G) {
  if (G.getTargetTriple().getObjectFormat() == Triple::MachO) {
    removeRedundantSectionSymbol(G, "__DWARF,__debug_line");
    if (auto E = jitlink::createDebugLineSplitterPass_MachO()(G))
      return E;
    // Add an anonymous symbol so that the blocks don't get dropped
    if (auto *DLSec = G.findSectionByName("__DWARF,__debug_line")) {
      for (auto *B : DLSec->blocks()) {
        G.addAnonymousSymbol(*B, 0, B->getSize(), false, true);
      }
    }
    
  }
  return Error::success();
}

static ObjectRef ingestFile(ObjectFormatSchemaBase &Schema, StringRef InputFile,
                            MemoryBufferRef FileContent, SharedStream &OS) {
  ExitOnError ExitOnErr;
  ExitOnErr.setBanner(("llvm-cas-object-format: " + InputFile + ": ").str());

  auto EmitDotOnReturn = llvm::make_scope_exit([&]() {
    if (!Silent)
      OS.applyLocked([&](raw_ostream &OS) { OS << "."; });
  });

  auto &CAS = Schema.CAS;
  if (JustBlobs)
    return ExitOnErr(CAS.storeFromString(None, FileContent.getBuffer()));

  if (CASIDFile) {
    auto ID = ExitOnErr(readCASIDBuffer(Schema.CAS, FileContent));
    return ExitOnErr(Schema.CAS.getProxy(ID)).getRef();
  }

  auto createLinkGraph =
      [&ExitOnErr](
          MemoryBufferRef FileContent) -> std::unique_ptr<jitlink::LinkGraph> {
    auto G = ExitOnErr(jitlink::createLinkGraphFromObject(FileContent));

    if (SplitEHFrames)
      ExitOnErr(createSplitEHFramePasses(*G));
    
    ExitOnErr(SplitDebugLine(*G));
            
    return G;
  };

  auto G = createLinkGraph(FileContent);

  if (!JustDsymutil.empty()) {
    // Create a map for each symbol in TEXT.
    jitlink::Section *Text = G->findSectionByName("__TEXT,__text");
    TreeSchema Schema(CAS);
    if (!Text)
      return CAS.getReference(ExitOnErr(Schema.create()));

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
      Builder.push(
          ExitOnErr(CAS.storeFromString(None, DwarfBuffer->getBuffer())),
          TreeEntry::Regular, S->getName());
    }
    ExitOnErr(MapFile.discard());
    ExitOnErr(DsymFile.discard());
    return CAS.getReference(ExitOnErr(Builder.create(CAS)));
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
      errs() << "error: got different CASID while repeating ingestion, "
             << CompileUnit.getID() << " and " << NewCompileUnit.getID()
             << "\n";
      exit(1);
    }
  }

  if (DebugIngest)
    OS.applyLocked([&](raw_ostream &OS) { OS << DebugIngestOutput; });

  auto Reader = ExitOnErr(Schema.createObjectReader(CompileUnit));
  auto RoundTripG = ExitOnErr(casobjectformats::createLinkGraph(
      *Reader, FileContent.getBufferIdentifier()));

  if (Dump)
    dumpGraph(*RoundTripG, OS, "after round-trip");

  return CompileUnit.getRef();
}
