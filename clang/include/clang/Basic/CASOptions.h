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

#include "llvm/ADT/AtomicSharedPtr.h"
#include <string>
#include <vector>

namespace llvm {
namespace cas {

class ActionCache;
class CASDB;
class Namespace;

} // end namespace cas
} // end namespace llvm

namespace clang {

class DiagnosticsEngine;

/// Configures the CAS.
///
/// FIXME: Currently includes any option slightly related to the CAS, but
/// options that affect compilation should be moved out. This should only
/// be configuring:
///
/// - What CAS is being used?
/// - What ActionCache is being used?
///
/// It should not affect *how* the CAS or action cache are being used, just
/// where to find them / how to connect to them / etc..
class CASOptions {
public:
  enum CASKind {
    BuiltinCAS,
    PluginCAS,
  };

  CASKind Kind = BuiltinCAS;

  /// If set, path to an on-disk action cache.
  std::string ActionCachePath;

  /// If set, uses this root ID with \a CASFileSystem.
  ///
  /// FIXME: Move to \a FileSystemOptions.
  std::string CASFileSystemRootID;

  /// If set, used as the working directory for -fcas-fs.
  ///
  /// FIXME: Move to \a FileSystemOptions.
  std::string CASFileSystemWorkingDirectory;

  /// Use a result cache when possible, using CASFileSystemRootID.
  ///
  /// FIXME: Move to \a FrontendOptions.
  /// FIXME: Rename to "-fcached-cc1" or something. While it depends on
  /// "-fcas-fs" it's not strongly related.
  bool CASFileSystemResultCache = false;

  /// Transparently enable pre-tokenized files with -fcas.
  ///
  /// FIXME: Move to \a PreprocessorOptions.
  /// FIXME: Rename to "-fpretokenize" (or something), reflecting the semantic
  /// change rather than whether things are being cached.
  bool CASTokenCache = false;

  /// When using -fcas-fs-result-cache, write a CASID for the output file.
  ///
  /// FIXME: Move to \a CodeGenOptions.
  /// FIXME: Add clang tests for this functionality.
  bool WriteOutputAsCASID = false;

  /// For \a BuiltinCAS, a path on-disk for a persistent backing store. This is
  /// optional, although \a CASFileSystemRootID is unlikely to work.
  std::string BuiltinPath;

  /// For \a PluginCAS, the path to the plugin.
  std::string PluginPath;

  /// For \a PluginCAS, the arguments to initialize the plugin.
  std::vector<std::string> PluginArgs;

  std::shared_ptr<llvm::cas::CASDB>
  getOrCreateCAS(DiagnosticsEngine &Diags) const;
  std::shared_ptr<llvm::cas::ActionCache>
  getOrCreateActionCache(const llvm::cas::Namespace &NS,
                         DiagnosticsEngine &Diags) const;

  mutable llvm::AtomicSharedPtr<llvm::cas::CASDB> CachedCAS;
  mutable llvm::AtomicSharedPtr<llvm::cas::ActionCache> CachedActionCache;
};

} // end namespace clang

#endif // LLVM_CLANG_BASIC_CASOPTIONS_H
