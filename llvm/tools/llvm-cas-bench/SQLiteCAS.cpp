//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// An ObjectStore backed by SQLite.
///
//===----------------------------------------------------------------------===//

#include "CASBackends.h"

#if CASBENCH_HAVE_SQLITE

#include "KeyValueCAS.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <sqlite3.h>

#include <mutex>
#include <string>

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casbench;

namespace {

class SQLiteCAS : public KeyValueCAS {
public:
  SQLiteCAS(const BackendOptions &Opts) : Opts(Opts) {}

  ~SQLiteCAS() override {
    sqlite3_finalize(InsertStmt);
    sqlite3_finalize(SelectStmt);
    sqlite3_finalize(ExistsStmt);
    if (InTransaction)
      consumeError(exec("COMMIT"));
    sqlite3_close(DB);
  }

  Error open(StringRef Dir, bool ForWrite);

  Error putBlob(ArrayRef<uint8_t> Key, ArrayRef<uint8_t> Blob) final {
    sqlite3_bind_blob(InsertStmt, 1, Key.data(), int(Key.size()),
                      SQLITE_STATIC);
    sqlite3_bind_blob64(InsertStmt, 2, Blob.data(), sqlite3_uint64(Blob.size()),
                        SQLITE_STATIC);
    int RC = sqlite3_step(InsertStmt);
    sqlite3_reset(InsertStmt);
    if (RC != SQLITE_DONE)
      return makeError("insert", RC);

    if (Opts.BatchSize && ++SinceCommit == Opts.BatchSize) {
      if (Error E = exec("COMMIT"))
        return E;
      if (Error E = exec("BEGIN"))
        return E;
      SinceCommit = 0;
    }
    return Error::success();
  }

  Expected<std::optional<ArrayRef<uint8_t>>>
  getBlob(ArrayRef<uint8_t> Key) final {
    sqlite3_bind_blob(SelectStmt, 1, Key.data(), int(Key.size()),
                      SQLITE_STATIC);
    auto Reset = scope_exit([&] { sqlite3_reset(SelectStmt); });

    int RC = sqlite3_step(SelectStmt);
    if (RC == SQLITE_DONE)
      return std::nullopt;
    if (RC != SQLITE_ROW)
      return makeError("select", RC);

    ArrayRef<uint8_t> Blob(
        reinterpret_cast<const uint8_t *>(sqlite3_column_blob(SelectStmt, 0)),
        size_t(sqlite3_column_bytes(SelectStmt, 0)));
    // SQLite's blob pointer dies at the reset below, before the caller has
    // read it, so this has to be copied. LMDB and LLVMCAS both hand out
    // pointers into a mapping and avoid it.
    return copyToScratch(Blob);
  }

  Expected<bool> containsBlob(ArrayRef<uint8_t> Key) final {
    sqlite3_bind_blob(ExistsStmt, 1, Key.data(), int(Key.size()),
                      SQLITE_STATIC);
    auto Reset = scope_exit([&] { sqlite3_reset(ExistsStmt); });
    int RC = sqlite3_step(ExistsStmt);
    if (RC == SQLITE_ROW)
      return true;
    if (RC == SQLITE_DONE)
      return false;
    return makeError("exists", RC);
  }

  Expected<std::optional<uint64_t>> getStorageSize() const final {
    std::optional<uint64_t> Pages = queryScalar("PRAGMA page_count");
    std::optional<uint64_t> PageSize = queryScalar("PRAGMA page_size");
    if (!Pages || !PageSize)
      return std::nullopt;
    return *Pages * *PageSize;
  }

private:
  Error exec(const char *SQL) {
    char *Msg = nullptr;
    if (int RC = sqlite3_exec(DB, SQL, nullptr, nullptr, &Msg)) {
      std::string Detail = Msg ? Msg : "unknown error";
      sqlite3_free(Msg);
      return createStringError(errc::io_error, "sqlite: %s: %s (%d)", SQL,
                               Detail.c_str(), RC);
    }
    if (StringRef(SQL) == "BEGIN")
      InTransaction = true;
    else if (StringRef(SQL) == "COMMIT")
      InTransaction = false;
    return Error::success();
  }

