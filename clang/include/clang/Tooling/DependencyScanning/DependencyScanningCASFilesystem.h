//===- DependencyScanningCASFilesystem.h - clang-scan-deps fs ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLING_DEPENDENCYSCANNING_DEPENDENCYSCANNINGCASFILESYSTEM_H
#define LLVM_CLANG_TOOLING_DEPENDENCYSCANNING_DEPENDENCYSCANNINGCASFILESYSTEM_H

#include "clang/Basic/LLVM.h"
#include "clang/Lex/PreprocessorExcludedConditionalDirectiveSkipMapping.h"
#include "clang/Tooling/DependencyScanning/DependencyScanningFilesystem.h"
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

class DependencyScanningCASFilesystem : public llvm::cas::CASFileSystemBase {
public:
  DependencyScanningCASFilesystem(
      IntrusiveRefCntPtr<llvm::cas::CachingOnDiskFileSystem> WorkerFS,
      ExcludedPreprocessorDirectiveSkipMapping *PPSkipMappings);

  ~DependencyScanningCASFilesystem();

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

  /// Disable minimization of the given file.
  void disableMinimization(StringRef Filename);
  /// Enable minimization of all files.
  void enableMinimizationOfAllFiles() { NotToBeMinimized.clear(); }

private:
  /// Check whether the file should be minimized.
  bool shouldMinimize(StringRef Filename);

  IntrusiveRefCntPtr<llvm::cas::CASFileSystemBase> FS;

  struct FileEntry {
    std::error_code EC; // If non-zero, caches a stat failure.
    Optional<StringRef> Buffer;
    llvm::vfs::Status Status;
    Optional<llvm::cas::CASID> ID;
  };
  llvm::BumpPtrAllocator EntryAlloc;
  llvm::StringMap<FileEntry, llvm::BumpPtrAllocator> Entries;

  struct LookupPathResult {
    const FileEntry *Entry = nullptr;

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
  llvm::StringSet<> NotToBeMinimized;
};

} // end namespace dependencies
} // end namespace tooling
} // end namespace clang

#endif // LLVM_CLANG_TOOLING_DEPENDENCYSCANNING_DEPENDENCYSCANNINGCASFILESYSTEM_H
