//===- Filesystem.h ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLD_FILESYSTEM_H
#define LLD_FILESYSTEM_H

#include "lld/Common/LLVM.h"
#include <system_error>

namespace llvm {
template <typename T> class IntrusiveRefCntPtr;

namespace cas {
class CASDB;
}

namespace vfs {
class FileSystem;
}

} // namespace llvm

namespace lld {
void unlinkAsync(StringRef path);
std::error_code tryCreateFile(StringRef path);

Expected<llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>>
createFileSystem(llvm::cas::CASDB *CAS,
                 llvm::Optional<StringRef> CASFileSystemRootID,
                 llvm::Optional<StringRef> CASFileSystemWorkingDirectory);

} // namespace lld

#endif
