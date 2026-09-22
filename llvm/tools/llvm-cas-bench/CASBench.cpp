//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Compares LLVMCAS's on-disk format against simpler alternatives on storage
/// size and DAG traversal speed, using a real CAS as the data source.
///
/// Every backend is an llvm::cas::ObjectStore, so the phases below are written
/// once against the public CAS API and run unchanged for all four.
///
//===----------------------------------------------------------------------===//

#include "CASBackends.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <atomic>
#include <chrono>
#include <map>
#include <thread>

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#endif

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casbench;

namespace {

using Clock = std::chrono::steady_clock;

cl::OptionCategory BenchCat("llvm-cas-bench options");

cl::opt<std::string>
    SourceCASPath("cas",
                  cl::desc("Path of the source LLVMCAS (required unless only "
                           "--traverse is requested)"),
                  cl::value_desc("path"), cl::cat(BenchCat));

cl::opt<std::string> IDsPath("ids",
                             cl::desc("File of root CASIDs, one per line "
                                      "(not used by --parallel-insert)"),
                             cl::value_desc("path"), cl::cat(BenchCat));

enum class BackendKind { None, LLVMCAS, SQLite, File, LMDB };

cl::opt<BackendKind> WhichBackend(
    "backend", cl::init(BackendKind::None), cl::cat(BenchCat),
    cl::desc("Storage backend to measure"),
    cl::values(clEnumValN(BackendKind::LLVMCAS, "llvmcas",
                          "LLVMCAS on-disk unified CAS"),
               clEnumValN(BackendKind::SQLite, "sqlite",
                          "SQLite table keyed by BLAKE3 digest"),
               clEnumValN(BackendKind::File, "file",
                          "One file per object, git-style two-hex subdirs"),
               clEnumValN(BackendKind::LMDB, "lmdb",
                          "LMDB database keyed by BLAKE3 digest")));

cl::opt<BackendKind> SourceBackend(
    "source-backend", cl::init(BackendKind::LLVMCAS), cl::cat(BenchCat),
    cl::desc("Backend of the source store given by --cas (default llvmcas). "
             "Keeping a copy of the input in another backend makes the data "
             "survive an LLVMCAS format change."),
    cl::values(clEnumValN(BackendKind::LLVMCAS, "llvmcas", "LLVMCAS"),
               clEnumValN(BackendKind::SQLite, "sqlite", "SQLite"),
               clEnumValN(BackendKind::File, "file", "One file per object"),
               clEnumValN(BackendKind::LMDB, "lmdb", "LMDB")));

cl::opt<std::string> OutPath("out",
                             cl::desc("Directory for the backend under test"),
                             cl::value_desc("dir"), cl::cat(BenchCat));

cl::opt<bool> DoInsertOpt("insert", cl::desc("Run the insert phase"),
                          cl::cat(BenchCat));
cl::opt<bool> DoTraverseOpt("traverse", cl::desc("Run the traversal phase"),
                            cl::cat(BenchCat));
cl::opt<bool> ParallelTraverse(
    "parallel-traverse",
    cl::desc("Traverse from several threads at once, each with its own "
             "instance of the store, splitting the roots between them."),
    cl::cat(BenchCat));
cl::opt<unsigned> TraverseThreads(
    "traverse-threads",
    cl::desc("Threads for --parallel-traverse (default: the core count)"),
    cl::init(0), cl::cat(BenchCat));
cl::opt<bool>
    StatsOnly("stats-only",
              cl::desc("Report source DAG statistics and exit without writing"),
              cl::cat(BenchCat));

cl::opt<bool> Durable(
    "durable",
    cl::desc("Use each store's durable settings instead of matching LLVMCAS, "
             "which does not fsync per object"),
    cl::cat(BenchCat));

cl::opt<bool> Verify("verify",
                     cl::desc("Check that each stored object lands under the "
                              "same CASID it has in the source CAS"),
                     cl::cat(BenchCat));

cl::opt<unsigned> MaxRoots("max-roots", cl::init(0),
                           cl::desc("Only use the first N roots (0 = all)"),
                           cl::value_desc("N"), cl::cat(BenchCat));

cl::opt<uint64_t>
    BatchSize("batch", cl::init(0),
              cl::desc("Commit every N inserts (0 = one transaction)"),
              cl::value_desc("N"), cl::cat(BenchCat));

cl::opt<std::string> JSONPath("json",
                              cl::desc("Also write the metrics as JSON here"),
                              cl::value_desc("path"), cl::cat(BenchCat));

cl::opt<bool>
    DumpBackend("dump-backend",
                cl::desc("Print backend internals after each phase. Verbose: "
                         "LLVMCAS dumps every record."),
                cl::cat(BenchCat));

cl::opt<bool> ParallelInsertMode(
    "parallel-insert",
    cl::desc("Measure how insert throughput scales with thread count, using "
             "synthesised unique objects. Needs no --cas or --ids."),
    cl::cat(BenchCat));

cl::opt<std::string>
    ThreadCounts("threads",
                 cl::desc("Comma-separated thread counts for --parallel-insert "
                          "(default: 1,2,4,8,... up to the core count)"),
                 cl::value_desc("N,N,..."), cl::cat(BenchCat));

cl::opt<uint64_t>
    NumObjects("objects", cl::init(2000000),
               cl::desc("Objects to insert per --parallel-insert run"),
               cl::value_desc("N"), cl::cat(BenchCat));

cl::opt<unsigned>
    ObjectSize("object-size", cl::init(488),
               cl::desc("Size of each synthesised object in bytes "
                        "(default 488, the mean in a real build cache)"),
               cl::value_desc("N"), cl::cat(BenchCat));

//===----------------------------------------------------------------------===//
// Formatting helpers.
//===----------------------------------------------------------------------===//

std::string humanBytes(uint64_t N) {
  const char *Units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double V = double(N);
  unsigned U = 0;
  while (V >= 1024.0 && U + 1 < std::size(Units)) {
    V /= 1024.0;
    ++U;
  }
  return U == 0 ? formatv("{0} B", N).str()
                : formatv("{0:F2} {1}", V, Units[U]).str();
}

double toSeconds(std::chrono::nanoseconds D) {
  return std::chrono::duration<double>(D).count();
}

double perSecond(uint64_t Count, std::chrono::nanoseconds D) {
  double S = toSeconds(D);
  return S > 0 ? double(Count) / S : 0.0;
}

double nanosEach(uint64_t Count, std::chrono::nanoseconds D) {
  return Count ? double(D.count()) / double(Count) : 0.0;
}

/// Peak resident set size of this process, in bytes.
///
/// Worth reporting because the backends differ in whether they can hand out
/// pointers into a live mapping: the ones that cannot must copy object data
/// into store-owned memory to satisfy the ObjectStore lifetime contract, and
/// that shows up here rather than on disk.
std::optional<uint64_t> getPeakRSS() {
#ifndef _WIN32
  struct rusage RU;
  if (::getrusage(RUSAGE_SELF, &RU) != 0)
    return std::nullopt;
#ifdef __APPLE__
  return uint64_t(RU.ru_maxrss); // Darwin reports bytes.
#else
  return uint64_t(RU.ru_maxrss) * 1024; // Linux reports kilobytes.
#endif
#else
  return std::nullopt;
#endif
}

//===----------------------------------------------------------------------===//
// Size on disk.
//===----------------------------------------------------------------------===//

struct DirSize {
  uint64_t Apparent = 0;  ///< Sum of st_size.
  uint64_t Allocated = 0; ///< Sum of st_blocks * 512: what the volume gives up.
  uint64_t FileCount = 0;
  uint64_t DirCount = 0;
};

Expected<DirSize> measureDirSize(StringRef Path) {
  DirSize S;
  std::error_code EC;
  for (sys::fs::recursive_directory_iterator I(Path, EC), E; I != E && !EC;
       I.increment(EC)) {
    ErrorOr<sys::fs::basic_file_status> Status = I->status();
    if (!Status)
      continue; // Raced with a delete, or unreadable; not our concern.
    if (sys::fs::is_directory(*Status)) {
      ++S.DirCount;
      continue;
    }
    if (!sys::fs::is_regular_file(*Status))
      continue;
    ++S.FileCount;
    S.Apparent += Status->getSize();
#ifndef _WIN32
    // file_status does not surface st_blocks, and for this measurement the
    // blocks actually charged are the whole point.
    struct stat SB;
    if (::stat(I->path().c_str(), &SB) == 0)
      S.Allocated += uint64_t(SB.st_blocks) * 512;
    else
      S.Allocated += Status->getSize();
#else
    S.Allocated += Status->getSize();
#endif
  }
  if (EC)
    return createFileError(Path, EC);
  return S;
}

/// Allocation granularity of the filesystem holding \p Path, for predicting
/// what the one-file-per-object layout would cost.
uint64_t getBlockSize(StringRef Path) {
#ifndef _WIN32
  struct statvfs VFS;
  SmallString<128> P(Path);
  if (::statvfs(P.c_str(), &VFS) == 0 && VFS.f_frsize)
    return VFS.f_frsize;
#endif
  return 4096;
}

uint64_t roundUpTo(uint64_t V, uint64_t Align) {
  return Align ? ((V + Align - 1) / Align) * Align : V;
}

//===----------------------------------------------------------------------===//
// Root CASIDs.
//===----------------------------------------------------------------------===//

/// CASIDs are portable between stores that share a hash schema, so these can
/// be parsed with whichever store is at hand and used against any other.
Expected<std::vector<CASID>> readRootIDs(ObjectStore &CAS, StringRef Path) {
  auto Buf = MemoryBuffer::getFile(Path);
  if (!Buf)
    return createFileError(Path, Buf.getError());

  std::vector<CASID> IDs;
  SmallVector<StringRef, 0> Lines;
  (*Buf)->getBuffer().split(Lines, '\n');
  for (StringRef Line : Lines) {
    Line = Line.trim();
    if (Line.empty() || Line.starts_with("#"))
      continue;
    Expected<CASID> ID = CAS.parseID(Line);
    if (!ID)
      return createStringError(errc::invalid_argument,
                               "%s: cannot parse '%s': %s", Path.str().c_str(),
                               Line.str().c_str(),
                               toString(ID.takeError()).c_str());
    IDs.push_back(*ID);
  }
  if (MaxRoots && MaxRoots < IDs.size())
    IDs.erase(IDs.begin() + MaxRoots, IDs.end());
  return IDs;
}

//===----------------------------------------------------------------------===//
// Phase A: collect the source DAG.
//===----------------------------------------------------------------------===//

struct Node {
  uint32_t RefBegin = 0; ///< Offset into NodeGraph::RefIndices.
  uint32_t NumRefs = 0;
  StringRef Data; ///< In NodeGraph::DataAlloc.
};

/// The deduplicated DAG in topological order: a node always appears after all
/// of its children, which is what ObjectStore::store() requires, since it
/// takes ObjectRefs of children that must already exist.
struct NodeGraph {
  std::vector<Node> Nodes;
  std::vector<uint32_t> RefIndices;
  std::vector<ObjectRef> SourceRefs; ///< Parallel to Nodes, for --verify.
  /// Every node's data, copied: a key/value source reuses its buffer on the
  /// next load.
  BumpPtrAllocator DataAlloc;
  uint64_t TotalDataBytes = 0;
  uint64_t TotalRefs = 0;
  unsigned MaxDepth = 0;
  unsigned MissingRoots = 0;
  std::chrono::nanoseconds Elapsed{0};