  std::optional<uint64_t> queryScalar(const char *SQL) const {
    if (!DB)
      return std::nullopt;
    sqlite3_stmt *Stmt = nullptr;
    if (sqlite3_prepare_v2(DB, SQL, -1, &Stmt, nullptr))
      return std::nullopt;
    auto Finalize = scope_exit([&] { sqlite3_finalize(Stmt); });
    if (sqlite3_step(Stmt) != SQLITE_ROW)
      return std::nullopt;
    return uint64_t(sqlite3_column_int64(Stmt, 0));
  }

  /// Say so, once per process, when the database is bigger than SQLite will
  /// map.
  ///
  /// The cap is compiled in (SQLITE_MAX_MMAP_SIZE), and \c PRAGMA mmap_size
  /// silently clamps to it. Pages past it are read with pread into a page
  /// cache whose one mutex every connection shares when the library is built
  /// with SQLITE_ENABLE_MEMORY_MANAGEMENT. Both serialise readers: pread on
  /// one file does not get faster with more threads on macOS. A walk of such
  /// a database therefore stops scaling at a few threads, whatever this
  /// backend does.
  void warnIfPartlyMapped() const {
    std::optional<uint64_t> Mapped = queryScalar("PRAGMA mmap_size");
    Expected<std::optional<uint64_t>> Size = getStorageSize();
    if (!Size) {
      consumeError(Size.takeError());
      return;
    }
    if (!Mapped || !*Size || *Mapped >= **Size)
      return;
    static std::once_flag Once;
    std::call_once(Once, [&] {
      errs() << formatv("warning: SQLite {0} maps at most {1} bytes of this "
                        "{2}-byte database; the rest is read with pread, "
                        "which does not scale across threads\n",
                        sqlite3_libversion(), *Mapped, **Size);
    });
  }

  Error makeError(const char *What, int RC) const {
    return createStringError(errc::io_error, "sqlite: %s: %s (%d)", What,
                             DB ? sqlite3_errmsg(DB) : sqlite3_errstr(RC), RC);
  }

