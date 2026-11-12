//===- llvm/CAS/TracingCASBackedFileSystem.h --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_TRACINGCASBACKEDFILESYSTEM_H
#define LLVM_CAS_TRACINGCASBACKEDFILESYSTEM_H

#include "llvm/CAS/CASFileSystem.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/VirtualFileSystem.h"

namespace llvm::cas {

/// A \c CASBackedFileSystem that counts the calls made to it, like
/// \c vfs::TracingFileSystem does for a plain filesystem.
///
/// The counts are kept in a \c vfs::TracingFileSystem over the underlying
/// filesystem, so existing clients of that type can read them.
/// \c getBufferAndObjectRefForFile() opens the file, so it counts as a call to
/// \c openFileForRead().
class LLVM_ABI TracingCASBackedFileSystem final
    : public RTTIExtends<TracingCASBackedFileSystem, CASBackedFileSystem> {
public:
  static const char ID;

  explicit TracingCASBackedFileSystem(
      IntrusiveRefCntPtr<CASBackedFileSystem> FS);

  /// The tracing filesystem that holds the counts.
  vfs::TracingFileSystem &getTracingFS() const { return *Tracing; }

  ErrorOr<vfs::Status> status(const Twine &Path) override;
  bool exists(const Twine &Path) override;
  ErrorOr<std::unique_ptr<vfs::File>>
  openFileForRead(const Twine &Path) override;
  vfs::directory_iterator dir_begin(const Twine &Dir,
                                    std::error_code &EC) override;
  ErrorOr<std::string> getCurrentWorkingDirectory() const override;
  std::error_code setCurrentWorkingDirectory(const Twine &Path) override;
  std::error_code getRealPath(const Twine &Path,
                              SmallVectorImpl<char> &Output) override;
  void
  getDirectoryContentRealSources(const Twine &Dir,
                                 SmallVectorImpl<std::string> &Out) override;
  std::error_code isLocal(const Twine &Path, bool &Result) override;
  Expected<const vfs::CachedDirectoryEntry *>
  getDirectoryEntry(const Twine &Path, bool FollowSymlinks) const override;
  bool isCASFS() const override;
  void visitChildFileSystems(VisitCallbackTy Callback) override;

  Expected<std::pair<std::unique_ptr<MemoryBuffer>, ObjectRef>>
  getBufferAndObjectRefForFile(const Twine &Name, int64_t FileSize,
                               bool RequiresNullTerminator, bool IsVolatile,
                               bool IsText) override;
  ObjectStore &getCAS() const override;

  /// Returns a tracing filesystem over a thread-safe proxy of the underlying
  /// filesystem. Its counts are separate from the counts of this one.
  IntrusiveRefCntPtr<CASBackedFileSystem> createThreadSafeProxyFS() override;

protected:
  void printImpl(raw_ostream &OS, PrintType Type,
                 unsigned IndentLevel) const override;

private:
  IntrusiveRefCntPtr<CASBackedFileSystem> FS;
  IntrusiveRefCntPtr<vfs::TracingFileSystem> Tracing;
};

} // namespace llvm::cas

#endif // LLVM_CAS_TRACINGCASBACKEDFILESYSTEM_H