  size_t size() const { return Nodes.size(); }
};

/// Walk every DAG rooted at \p RootIDs, emitting each unique node exactly
/// once and always after its children. Shaped after ObjectStore::importObject
/// (llvm/lib/CAS/ObjectStore.cpp), which does the same children-first cursor
/// walk when copying a tree between stores.
///
/// A key/value store's handle is only good until its next load, so whatever
/// is needed from a node is copied out the moment it is loaded: its
/// references onto \c Pending, its data into the graph.
Expected<NodeGraph> collectGraph(ObjectStore &CAS, ArrayRef<CASID> RootIDs) {
  NodeGraph G;
  auto Start = Clock::now();

  DenseMap<ObjectRef, uint32_t> Emitted;

  struct Frame {
    ObjectRef Ref;
    StringRef Data;
    uint32_t ChildBegin; ///< Offset into Pending.
    uint32_t NumChildren;
    uint32_t NextChild;
  };
  SmallVector<Frame, 64> Stack;
  SmallVector<ObjectRef, 256> Pending; ///< The children of every frame.

  auto Push = [&](ObjectRef Ref) -> Error {
    Expected<ObjectProxy> Proxy = CAS.getProxy(Ref);
    if (!Proxy)
      return Proxy.takeError();
    StringRef Data = Proxy->getData();
    char *Copy = G.DataAlloc.Allocate<char>(Data.size());
    llvm::copy(Data, Copy);
    uint32_t ChildBegin = uint32_t(Pending.size());
    if (Error E = Proxy->forEachReference([&](ObjectRef Child) -> Error {
          Pending.push_back(Child);
          return Error::success();
        }))
      return E;
    Stack.push_back({Ref, StringRef(Copy, Data.size()), ChildBegin,
                     uint32_t(Pending.size()) - ChildBegin, 0});
    return Error::success();
  };

  for (const CASID &RootID : RootIDs) {
    std::optional<ObjectRef> RootRef = CAS.getReference(RootID);
    if (!RootRef) {
      ++G.MissingRoots;
      errs() << "warning: root not present in CAS, skipping: "
             << RootID.toString() << "\n";
      continue;
    }
    if (Emitted.contains(*RootRef))
      continue;
    if (Error E = Push(*RootRef))
      return std::move(E);

    while (!Stack.empty()) {
      G.MaxDepth = std::max<unsigned>(G.MaxDepth, Stack.size());

      Frame &F = Stack.back();
      if (F.NextChild != F.NumChildren) {
        ObjectRef Child = Pending[F.ChildBegin + F.NextChild++];
        if (!Emitted.contains(Child))
          if (Error E = Push(Child)) // Invalidates F.
            return std::move(E);
        continue;
      }

      // Every child has been emitted, so this node can be too.
      Node N;
      N.RefBegin = uint32_t(G.RefIndices.size());
      N.NumRefs = F.NumChildren;
      N.Data = F.Data;
      for (uint32_t J = 0; J != F.NumChildren; ++J) {
        auto It = Emitted.find(Pending[F.ChildBegin + J]);
        assert(It != Emitted.end() && "child must already be emitted");
        G.RefIndices.push_back(It->second);
      }
      Emitted[F.Ref] = uint32_t(G.Nodes.size());
      G.Nodes.push_back(N);
      G.SourceRefs.push_back(F.Ref);
      G.TotalDataBytes += N.Data.size();
      G.TotalRefs += N.NumRefs;
      Pending.truncate(F.ChildBegin);
      Stack.pop_back();
    }
  }

  G.Elapsed = Clock::now() - Start;
  return std::move(G);
}

//===----------------------------------------------------------------------===//
// Parallel insert: how insert throughput scales with thread count.
//
// This mode needs no input data set. It synthesises objects whose contents are
// unique by construction, so every store creates a new entry rather than
// hitting the existing-object early return in OnDiskGraphDB::store().
//===----------------------------------------------------------------------===//

/// Deterministically unique contents for object \p Index.
///
/// The index goes in the first eight bytes so uniqueness is guaranteed, and
/// the remainder is filled with a cheap PRNG so the hash has to cover the
/// whole buffer rather than a run of identical bytes.
void makeObject(uint64_t Index, unsigned Size, SmallVectorImpl<char> &Out) {
  assert(Size >= sizeof(uint64_t) && "object too small to be unique");
  Out.resize_for_overwrite(Size);
  char *P = Out.data();
  std::memcpy(P, &Index, sizeof(Index));
  uint64_t X = Index * 0x9E3779B97F4A7C15ULL + 1;
  size_t I = sizeof(Index);
  for (; I + sizeof(uint64_t) <= Size; I += sizeof(uint64_t)) {
    X ^= X >> 30;
    X *= 0xBF58476D1CE4E5B9ULL;
    X ^= X >> 27;
    std::memcpy(P + I, &X, sizeof(X));
  }
  for (; I != Size; ++I)
    P[I] = char(X >> (8 * (I % sizeof(uint64_t))));
}

struct ParallelResult {
  unsigned Threads = 0;
  uint64_t Objects = 0;
  uint64_t Bytes = 0;
  std::chrono::nanoseconds Elapsed{0};
};

/// Store \p Objects synthetic objects using \p Threads threads.
///
/// The timed region is the store() calls and nothing else: contents are cheap
/// to synthesise and the returned refs are discarded, so this does not pay for
/// getID() or for accumulating results.
Error parallelInsert(ObjectStore &CAS, unsigned Threads, uint64_t Objects,
                     unsigned Size, ParallelResult &R) {
  std::vector<std::string> Failures(Threads);

  auto Body = [&](unsigned T) {
    SmallVector<char, 1024> Data;
    uint64_t Begin = Objects * T / Threads, End = Objects * (T + 1) / Threads;
    for (uint64_t I = Begin; I != End; ++I) {
      makeObject(I, Size, Data);
      Expected<ObjectRef> Ref =
          CAS.storeFromString({}, StringRef(Data.data(), Data.size()));
      if (!Ref) {
        Failures[T] = toString(Ref.takeError());
        return;
      }
    }
  };

  auto Start = Clock::now();
  if (Threads == 1) {
    Body(0);
  } else {
    std::vector<std::thread> Pool;
    Pool.reserve(Threads);
    for (unsigned T = 0; T != Threads; ++T)
      Pool.emplace_back(Body, T);
    for (std::thread &P : Pool)
      P.join();
  }
  R.Elapsed = Clock::now() - Start;

  for (const std::string &F : Failures)
    if (!F.empty())
      return createStringError(errc::io_error, "store failed: %s", F.c_str());

  R.Threads = Threads;
  R.Objects = Objects;
  R.Bytes = Objects * uint64_t(Size);
  return Error::success();
}

//===----------------------------------------------------------------------===//
// Phase B: insert. Phase C: traverse. Both are pure ObjectStore API.
//===----------------------------------------------------------------------===//

struct InsertStats {
  uint64_t Nodes = 0;
  uint64_t Bytes = 0;
  std::chrono::nanoseconds Elapsed{0};
};

struct TraverseStats {
  unsigned Threads = 1;
  uint64_t Nodes = 0;
  uint64_t Edges = 0;
  uint64_t Bytes = 0;
  std::chrono::nanoseconds Elapsed{0};
};

Error insertAll(ObjectStore &Target, ObjectStore &Source, const NodeGraph &G,
                InsertStats &S) {
  // ObjectRef of each node in the target, filled in as we go. The graph is in
  // topological order, so a node's children always already have one.
  std::vector<ObjectRef> Stored;
  Stored.reserve(G.size());
  SmallVector<ObjectRef, 16> ChildRefs;

  auto Start = Clock::now();
  for (size_t I = 0, E = G.size(); I != E; ++I) {
    const Node &N = G.Nodes[I];
    ChildRefs.clear();
    ChildRefs.reserve(N.NumRefs);
    for (uint32_t J = 0; J != N.NumRefs; ++J)
      ChildRefs.push_back(Stored[G.RefIndices[N.RefBegin + J]]);

    Expected<ObjectRef> Ref = Target.storeFromString(ChildRefs, N.Data);
    if (!Ref)
      return Ref.takeError();
    Stored.push_back(*Ref);

    ++S.Nodes;
    S.Bytes += N.Data.size();
  }
  S.Elapsed = Clock::now() - Start;

  if (Verify)
    for (size_t I = 0, E = G.size(); I != E; ++I)
      if (Target.getID(Stored[I]) != Source.getID(G.SourceRefs[I]))
        return createStringError(
            errc::invalid_argument,
            "node %zu stored as %s but the source calls it %s", I,
            Target.getID(Stored[I]).toString().c_str(),
            Source.getID(G.SourceRefs[I]).toString().c_str());

  return Error::success();
}

Error traverseAll(ObjectStore &CAS, ArrayRef<CASID> RootIDs, TraverseStats &S) {
  SmallVector<ObjectRef, 64> Work;
  // Counted in locals and stored once at the end. A parallel walk keeps one
  // TraverseStats per thread side by side, and incrementing those per visit
  // put several threads' counters on one cache line; that false sharing, not
  // the store, was once most of what a parallel run measured.
  uint64_t Nodes = 0, Edges = 0, Bytes = 0;

  auto Start = Clock::now();
  for (const CASID &RootID : RootIDs) {
    std::optional<ObjectRef> Root = CAS.getReference(RootID);
    if (!Root)
      return createStringError(errc::no_such_file_or_directory,
                               "missing root %s", RootID.toString().c_str());
    Work.push_back(*Root);

    while (!Work.empty()) {
      ObjectRef Ref = Work.pop_back_val();

      Expected<ObjectProxy> Proxy = CAS.getProxy(Ref);
      if (!Proxy)
        return Proxy.takeError();

      Bytes += Proxy->getData().size();
      ++Nodes;

      // forEachReference, not getNumReferences plus getReference in a loop:
      // OnDiskCAS::readRef re-decodes the object's record on every index, so
      // the indexed form costs one decode per edge instead of one per node.
      // No visited set: this measures raw traversal throughput, so every edge
      // is followed even into subgraphs already walked. Visits therefore come
      // to one per edge plus one per root, not one per unique node.
      if (Error E = Proxy->forEachReference([&](ObjectRef Child) -> Error {
            ++Edges;
            Work.push_back(Child);
            return Error::success();
          }))
        return E;
    }
  }
  S.Nodes = Nodes;
  S.Edges = Edges;
  S.Bytes = Bytes;
  S.Elapsed = Clock::now() - Start;
  return Error::success();
}

/// Traverse from \p NumThreads threads at once, each with its own instance of
/// the store at \p Dir and its own share of the roots.
///
/// The threads share nothing, not even an ObjectStore: each opens its own, as
/// separate processes reading one cache would, so this measures what
/// concurrent readers cost each other inside the store rather than in the
/// harness. As in the serial walk there is no visited set, so the thread that
/// gets a root does all the work reachable from it and threads duplicate each
/// other's work wherever the roots share subgraphs.
///
/// Every thread opens its store before any starts walking, and closes it
/// after it stops, so neither is timed. The elapsed time is the slowest
/// thread's, and the rate is the total visits over that, so an uneven split
/// shows up as lost throughput rather than being hidden.
Error traverseAllParallel(BackendFactory Factory, StringRef Dir,
                          const BackendOptions &Opts, ArrayRef<CASID> RootIDs,
                          unsigned NumThreads, TraverseStats &S) {
  if (NumThreads > RootIDs.size())
    NumThreads = RootIDs.size();
  if (NumThreads == 0)
    return Error::success();
  S.Threads = NumThreads;

  std::vector<TraverseStats> Stats(NumThreads);
  std::vector<Clock::time_point> Ends(NumThreads);
  std::vector<std::string> Failures(NumThreads);
  std::vector<std::thread> Threads;
  Threads.reserve(NumThreads);
  std::atomic<unsigned> Ready{0};
  std::atomic<bool> Go{false};

  for (unsigned T = 0; T != NumThreads; ++T) {
    Threads.emplace_back([&, T] {
      // Round-robin rather than contiguous blocks: roots next to each other in
      // the file tend to be the same size, so blocks split the work unevenly.
      std::vector<CASID> Mine;
      for (size_t I = T; I < RootIDs.size(); I += NumThreads)
        Mine.push_back(RootIDs[I]);
      Expected<std::unique_ptr<ObjectStore>> CAS =
          Factory(Dir, /*ForWrite=*/false, Opts);
      Ready.fetch_add(1, std::memory_order_release);
      if (!CAS) {
        Failures[T] = toString(CAS.takeError());
        return;
      }
      while (!Go.load(std::memory_order_acquire))
        std::this_thread::yield();
      if (Error E = traverseAll(**CAS, Mine, Stats[T]))
        Failures[T] = toString(std::move(E));
      Ends[T] = Clock::now();
    });
  }
  while (Ready.load(std::memory_order_acquire) != NumThreads)
    std::this_thread::yield();
  auto Start = Clock::now();
  Go.store(true, std::memory_order_release);
  for (std::thread &T : Threads)
    T.join();

  for (const std::string &F : Failures)
    if (!F.empty())
      return createStringError(errc::invalid_argument, "%s", F.c_str());

  for (const TraverseStats &L : Stats) {
    S.Nodes += L.Nodes;
    S.Edges += L.Edges;
    S.Bytes += L.Bytes;
  }
  S.Elapsed = *llvm::max_element(Ends) - Start;

  // How evenly the work split. Elapsed is the slowest thread, so a wide
  // spread is lost throughput, and it is the harness's fault rather than the
  // store's: roots are handed out once and never rebalanced, and a machine
  // with efficiency cores takes longer over the same share on those.
  std::vector<double> Each;
  Each.reserve(Stats.size());
  for (const TraverseStats &L : Stats)
    Each.push_back(toSeconds(L.Elapsed));
  llvm::sort(Each);
  outs() << formatv(
      "  thread time           fastest {0:F3}s  median {1:F3}s  slowest "
      "{2:F3}s  ({3:F2}x spread)\n",
      Each.front(), Each[Each.size() / 2], Each.back(),
      Each.back() / std::max(Each.front(), 1e-9));
  double Sum = 0;
  for (double D : Each)
    Sum += D;
  outs() << formatv("  utilisation           {0:P1} (sum of thread time over "
                    "threads x wall)\n",
                    Sum / (double(Each.size()) * toSeconds(S.Elapsed)));
  return Error::success();
}

//===----------------------------------------------------------------------===//
// Reporting.
//===----------------------------------------------------------------------===//

void reportGraph(raw_ostream &OS, const NodeGraph &G, size_t NumRootIDs) {
  OS << "Source CAS\n";
  OS << "  path                  " << SourceCASPath << "\n";
  OS << "  roots walked          " << NumRootIDs - G.MissingRoots << "\n";
  if (G.MissingRoots)
    OS << "  roots missing         " << G.MissingRoots << "\n";
  OS << "  unique CAS nodes      " << G.size() << "\n";
  OS << "  unique object data    " << humanBytes(G.TotalDataBytes) << " ("
     << G.TotalDataBytes << " bytes)\n";
  OS << "  total references      " << G.TotalRefs << "\n";
  if (G.size()) {
    OS << "  mean refs per node    "
       << formatv("{0:F1}", double(G.TotalRefs) / double(G.size())) << "\n";
    OS << "  mean object size      " << humanBytes(G.TotalDataBytes / G.size())
       << "\n";
  }
  // LLVMCAS names a child by a compact internal offset. A digest-keyed store
  // has no such handle and must spell out the whole hash, so this is a floor
  // on what the alternatives have to write.
  OS << "  reference digests     " << humanBytes(G.TotalRefs * 32) << " ("
     << G.TotalRefs * 32 << " bytes at 32 B each)\n";
  OS << "  max DAG depth         " << G.MaxDepth << "\n";
  // Not a comparison metric: this is the one-off walk of the source CAS that
  // builds the node list, identical for every backend and excluded from both
  // the insert and traverse timings below. It is not comparable to the
  // traverse figure either, since it also builds the dedup map and ref index.
  OS << "  scan time (not timed as a backend result)\n";
  OS << "                        " << formatv("{0:F3} s", toSeconds(G.Elapsed))
     << "  (" << formatv("{0:F0}", perSecond(G.size(), G.Elapsed))
     << " nodes/s)\n";
}

/// Size histogram plus a prediction of what one-file-per-object would cost,
/// so a run that would not fit on the volume can be caught before it starts.
void reportSizeDistribution(raw_ostream &OS, const NodeGraph &G,
                            uint64_t BlockSize) {
  static const uint64_t Buckets[] = {256,       1024,       4096,    16384,
                                     64 * 1024, 256 * 1024, 1 << 20, 1 << 24};
  std::map<uint64_t, uint64_t> Counts;
  uint64_t Huge = 0;
  uint64_t PredictedFileBytes = 0;
  uint64_t BlobBytes = 0;

  for (size_t I = 0, E = G.size(); I != E; ++I) {
    // Matches KeyValueCAS's wire format: 4-byte count, 32 bytes per
    // reference, the data, and a terminator.
    uint64_t Blob = 4 + 32ull * G.Nodes[I].NumRefs + G.Nodes[I].Data.size() + 1;
    BlobBytes += Blob;
    PredictedFileBytes += roundUpTo(Blob, BlockSize);

    uint64_t Size = G.Nodes[I].Data.size();
    const uint64_t *B = llvm::lower_bound(Buckets, Size);
    if (B == std::end(Buckets))
      ++Huge;
    else
      ++Counts[*B];
  }

  OS << "\nObject size distribution (data bytes)\n";
  uint64_t Prev = 0;
  for (uint64_t B : Buckets) {
    auto It = Counts.find(B);
    uint64_t C = It == Counts.end() ? 0 : It->second;
    if (C)
      OS << formatv("  {0,10} .. {1,-10} {2,10}\n", Prev, B, C);
    Prev = B + 1;
  }
  if (Huge)
    OS << formatv("  {0,10} .. {1,-10} {2,10}\n", Prev, "inf", Huge);

  OS << "\n  objects >= 64 KiB go to standalone files in LLVMCAS "
        "(TrieRecord::MaxEmbeddedSize)\n";
  OS << "  key/value payload     " << humanBytes(BlobBytes)
     << " (data + 32 B per reference + 5 B per object)\n";
  OS << "  filesystem block size " << BlockSize << " bytes\n";
  OS << "  predicted 'file' size "
     << humanBytes(PredictedFileBytes + 256 * BlockSize) << " on disk ("
     << G.size() << " files + 256 dirs, block-rounded)\n";
}

void reportInsert(raw_ostream &OS, const InsertStats &S) {
  OS << "\nInsert\n";
  OS << "  nodes stored          " << S.Nodes << "\n";
  OS << "  data bytes            " << humanBytes(S.Bytes) << "\n";
  OS << "  elapsed               " << formatv("{0:F3} s", toSeconds(S.Elapsed))
     << "\n";
  OS << "  throughput            "
     << formatv("{0:F0} nodes/s", perSecond(S.Nodes, S.Elapsed)) << ", "
     << formatv("{0:F1} MiB/s", perSecond(S.Bytes, S.Elapsed) / (1024 * 1024))
     << "\n";
  OS << "  per node              "
     << formatv("{0:F0} ns", nanosEach(S.Nodes, S.Elapsed)) << "\n";
}

void reportTraverse(raw_ostream &OS, const TraverseStats &S) {
  OS << "\nTraverse\n";
  OS << "  nodes visited         " << S.Nodes << "\n";
  OS << "  edges followed        " << S.Edges << "\n";
  // The traversal decodes each object's references but does not read its
  // data, so this is the size the walk spanned, not bytes moved. Backends
  // that map data lazily never fault most of it in.
  OS << "  object data spanned   " << humanBytes(S.Bytes) << "  (not read)\n";
  OS << "  elapsed               " << formatv("{0:F3} s", toSeconds(S.Elapsed))
     << "\n";
  OS << "  throughput            "
     << formatv("{0:F0} nodes/s", perSecond(S.Nodes, S.Elapsed)) << "\n";
  OS << "  per node              "
     << formatv("{0:F0} ns", nanosEach(S.Nodes, S.Elapsed)) << "\n";
}

void reportSize(raw_ostream &OS, const DirSize &D,
                std::optional<uint64_t> Logical, uint64_t DataBytes) {
  OS << "\nSize on disk\n";
  OS << "  allocated             " << humanBytes(D.Allocated) << " ("
     << D.Allocated << " bytes)\n";
  OS << "  apparent              " << humanBytes(D.Apparent) << " ("
     << D.Apparent << " bytes)\n";
  if (Logical)
    OS << "  backend logical size  " << humanBytes(*Logical) << " (" << *Logical
       << " bytes)\n";
  OS << "  files / dirs          " << D.FileCount << " / " << D.DirCount
     << "\n";
  if (DataBytes)
    OS << "  amplification         "
       << formatv("{0:F3}x", double(D.Allocated) / double(DataBytes))
       << " vs unique object data\n";
  if (std::optional<uint64_t> RSS = getPeakRSS())
    OS << "  peak RSS              " << humanBytes(*RSS)
       << "  (backends that cannot point into a mapping must copy object "
          "data into memory)\n";
}

json::Object toJSON(StringRef BackendName, const NodeGraph *G,
                    const InsertStats *IS, const TraverseStats *TS,
                    const DirSize *DS, std::optional<uint64_t> Logical) {
  json::Object O{
      {"backend", BackendName},
      {"source_cas", SourceCASPath.getValue()},
      {"ids", IDsPath.getValue()},
      {"durable", Durable.getValue()},
  };
  if (G) {
    O["unique_nodes"] = int64_t(G->size());
    O["unique_data_bytes"] = int64_t(G->TotalDataBytes);
    O["total_refs"] = int64_t(G->TotalRefs);
    O["max_depth"] = int64_t(G->MaxDepth);
    O["scan_seconds"] = toSeconds(G->Elapsed);
  }
  if (IS)
    O["insert"] = json::Object{
        {"nodes", int64_t(IS->Nodes)},
        {"data_bytes", int64_t(IS->Bytes)},
        {"seconds", toSeconds(IS->Elapsed)},
        {"nodes_per_second", perSecond(IS->Nodes, IS->Elapsed)},
        {"ns_per_node", nanosEach(IS->Nodes, IS->Elapsed)},
    };
  if (TS)
    O["traverse"] = json::Object{
        {"threads", int64_t(TS->Threads)},
        {"nodes", int64_t(TS->Nodes)},
        {"edges", int64_t(TS->Edges)},
        {"data_bytes_spanned", int64_t(TS->Bytes)},
        {"seconds", toSeconds(TS->Elapsed)},
        {"nodes_per_second", perSecond(TS->Nodes, TS->Elapsed)},
        {"ns_per_node", nanosEach(TS->Nodes, TS->Elapsed)},
    };
  if (DS) {
    json::Object Size{
        {"allocated_bytes", int64_t(DS->Allocated)},
        {"apparent_bytes", int64_t(DS->Apparent)},
        {"files", int64_t(DS->FileCount)},
        {"dirs", int64_t(DS->DirCount)},
    };
    if (Logical)
      Size["logical_bytes"] = int64_t(*Logical);
    O["size"] = std::move(Size);
  }
  if (std::optional<uint64_t> RSS = getPeakRSS())
    O["peak_rss_bytes"] = int64_t(*RSS);
  return O;
}

/// Thread counts to sweep: powers of two up to the core count, plus the core
/// count itself when it is not a power of two.
std::vector<unsigned> getThreadCounts() {
  std::vector<unsigned> Out;
  if (!ThreadCounts.empty()) {
    SmallVector<StringRef, 8> Parts;
    StringRef(ThreadCounts).split(Parts, ',', -1, /*KeepEmpty=*/false);
    for (StringRef P : Parts) {
      unsigned N = 0;
      if (!P.trim().getAsInteger(10, N) && N)
        Out.push_back(N);
    }
    return Out;
  }
  unsigned Max = std::max(1u, std::thread::hardware_concurrency());
  for (unsigned N = 1; N <= Max; N *= 2)
    Out.push_back(N);
  if (Out.back() != Max)
    Out.push_back(Max);
  return Out;
}

void reportParallel(raw_ostream &OS, ArrayRef<ParallelResult> Results) {
  OS << "\nParallel insert\n";
  OS << "  objects per run       " << NumObjects << " x " << ObjectSize
     << " B = " << humanBytes(NumObjects * uint64_t(ObjectSize)) << "\n";
  OS << "  cores available       " << std::thread::hardware_concurrency()
     << "\n\n";
  OS << "  threads    elapsed    objects/s      MiB/s   ns/object   speedup"
        "   efficiency\n";
  double Base = Results.empty() ? 0 : toSeconds(Results.front().Elapsed);
  for (const ParallelResult &R : Results) {
    double S = toSeconds(R.Elapsed);
    double Speedup = S > 0 ? Base / S : 0;
    OS << formatv("  {0,7}  {1,8:F3}s  {2,11:F0}  {3,9:F1}  {4,10:F0}  "
                  "{5,7:F2}x  {6,10:F0}%\n",
                  R.Threads, S, perSecond(R.Objects, R.Elapsed),
                  perSecond(R.Bytes, R.Elapsed) / (1024 * 1024),
                  nanosEach(R.Objects, R.Elapsed), Speedup,
                  R.Threads ? 100.0 * Speedup / R.Threads : 0.0);
  }
}

BackendFactory getFactory(BackendKind Kind) {
  switch (Kind) {
  case BackendKind::LLVMCAS:
    return createLLVMCAS;
  case BackendKind::SQLite:
    return createSQLiteCAS;
  case BackendKind::File:
    return createFileCAS;
  case BackendKind::LMDB:
    return createLMDBCAS;
  case BackendKind::None:
    return nullptr;
  }
  llvm_unreachable("unhandled backend kind");
}

StringRef getBackendName(BackendKind Kind) {
  switch (Kind) {
  case BackendKind::LLVMCAS:
    return "llvmcas";
  case BackendKind::SQLite:
    return "sqlite";
  case BackendKind::File:
    return "file";
  case BackendKind::LMDB:
    return "lmdb";
  case BackendKind::None:
    return "none";
  }
  llvm_unreachable("unhandled backend kind");
}

/// Ask a backend how big it thinks it is, from a fresh read-only open.
///
/// It has to be a fresh open: a store may still be holding an uncommitted
/// transaction or an unflushed mapping while the writing instance is alive,
/// and ObjectStore has no flush short of destruction. LMDB in particular
/// reports two pages until its write transaction commits.
std::optional<uint64_t> probeLogicalSize(BackendFactory Factory, StringRef Dir,
                                         const BackendOptions &Opts) {
  Expected<std::unique_ptr<ObjectStore>> CAS =
      Factory(Dir, /*ForWrite=*/false, Opts);
  if (!CAS) {
    consumeError(CAS.takeError());
    return std::nullopt;
  }
  Expected<std::optional<uint64_t>> Size = (*CAS)->getStorageSize();
  if (!Size) {
    consumeError(Size.takeError());
    return std::nullopt;
  }
  return *Size;
}

} // end anonymous namespace