  BackendOptions Opts;
  sqlite3 *DB = nullptr;
  sqlite3_stmt *InsertStmt = nullptr;
  sqlite3_stmt *SelectStmt = nullptr;
  sqlite3_stmt *ExistsStmt = nullptr;
  uint64_t SinceCommit = 0;
  bool InTransaction = false;
};

Error SQLiteCAS::open(StringRef Dir, bool ForWrite) {
  SmallString<256> Path(Dir);
  sys::path::append(Path, "cas.sqlite");

  int Flags = ForWrite ? (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
                       : SQLITE_OPEN_READONLY;
  if (int RC = sqlite3_open_v2(Path.c_str(), &DB, Flags, nullptr))
    return makeError("open", RC);
  // A parallel traversal opens one connection per thread at once, and the
  // first to reach a WAL database rebuilds its index while the rest are
  // turned away as busy. Wait instead; readers never block each other once
  // they are walking.
  sqlite3_busy_timeout(DB, 60 * 1000);

  // SQLite's 2 MB default page cache thrashes, so ask for 256 MiB to insert
  // with and 64 MiB per reader. A reader's is modest because the mapping
  // below is what its reads should come from, and that is shared between
  // connections; twenty readers each asking for 256 MiB and a 64 GiB mapping
  // wedge the process. Negative means kibibytes.
  if (Error E = exec(ForWrite ? "PRAGMA cache_size=-262144"
                              : "PRAGMA cache_size=-65536"))
    return E;
  // Read pages out of a mapping instead of issuing a pread per page. Without
  // this SQLite would be the only backend here paying a syscall per page,
  // which measures its I/O default rather than its data structure. SQLite
  // clamps this to its compiled-in maximum.
  if (Error E = exec(ForWrite ? "PRAGMA mmap_size=68719476736"
                              : "PRAGMA mmap_size=8589934592"))
    return E;
  if (!ForWrite)
    warnIfPartlyMapped();

  if (ForWrite) {
    // page_size only takes effect before the first table is created.
    if (Error E = exec("PRAGMA page_size=4096"))
      return E;
    // WAL in both modes, because it is the mode that lets readers and a
    // writer run at once, which is what a shared cache needs. Without it a
    // writer has to wait for every reader to finish before it can commit, and
    // readers wait for the commit in turn. A read-only walk measures the same
    // either way; insert writes each page twice, to the log and then back.
    if (Error E = exec("PRAGMA journal_mode=WAL"))
      return E;
    // Without durability, match LLVMCAS, which does not fsync per object.
    if (Error E = exec(Opts.Durable ? "PRAGMA synchronous=FULL"
                                    : "PRAGMA synchronous=OFF"))
      return E;
    // WITHOUT ROWID stores the payload directly in the primary-key B-tree.
    // A rowid table would keep a second B-tree plus a unique index over the
    // digest, inflating the file substantially for a pure key/value load.
    if (Error E = exec("CREATE TABLE IF NOT EXISTS obj("
                       "  k BLOB PRIMARY KEY,"
                       "  v BLOB NOT NULL"
                       ") WITHOUT ROWID"))
      return E;
  }

  if (ForWrite &&
      sqlite3_prepare_v2(DB, "INSERT OR IGNORE INTO obj(k,v) VALUES(?,?)", -1,
                         &InsertStmt, nullptr))
    return makeError("prepare insert", SQLITE_ERROR);
  if (sqlite3_prepare_v2(DB, "SELECT v FROM obj WHERE k=?", -1, &SelectStmt,
                         nullptr))
    return makeError("prepare select", SQLITE_ERROR);
  if (sqlite3_prepare_v2(DB, "SELECT 1 FROM obj WHERE k=?", -1, &ExistsStmt,
                         nullptr))
    return makeError("prepare exists", SQLITE_ERROR);

  // One transaction for the life of the connection: the write, or a read.
  //
  // Without a read transaction every SELECT is its own implicit one, so each
  // lookup takes and drops an fcntl shared lock on the database file. One
  // reader doing that a few million times is survivable; twenty doing it a
  // hundred million times spend nearly all their time in the kernel
  // contending for the same lock, and the walk runs slower than it does on
  // one thread. Holding the transaction open takes the lock once.
  return exec("BEGIN");
}

} // end anonymous namespace

#endif // CASBENCH_HAVE_SQLITE

Expected<std::unique_ptr<ObjectStore>>
llvm::casbench::createSQLiteCAS(StringRef Dir, bool ForWrite,
                                const BackendOptions &Opts) {
#if CASBENCH_HAVE_SQLITE
  // Keeping allocation statistics takes one process-wide mutex around every
  // sqlite3_malloc and sqlite3_free, and a lookup of a blob bigger than a
  // B-tree page holds locally mallocs a buffer for it. With the database
  // mapped, that mutex is all twenty readers wait on. SQLite's own
  // recommended options turn it off; it has to be done before the library
  // initialises, which the first open does.
  static std::once_flag ConfigOnce;
  std::call_once(ConfigOnce,
                 [] { sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0); });
  auto CAS = std::make_unique<SQLiteCAS>(Opts);
  if (Error E = CAS->open(Dir, ForWrite))
    return std::move(E);
  return std::unique_ptr<ObjectStore>(std::move(CAS));
#else
  (void)Dir, (void)ForWrite, (void)Opts;
  return createStringError(errc::not_supported,
                           "this build has no SQLite support");
#endif
}
