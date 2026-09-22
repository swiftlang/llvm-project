//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// LMDB is a memory-mapped B+tree, so lookups hand back a pointer straight
/// into the map. That matches what ObjectStore expects of object data, and
/// makes this the closest structural analogue to LLVMCAS among the
/// alternatives here.
///
//===----------------------------------------------------------------------===//

#include "CASBackends.h"

#if CASBENCH_HAVE_LMDB

#include "KeyValueCAS.h"
#include "llvm/ADT/SmallString.h"

#include <lmdb.h>

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casbench;

namespace {

/// Address space reserved for the map. LMDB only reserves, so oversizing is
/// free; this is the hard ceiling on database size.
constexpr size_t MapSize = size_t(64) * 1024 * 1024 * 1024;

class LMDBCAS : public KeyValueCAS {
public:
  LMDBCAS(const BackendOptions &Opts) : Opts(Opts) {}

  ~LMDBCAS() override {
    if (Txn) {
      if (ReadOnly)
        mdb_txn_abort(Txn);
      else if (int RC = mdb_txn_commit(Txn))
        errs() << "lmdb: commit failed on close: " << mdb_strerror(RC) << "\n";
    }
    if (Env) {
      // Insert runs with MDB_NOSYNC, so this final sync is what makes the
      // data durable. A read-only environment cannot be synced.
      if (!ReadOnly)
        if (int RC = mdb_env_sync(Env, 1))
          errs() << "lmdb: sync failed on close: " << mdb_strerror(RC) << "\n";
      mdb_env_close(Env);
    }
  }

  Error open(StringRef Dir, bool ForWrite) {
    ReadOnly = !ForWrite;
    SmallString<256> Path(Dir);

    if (int RC = mdb_env_create(&Env))
      return makeError("mdb_env_create", RC);
    if (int RC = mdb_env_set_mapsize(Env, MapSize))
      return makeError("mdb_env_set_mapsize", RC);

    unsigned EnvFlags = 0;
    if (ReadOnly) {
      // A parallel traversal opens the same database once per thread, which
      // LMDB's locking cannot take: its locks are per process, so closing
      // one environment drops the others'. Nothing writes while readers
      // walk, which is when LMDB allows going without.
      EnvFlags |= MDB_RDONLY | MDB_NOLOCK;
    } else if (!Opts.Durable) {
      // Match LLVMCAS, which writes through a mapping and does not fsync per
      // object. MDB_WRITEMAP also lets writes go straight into the map.
      EnvFlags |= MDB_WRITEMAP | MDB_NOSYNC | MDB_NOMETASYNC | MDB_MAPASYNC;
    }
    if (int RC = mdb_env_open(Env, Path.c_str(), EnvFlags, 0644))
      return makeError("mdb_env_open", RC);

    // One transaction for the store's lifetime. Values from mdb_get() stay
    // valid as long as the reading transaction does, which is exactly the
    // lifetime ObjectStore requires of object data.
    if (int RC = mdb_txn_begin(Env, nullptr, ReadOnly ? MDB_RDONLY : 0, &Txn))
      return makeError("mdb_txn_begin", RC);
    if (int RC = mdb_dbi_open(Txn, nullptr, ForWrite ? MDB_CREATE : 0, &DBI))
      return makeError("mdb_dbi_open", RC);

    return Error::success();
  }

  Error putBlob(ArrayRef<uint8_t> Key, ArrayRef<uint8_t> Blob) final {
    MDB_val K{Key.size(), const_cast<uint8_t *>(Key.data())};
    MDB_val V{Blob.size(), const_cast<uint8_t *>(Blob.data())};
    int RC = mdb_put(Txn, DBI, &K, &V, MDB_NOOVERWRITE);
    if (RC == MDB_KEYEXIST)
      return Error::success(); // Content addressed: same key, same bytes.
    if (RC)
      return makeError("mdb_put", RC);

    if (Opts.BatchSize && ++SinceCommit == Opts.BatchSize) {
      // Committing invalidates every value pointer handed out by this
      // transaction, so batching is only safe while nothing has been loaded.
      if (int RC2 = mdb_txn_commit(Txn)) {
        Txn = nullptr;
        return makeError("mdb_txn_commit", RC2);
      }
      if (int RC2 = mdb_txn_begin(Env, nullptr, 0, &Txn)) {
        Txn = nullptr;
        return makeError("mdb_txn_begin", RC2);
      }
      SinceCommit = 0;
    }
    return Error::success();
  }

  Expected<std::optional<ArrayRef<uint8_t>>>
  getBlob(ArrayRef<uint8_t> Key) final {
    MDB_val K{Key.size(), const_cast<uint8_t *>(Key.data())};
    MDB_val V{};
    int RC = mdb_get(Txn, DBI, &K, &V);
    if (RC == MDB_NOTFOUND)
      return std::nullopt;
    if (RC)
      return makeError("mdb_get", RC);
    // Zero-copy: this aliases the mapped database pages.
    return ArrayRef<uint8_t>(static_cast<const uint8_t *>(V.mv_data),
                             V.mv_size);
  }

  Expected<bool> containsBlob(ArrayRef<uint8_t> Key) final {
    MDB_val K{Key.size(), const_cast<uint8_t *>(Key.data())};
    MDB_val V{};
    int RC = mdb_get(Txn, DBI, &K, &V);
    if (RC == MDB_NOTFOUND)
      return false;
    if (RC)
      return makeError("mdb_get", RC);
    return true;
  }

  Expected<std::optional<uint64_t>> getStorageSize() const final {
    MDB_envinfo Info;
    MDB_stat Stat;
    if (mdb_env_info(Env, &Info) || mdb_env_stat(Env, &Stat))
      return std::nullopt;
    // last_pgno is the highest page in use, so this is the live prefix of the
    // map rather than the deliberately huge reserved map size.
    return uint64_t(Info.me_last_pgno + 1) * uint64_t(Stat.ms_psize);
  }

  void print(raw_ostream &OS) const final {
    MDB_stat Stat;
    if (mdb_env_stat(Env, &Stat))
      return;
    OS << "LMDBCAS: depth " << Stat.ms_depth << ", " << Stat.ms_branch_pages
       << " branch / " << Stat.ms_leaf_pages << " leaf / "
       << Stat.ms_overflow_pages << " overflow pages of " << Stat.ms_psize
       << " bytes\n";
  }

private:
  Error makeError(const char *What, int RC) const {
    return createStringError(errc::io_error, "lmdb: %s: %s (%d)", What,
                             mdb_strerror(RC), RC);
  }

  BackendOptions Opts;
  MDB_env *Env = nullptr;
  MDB_txn *Txn = nullptr;
  MDB_dbi DBI = 0;
  uint64_t SinceCommit = 0;
  bool ReadOnly = false;
};

} // end anonymous namespace

#endif // CASBENCH_HAVE_LMDB

Expected<std::unique_ptr<ObjectStore>>
llvm::casbench::createLMDBCAS(StringRef Dir, bool ForWrite,
                              const BackendOptions &Opts) {
#if CASBENCH_HAVE_LMDB
  auto CAS = std::make_unique<LMDBCAS>(Opts);
  if (Error E = CAS->open(Dir, ForWrite))
    return std::move(E);
  return std::unique_ptr<ObjectStore>(std::move(CAS));
#else
  (void)Dir, (void)ForWrite, (void)Opts;
  return createStringError(errc::not_supported,
                           "this build has no LMDB support");
#endif
}
