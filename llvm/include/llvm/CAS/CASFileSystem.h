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
class CASBackedFileSystem
    : public llvm::RTTIExtends<CASBackedFileSystem, vfs::FileSystem> {
public:
  LLVM_ABI static const char ID;

  /// This is a convenience method that opens a file, gets its content and then
  /// closes the file. It returns MemoryBuffer and ObjectRef in one call to avoid
  /// open the file twice.
  /// The IsText parameter is used to distinguish whether the file should be
  /// opened as a binary or text file.
  virtual llvm::Expected<
      std::pair<std::unique_ptr<llvm::MemoryBuffer>, cas::ObjectRef>>
  getBufferAndObjectRefForFile(const Twine &Name, int64_t FileSize = -1,
                               bool RequiresNullTerminator = true,
                               bool IsVolatile = false, bool IsText = true) = 0;

  /// The CAS that the references returned by this filesystem belong to.
  virtual ObjectStore &getCAS() const = 0;

  /// Get ObjectRef of a file from its path.
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
