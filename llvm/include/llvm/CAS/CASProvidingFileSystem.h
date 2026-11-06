//===- llvm/CAS/CASProvidingFileSystem.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASPROVIDINGFILESYSTEM_H
#define LLVM_CAS_CASPROVIDINGFILESYSTEM_H

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/CAS/CASFileSystem.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Error.h"

namespace llvm {
namespace vfs {
class FileSystem;
}

namespace cas {
class ObjectStore;

/// Implements \c CASBackedFileSystem::getBufferAndObjectRefForFile() by
/// ingesting the file buffer into \p DB. Files opened with \c openFileForRead
/// are ingested too.
LLVM_ABI std::unique_ptr<CASBackedFileSystem> createCASProvidingFileSystem(
    std::shared_ptr<ObjectStore> DB,
    IntrusiveRefCntPtr<llvm::vfs::FileSystem> UnderlyingFS);

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_CASPROVIDINGFILESYSTEM_H
