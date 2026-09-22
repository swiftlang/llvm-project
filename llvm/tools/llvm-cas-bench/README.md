# llvm-cas-bench

Compares LLVMCAS's on-disk format against simpler alternatives on two axes:
**bytes actually charged on disk** and **DAG traversal speed**.

Input is a populated LLVMCAS plus a file of root CASIDs, one per line. The tool
collects the deduplicated DAG reachable from those roots, replays it into the
selected backend, reads it back, and reports timings and the resulting size.

## Design

**Every backend is an `llvm::cas::ObjectStore`.** The three alternatives derive
from `cas::builtin::BuiltinCAS` (`llvm/lib/CAS/BuiltinCAS.h`), using BLAKE3
hashing.

| `--backend=` | Storage |
|--------------|---------|
| `llvmcas`    | `createOnDiskUnifiedCASDatabases` — the implementation as-is, no adapter |
| `sqlite`     | One `(k BLOB PRIMARY KEY, v BLOB) WITHOUT ROWID` table in WAL mode, C API |
| `file`       | Git-style `<out>/ab/cdef…`: first digest byte is the subdirectory |
| `lmdb`       | One LMDB database, C API |

## Usage

```
llvm-cas-bench --cas <source-llvmcas> --ids <casid.txt>
               --backend={llvmcas|sqlite|file|lmdb} --out <dir>
               [--insert] [--traverse] [--stats-only]
               [--durable] [--verify] [--max-roots N] [--batch N]
               [--json <file>]
```

Start with `--stats-only`. It reports the node count, the reference-digest
volume, and a block-rounded prediction of what the `file` backend would cost,
without writing anything — worth checking before a full run, since one file per
object can be an order of magnitude larger than the data itself.

`--insert` and `--traverse` are separate phases so traversal can be measured in
its own process. With neither flag both run, which measures a **warm** traversal
against the page cache. For a cold number:

```
llvm-cas-bench ... --backend=lmdb --out /tmp/b --insert
sudo purge
llvm-cas-bench ... --backend=lmdb --out /tmp/b --traverse
```

`--traverse` on its own needs no `--cas`: root CASIDs are portable between
stores that share a hash schema, so they are parsed with the backend itself.

`--verify` checks that every object lands under the same CASID it has in the
source CAS. It exercises parse → traverse → re-hash → store end to end and is
cheap; use it whenever correctness is in question.

### Parallel traversal

`--parallel-traverse` splits the roots round-robin between
`--traverse-threads` threads (default: the core count). Each thread opens its
own instance of the store and shares nothing with the others, as separate
processes reading one cache would. There is no visited set, so threads redo
each other's work wherever roots share subgraphs. Opening and closing the
stores is not timed; elapsed is the slowest thread's walk, so an uneven split
shows up as lost throughput rather than being hidden.

## Running the benchmark

`run-bench.sh` does the whole thing: builds the tool in Release without
assertions, creates a throwaway APFS volume, runs all four backends, records
the machine and library versions, and prints a summary.

```
llvm/tools/llvm-cas-bench/run-bench.sh --corpus <dir>
```

`<dir>` holds `lmdb/`, the corpus as an LMDB store, and `casid.txt`, its
roots. LLVMCAS and LMDB run five rounds, interleaved, and the summary reports
medians and spreads; SQLite and the file backend run once, since a full pass
of the two takes hours. Traversal runs on one thread, on half the cores and
on all of them. Results land in `<build>/bench-results/<time>/`: one log and
one JSON per process, `environment.txt`, and `summary.md`, which
`summarize-bench.py <results-dir>` regenerates from the JSON. Off macOS, pass
`--scratch` with a directory on a filesystem that can be thrown away.

The volume is the point of the exercise, not a convenience. Deleting the file
backend's 5.8M files takes longer than the benchmark that creates them;
deleting a volume is O(1). The script only ever deletes a volume it made
itself, and turns Spotlight off on it so indexing does not compete with the
runs.

## Reading the size numbers

Two are reported, and they can differ by orders of magnitude:

- **allocated** — `st_blocks * 512` summed over the tree. This is the headline
  number: what the volume actually gives up. It matches `du -sk`.
- **apparent** — `st_size` summed. Misleading for backends that reserve sparse
  address space; LMDB reserves a 64 GiB map and will report that here.

Two properties of LLVMCAS shape the methodology:

1. It reserves 12 GB of index and 24 GB of data pool as sparse files and only
   truncates them back down to the bytes used when the last reference is
   released (`MappedFileRegionArena::destroyImpl`). The tool therefore always
   destroys a store before measuring, and nothing may read the directory in
   between.
2. Objects of 64 KiB or more are stored as standalone `leaf.*` files rather
   than in the data pool (`TrieRecord::MaxEmbeddedSize`).

## Building

The tool is discovered automatically by `add_llvm_implicit_projects()`; no
parent CMake edit is needed. It requires `LLVM_ENABLE_ONDISK_CAS=ON`, and adds
`llvm/lib/CAS` to its own include path to reach `BuiltinCAS.h`.

SQLite is located with CMake's builtin `find_package(SQLite3)`, which on macOS
finds the one in the SDK. **Which SQLite that is decides whether it can scale**,
because of a compile-time cap on how much of a database it will map,
`SQLITE_MAX_MMAP_SIZE`. `PRAGMA mmap_size` silently clamps to it, and pages
past it are read with `pread`, which on macOS does not get faster with more
threads on one file. macOS's own 3.54.0 caps the mapping at 1 GiB and
Homebrew's 3.53.4 at 2 GiB, both well under a corpus-sized store, so on either
a parallel walk stops scaling at a few threads. The backend prints a warning
when that is happening. Point the build at another SQLite with

```
cmake -DSQLite3_LIBRARY=/opt/homebrew/opt/sqlite/lib/libsqlite3.dylib \
      -DSQLite3_INCLUDE_DIR=/opt/homebrew/opt/sqlite/include .
```

and confirm with `otool -L bin/llvm-cas-bench | grep sqlite`. `run-bench.sh`
picks Homebrew's when it is installed, and records the library it linked in
`environment.txt`.

The backend turns SQLite's memory statistics off
(`SQLITE_CONFIG_MEMSTATUS`) before the library initialises. They take one
process-wide mutex around every allocation, and a lookup of a blob too big
for a B-tree page to hold locally allocates one; with the whole database
mapped, that mutex is what a parallel walk otherwise waits on.

LMDB has no find module anywhere in the monorepo, so `CMakeLists.txt` probes the
usual prefixes; pass `-DLMDB_ROOT=<prefix>` if it lives elsewhere. A missing
library makes only that backend fail at `--backend=`, with a clear message.
