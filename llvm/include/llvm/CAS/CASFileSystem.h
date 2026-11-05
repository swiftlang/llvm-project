//===- llvm/CAS/CASFileSystem.h -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASFILESYSTEM_H
#define LLVM_CAS_CASFILESYSTEM_H

#include "llvm/Support/Compiler.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ExtensibleRTTI.h"
#include "llvm/Support/VirtualFileSystem.h"

namespace llvm::cas {
class ObjectStore;
class CASID;

/// Abstract class represents a CAS backed file system.
///
/// CAS-awareness deliberately lives here, on the *filesystem*, and not on
/// \c vfs::File. A \c vfs::File is a transient object that any VFS decorator is
/// free to wrap: \c RedirectingFileSystem wraps it to fix up the reported path,
/// and nothing stops a future decorator from doing the same. A CAS reference
/// attached to a \c vfs::File therefore cannot be recovered reliably from the
/// top of a filesystem stack, and making \c vfs::File an extensible-RTTI root
/// so that it could be is both a core-LLVM change and a standing invitation for
/// any new decorator to silently disable CAS caching. \c vfs::FileSystem, by
/// contrast, is already an extensible-RTTI root upstream, so \c dyn_cast to
/// this class needs no change to core LLVM.
///
/// The consequence for callers is that a CAS reference must be obtained from
/// the filesystem that actually produced the bytes, at the moment it produces
/// them -- see \c getBufferAndObjectRefForFile -- rather than re-derived later
/// from a path or from an already-opened file.
class CASBackedFileSystem
    : public llvm::RTTIExtends<CASBackedFileSystem, vfs::FileSystem> {
public:
  LLVM_ABI static const char ID;

  /// Open \p Name and return both its contents and the CAS reference for those
  /// contents.
  ///
  /// This is the primitive CAS operation of this interface: the reference is
  /// produced by (or looked up alongside) the very read that produces the
  /// buffer, so a caller that needs both pays for a single open and never has
  /// to re-derive the reference afterwards.
  ///
  /// The IsText parameter is used to distinguish whether the file should be
  /// opened as a binary or text file.
  virtual llvm::Expected<
      std::pair<std::unique_ptr<llvm::MemoryBuffer>, cas::ObjectRef>>
  getBufferAndObjectRefForFile(const Twine &Name, int64_t FileSize = -1,
                               bool RequiresNullTerminator = true,
                               bool IsVolatile = false, bool IsText = true) = 0;

  /// Get ObjectRef of a file from its path, discarding the contents.
  ///
  /// Prefer \c getBufferAndObjectRefForFile when the contents are wanted too.
  /// Note that a path is only meaningful to the filesystem that is asked: do
  /// not take a path from the top of a VFS stack (e.g. a \c FileEntryRef name,
  /// which may be an \c -ivfsoverlay virtual path) and hand it to a filesystem
  /// further down, where it may not exist or may name different contents.
  LLVM_ABI llvm::Expected<cas::ObjectRef>
  getObjectRefForFileContent(const Twine &Name);

  /// Get a proxy FS that has an independent working directory.
  virtual IntrusiveRefCntPtr<CASBackedFileSystem>
  createThreadSafeProxyFS() = 0;
};

LLVM_ABI Expected<std::unique_ptr<vfs::FileSystem>>
createCASFileSystem(std::shared_ptr<ObjectStore> DB, const CASID &RootID,
                    sys::path::Style PathStyle = sys::path::Style::native);

LLVM_ABI Expected<std::unique_ptr<vfs::FileSystem>>
createCASFileSystem(ObjectStore &DB, const CASID &RootID,
                    sys::path::Style PathStyle = sys::path::Style::native);

} // namespace llvm::cas

#endif // LLVM_CAS_CASFILESYSTEM_H
