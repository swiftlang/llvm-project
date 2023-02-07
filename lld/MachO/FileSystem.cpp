//===- FileSystem.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "FileSystem.h"
#include "Config.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace llvm;
using namespace lld;
using namespace lld::macho;

bool fs::exists(const Twine &path) { return config->fs->exists(path); }

bool fs::is_directory(const Twine &path) {
  auto status = config->fs->status(path);
  return status && status->isDirectory();
}

std::error_code fs::status(const Twine &path, vfs::Status &result) {
  auto status = config->fs->status(path);
  if (!status)
    return status.getError();
  result = *status;
  return std::error_code();
}

std::error_code fs::real_path(const Twine &path,
                              llvm::SmallVectorImpl<char> &output) {
  return config->fs->getRealPath(path, output);
}
