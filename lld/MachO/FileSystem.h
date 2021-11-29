//===- FileSystem.h ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines a number of functions as they exist in 'llvm/Support/FileSystem.h'
// but with implementations based on using a virtual filesystem that is setup
// during configuration.
//
// FIXME: Generalize and use for the other linker flavors as well.
//===----------------------------------------------------------------------===//

#ifndef LLD_MACHO_FILESYSTEM_H
#define LLD_MACHO_FILESYSTEM_H

#include "lld/Common/LLVM.h"
#include <system_error>

namespace llvm {
namespace vfs {
class Status;
}
} // namespace llvm

namespace lld {
namespace macho {
namespace fs {

bool exists(const Twine &path);
bool is_directory(const Twine &path);
std::error_code status(const Twine &path, llvm::vfs::Status &result);
std::error_code real_path(const Twine &path,
                          llvm::SmallVectorImpl<char> &output);

} // namespace fs
} // namespace macho
} // namespace lld

#endif
