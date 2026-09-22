//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// The baseline: LLVMCAS exactly as it ships, with no adapter of any kind.
///
//===----------------------------------------------------------------------===//

#include "CASBackends.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/BuiltinUnifiedCASDatabases.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casbench;

Expected<std::unique_ptr<ObjectStore>>
llvm::casbench::createLLVMCAS(StringRef Dir, bool ForWrite,
                              const BackendOptions &Opts) {
  // LLVMCAS has no read-only mode and no durability knob: it maps its files
  // read/write and syncs when the last reference is released.
  (void)ForWrite, (void)Opts;

  std::pair<std::unique_ptr<ObjectStore>, std::unique_ptr<ActionCache>> DBs;
  if (Error E = createOnDiskUnifiedCASDatabases(Dir).moveInto(DBs))
    return std::move(E);
  // The action cache is not exercised here; the ObjectStore keeps whatever
  // shared state it needs alive on its own.
  return std::move(DBs.first);
}