int main(int Argc, char **Argv) {
  InitLLVM X(Argc, Argv);
  cl::HideUnrelatedOptions(BenchCat);
  cl::ParseCommandLineOptions(
      Argc, Argv,
      "Compare LLVMCAS against alternative storage backends.\n\n"
      "Collects the DAG reachable from a list of root CASIDs in an existing\n"
      "LLVMCAS, replays it into the selected backend, and reports insert and\n"
      "traversal timings plus the resulting size on disk. Every backend is an\n"
      "ObjectStore, so all of them are driven through the same CAS API.\n");

  ExitOnError ExitOnErr("llvm-cas-bench: ");

  if (ParallelInsertMode) {
    if (OutPath.empty())
      ExitOnErr(createStringError(errc::invalid_argument,
                                  "--out is required for --parallel-insert"));
    // Only LLVMCAS is measured here. OnDiskGraphDB is built for concurrent
    // writers; a KeyValueCAS is not thread-safe at all.
    if (WhichBackend != BackendKind::LLVMCAS)
      ExitOnErr(createStringError(
          errc::invalid_argument,
          "--parallel-insert supports only --backend=llvmcas"));
    if (ObjectSize < sizeof(uint64_t))
      ExitOnErr(createStringError(errc::invalid_argument,
                                  "--object-size must be at least 8"));

    BackendOptions POpts;
    POpts.Durable = Durable;
    std::vector<ParallelResult> Results;
    for (unsigned T : getThreadCounts()) {
      // A fresh store per run, so every insert is into an empty index and the
      // runs are comparable.
      SmallString<256> Dir(OutPath);
      sys::path::append(Dir, formatv("t{0}", T).str());
      sys::fs::remove_directories(Dir);
      if (std::error_code EC = sys::fs::create_directories(Dir))
        ExitOnErr(createFileError(Dir, EC));

      ParallelResult R;
      {
        std::unique_ptr<ObjectStore> CAS =
            ExitOnErr(createLLVMCAS(Dir, /*ForWrite=*/true, POpts));
        ExitOnErr(parallelInsert(*CAS, T, NumObjects, ObjectSize, R));
      }
      Results.push_back(R);
      sys::fs::remove_directories(Dir);
      outs() << formatv("  ran {0,3} thread(s): {1:F3} s\n", T,
                        toSeconds(R.Elapsed));
    }
    reportParallel(outs(), Results);

    if (!JSONPath.empty()) {
      std::error_code EC;
      raw_fd_ostream JOS(JSONPath, EC);
      if (EC)
        ExitOnErr(createFileError(JSONPath, EC));
      json::Array Runs;
      for (const ParallelResult &R : Results)
        Runs.push_back(json::Object{
            {"threads", int64_t(R.Threads)},
            {"objects", int64_t(R.Objects)},
            {"bytes", int64_t(R.Bytes)},
            {"seconds", toSeconds(R.Elapsed)},
            {"objects_per_second", perSecond(R.Objects, R.Elapsed)},
            {"ns_per_object", nanosEach(R.Objects, R.Elapsed)},
        });
      json::Object O{{"mode", "parallel-insert"},
                     {"backend", "llvmcas"},
                     {"object_size", int64_t(ObjectSize)},
                     {"cores", int64_t(std::thread::hardware_concurrency())},
                     {"runs", std::move(Runs)}};
      JOS << formatv("{0:2}", json::Value(std::move(O))) << "\n";
    }
    return 0;
  }

  if (IDsPath.empty())
    ExitOnErr(createStringError(errc::invalid_argument, "--ids is required"));

  // Default to running both phases.
  bool DoInsert = DoInsertOpt || !DoTraverseOpt;
  bool DoTraverse = DoTraverseOpt || !DoInsertOpt;
  bool NeedSource = StatsOnly || DoInsert;

  if (StatsOnly)
    DoInsert = DoTraverse = false;
  if (NeedSource && SourceCASPath.empty())
    ExitOnErr(createStringError(errc::invalid_argument,
                                "--cas is required for this mode"));
  if (!StatsOnly && WhichBackend == BackendKind::None)
    ExitOnErr(createStringError(errc::invalid_argument,
                                "--backend is required (or use --stats-only)"));
  if (!StatsOnly && OutPath.empty())
    ExitOnErr(createStringError(errc::invalid_argument,
                                "--out is required (or use --stats-only)"));

  BackendOptions Opts;
  Opts.Durable = Durable;
  Opts.BatchSize = BatchSize;
  BackendFactory Factory = getFactory(WhichBackend);

  // The source CAS must be opened with the unified-database entry point: the
  // data lives in a versioned subdirectory, and createOnDiskCAS would instead
  // create a fresh empty database in the top-level directory.
  std::unique_ptr<ObjectStore> Source;
  std::optional<NodeGraph> Graph;
  std::vector<CASID> RootIDs;

  if (NeedSource) {
    Source = ExitOnErr(
        getFactory(SourceBackend)(SourceCASPath, /*ForWrite=*/false, Opts));
    RootIDs = ExitOnErr(readRootIDs(*Source, IDsPath));
    Graph = ExitOnErr(collectGraph(*Source, RootIDs));
    reportGraph(outs(), *Graph, RootIDs.size());
  }

  if (StatsOnly) {
    reportSizeDistribution(
        outs(), *Graph,
        getBlockSize(OutPath.empty() ? SourceCASPath : OutPath));
    return 0;
  }

  outs() << "\nBackend\n";
  outs() << "  name                  " << getBackendName(WhichBackend) << "\n";
  outs() << "  output                " << OutPath << "\n";
  outs() << "  mode                  "
         << (Durable ? "durable (each store's own fsync policy)"
                     : "parity (no fsync, matching LLVMCAS)")
         << "\n";
  if (BatchSize)
    outs() << "  batch                 " << BatchSize << " inserts\n";

  InsertStats IS;
  TraverseStats TS;

  if (DoInsert) {
    if (std::error_code EC = sys::fs::create_directories(OutPath))
      ExitOnErr(createFileError(OutPath, EC));
    std::unique_ptr<ObjectStore> Target =
        ExitOnErr(Factory(OutPath, /*ForWrite=*/true, Opts));
    ExitOnErr(insertAll(*Target, *Source, *Graph, IS));
    if (DumpBackend)
      Target->print(outs());
    // Destroying the store is the only flush ObjectStore offers, and it
    // matters for more than tidiness: LLVMCAS reserves its index and data
    // files sparsely and only truncates them back down to the bytes actually
    // used when the last reference is released.
    Target.reset();
    reportInsert(outs(), IS);
  }

  if (DoTraverse) {
    if (ParallelTraverse) {
      unsigned NT = TraverseThreads ? unsigned(TraverseThreads)
                                    : std::thread::hardware_concurrency();
      // CASIDs are portable between instances of a store, so any can parse
      // them; each thread then opens its own to walk with.
      if (RootIDs.empty()) {
        std::unique_ptr<ObjectStore> CAS =
            ExitOnErr(Factory(OutPath, /*ForWrite=*/false, Opts));
        RootIDs = ExitOnErr(readRootIDs(*CAS, IDsPath));
      }
      outs() << formatv("\nParallel traverse on {0} threads\n", NT);
      ExitOnErr(traverseAllParallel(Factory, OutPath, Opts, RootIDs, NT, TS));
    } else {
      std::unique_ptr<ObjectStore> Target =
          ExitOnErr(Factory(OutPath, /*ForWrite=*/false, Opts));
      if (RootIDs.empty())
        RootIDs = ExitOnErr(readRootIDs(*Target, IDsPath));
      ExitOnErr(traverseAll(*Target, RootIDs, TS));
      if (DumpBackend)
        Target->print(outs());
    }
    reportTraverse(outs(), TS);
  }

  std::optional<uint64_t> LogicalSize =
      probeLogicalSize(Factory, OutPath, Opts);
  DirSize DS = ExitOnErr(measureDirSize(OutPath));
  reportSize(outs(), DS, LogicalSize, Graph ? Graph->TotalDataBytes : 0);

  if (!JSONPath.empty()) {
    std::error_code EC;
    raw_fd_ostream JOS(JSONPath, EC);
    if (EC)
      ExitOnErr(createFileError(JSONPath, EC));
    json::Object O = toJSON(getBackendName(WhichBackend),
                            Graph ? &*Graph : nullptr, DoInsert ? &IS : nullptr,
                            DoTraverse ? &TS : nullptr, &DS, LogicalSize);
    JOS << formatv("{0:2}", json::Value(std::move(O))) << "\n";
  }

  return 0;
}
