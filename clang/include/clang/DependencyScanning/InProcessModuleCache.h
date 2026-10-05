//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_DEPENDENCYSCANNING_INPROCESSMODULECACHE_H
#define LLVM_CLANG_DEPENDENCYSCANNING_INPROCESSMODULECACHE_H

#include "clang/Serialization/ModuleCache.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace clang {
namespace dependencies {

struct ModuleCacheEntry {
  std::mutex Mutex;
  std::condition_variable CondVar;
  bool Locked = false;
  unsigned Generation = 0;

  std::atomic<std::time_t> Timestamp = 0;

  std::atomic<bool> DirectoriesValidated = false;
};

struct ModuleCacheEntries {
  std::mutex Mutex;
  llvm::StringMap<std::unique_ptr<ModuleCacheEntry>> Map;

  void addInvalidatedDirectories(llvm::ArrayRef<std::string> Dirs);
  bool isDirectoryInvalidated(StringRef Directory) const;
  bool hasInvalidatedDirectories() const {
    return AnyInvalidatedDirs.load(std::memory_order_acquire);
  }

private:
  mutable std::mutex InvalidatedDirsMutex;
  llvm::StringSet<> InvalidatedDirs;
  std::atomic<bool> AnyInvalidatedDirs = false;
};

std::shared_ptr<ModuleCache>
makeInProcessModuleCache(ModuleCacheEntries &Entries);

} // namespace dependencies
} // namespace clang

#endif // LLVM_CLANG_DEPENDENCYSCANNING_INPROCESSMODULECACHE_H
