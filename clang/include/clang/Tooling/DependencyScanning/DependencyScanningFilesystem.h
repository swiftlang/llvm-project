//===- DependencyScanningFilesystem.h - clang-scan-deps fs ===---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLING_DEPENDENCY_SCANNING_FILESYSTEM_H
#define LLVM_CLANG_TOOLING_DEPENDENCY_SCANNING_FILESYSTEM_H

#include "clang/Basic/LLVM.h"
#include "clang/Lex/PreprocessorExcludedConditionalDirectiveSkipMapping.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ThreadSafeFileSystem.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/VirtualFileSystem.h"
#include <mutex>

namespace llvm {
namespace cas {
class CachingOnDiskFileSystem;
} // namespace cas
} // namespace llvm

namespace clang {
namespace tooling {
namespace dependencies {

/// A virtual file system optimized for the dependency discovery.
///
/// It is primarily designed to work with source files whose contents was was
/// preprocessed to remove any tokens that are unlikely to affect the dependency
/// computation.
///
/// This is not a thread safe VFS. A single instance is meant to be used only in
/// one thread. Multiple instances are allowed to service multiple threads
/// running in parallel.
class DependencyScanningWorkerFilesystem : public llvm::cas::CASFileSystemBase {
public:
  DependencyScanningWorkerFilesystem(
      IntrusiveRefCntPtr<llvm::cas::CachingOnDiskFileSystem> WorkerFS,
      ExcludedPreprocessorDirectiveSkipMapping *PPSkipMappings);

  ~DependencyScanningWorkerFilesystem();

  llvm::cas::CASDB &getCAS() const override { return FS->getCAS(); }
  Optional<llvm::cas::CASID> getFileCASID(const Twine &Path) override;

  // FIXME: Make a templated version of ProxyFileSystem with a configurable
  // base class.
  llvm::vfs::directory_iterator dir_begin(const Twine &Dir, std::error_code &EC) override {
    return FS->dir_begin(Dir, EC);
  }
  llvm::ErrorOr<std::string> getCurrentWorkingDirectory() const override {
    return FS->getCurrentWorkingDirectory();
  }
  std::error_code setCurrentWorkingDirectory(const Twine &Path) override {
    return FS->setCurrentWorkingDirectory(Path);
  }
  std::error_code getRealPath(const Twine &Path,
                              SmallVectorImpl<char> &Output) const override {
    return FS->getRealPath(Path, Output);
  }
  std::error_code isLocal(const Twine &Path, bool &Result) override {
    return FS->isLocal(Path, Result);
  }

  IntrusiveRefCntPtr<llvm::cas::ThreadSafeFileSystem>
  createThreadSafeProxyFS() override;

  llvm::ErrorOr<llvm::vfs::Status> status(const Twine &Path) override;
  llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>>
  openFileForRead(const Twine &Path) override;

  void clearIgnoredFiles() { IgnoredFiles.clear(); }
  void ignoreFile(StringRef Filename);

private:
  bool shouldIgnoreFile(StringRef Filename);

  IntrusiveRefCntPtr<llvm::cas::CASFileSystemBase> FS;

  struct Entry {
    std::error_code EC; // If non-zero, caches a stat failure.
    Optional<StringRef> Buffer;
    llvm::vfs::Status Status;
    Optional<llvm::cas::CASID> ID;
  };
  llvm::BumpPtrAllocator EntryAlloc;
  llvm::StringMap<Entry, llvm::BumpPtrAllocator> Entries;

  struct LookupPathResult {
    const Entry *Entry = nullptr;

    // Only filled if the Entry is nullptr.
    llvm::ErrorOr<llvm::vfs::Status> Status;
  };
  Expected<StringRef>
  computeMinimized(llvm::cas::CASID InputDataID, StringRef Identifier,
                   Optional<llvm::cas::CASID> &MinimizedDataID);

  Expected<StringRef> getMinimized(llvm::cas::CASID OutputID,
                                   StringRef Identifier,
                                   Optional<llvm::cas::CASID> &MinimizedDataID);

  Expected<StringRef> getOriginal(llvm::cas::CASID InputDataID);

  LookupPathResult lookupPath(const Twine &Path);

  llvm::cas::CachingOnDiskFileSystem &getCachingFS();

  llvm::cas::CASDB &CAS;
  Optional<llvm::cas::CASID> ClangFullVersionID;
  Optional<llvm::cas::CASID> MinimizeID;
  Optional<llvm::cas::CASID> EmptyBlobID;

  /// The optional mapping structure which records information about the
  /// excluded conditional directive skip mappings that are used by the
  /// currently active preprocessor.
  ExcludedPreprocessorDirectiveSkipMapping *PPSkipMappings;
  /// The set of files that should not be minimized.
  llvm::StringSet<> IgnoredFiles;
};

} // end namespace dependencies
} // end namespace tooling
} // end namespace clang

#endif // LLVM_CLANG_TOOLING_DEPENDENCY_SCANNING_FILESYSTEM_H
