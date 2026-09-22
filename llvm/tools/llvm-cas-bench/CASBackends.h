//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Every backend is an llvm::cas::ObjectStore, so the benchmark driver only
/// ever speaks the public CAS API.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_CAS_BENCH_CASBACKENDS_H
#define LLVM_TOOLS_LLVM_CAS_BENCH_CASBACKENDS_H

#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/Error.h"
#include <memory>

namespace llvm {
namespace casbench {

struct BackendOptions {
  /// When false, configure the backend the way LLVMCAS behaves: no fsync per
  /// object. When true, use the store's own durable settings.
  bool Durable = false;
  /// Commit every N inserts; 0 means one transaction for the whole run. Only
  /// meaningful for the transactional backends.
  uint64_t BatchSize = 0;
};

/// Open (creating if needed) the store rooted at \p Dir.
using BackendFactory = Expected<std::unique_ptr<cas::ObjectStore>> (*)(
    StringRef Dir, bool ForWrite, const BackendOptions &Opts);

Expected<std::unique_ptr<cas::ObjectStore>>
createLLVMCAS(StringRef Dir, bool ForWrite, const BackendOptions &Opts);

Expected<std::unique_ptr<cas::ObjectStore>>
createFileCAS(StringRef Dir, bool ForWrite, const BackendOptions &Opts);

/// Errors if the tool was built without SQLite.
Expected<std::unique_ptr<cas::ObjectStore>>
createSQLiteCAS(StringRef Dir, bool ForWrite, const BackendOptions &Opts);

/// Errors if the tool was built without LMDB.
Expected<std::unique_ptr<cas::ObjectStore>>
createLMDBCAS(StringRef Dir, bool ForWrite, const BackendOptions &Opts);

} // namespace casbench
} // namespace llvm

#endif // LLVM_TOOLS_LLVM_CAS_BENCH_CASBACKENDS_H
