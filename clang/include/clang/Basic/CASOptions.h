//===- CASOptions.h - Options for configuring the CAS -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the clang::CASOptions interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_CASOPTIONS_H
#define LLVM_CLANG_BASIC_CASOPTIONS_H

#include <string>
#include <vector>

namespace clang {

/// Keeps track of options that affect how file operations are performed.
class CASOptions {
public:
  enum CASKind {
    NoCAS, // FIXME: Should we delete this, and always use a CAS?
    InMemoryCAS,
    BuiltinCAS,
    PluginCAS,
  };

  CASKind Kind = NoCAS; // FIXME: Should this be \a InMemoryCAS?

  /// If set, uses this root ID with \a CASFileSystem.
  std::string CASFileSystemRootID;

  /// If set, used as the working directory for -fcas-fs.
  std::string CASFileSystemWorkingDirectory;

  /// Use a result cache when possible, using CASFileSystemRootID.
  bool CASFileSystemResultCache = false;

  /// For \a BuiltinCAS, a path on-disk for a persistent backing store. This is
  /// optional, although \a CASFileSystemRootID is unlikely to work.
  std::string BuiltinPath;

  /// For \a PluginCAS, the path to the plugin.
  std::string PluginPath;

  /// For \a PluginCAS, the arguments to initialize the plugin.
  std::vector<std::string> PluginArgs;
};

} // end namespace clang

#endif // LLVM_CLANG_BASIC_CASOPTIONS_H
