//===- DependencyScanningTool.h - clang-scan-deps service ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLING_DEPENDENCY_SCANNING_TOOL_H
#define LLVM_CLANG_TOOLING_DEPENDENCY_SCANNING_TOOL_H

#include "clang/Tooling/DependencyScanning/DependencyScanningService.h"
#include "clang/Tooling/DependencyScanning/DependencyScanningWorker.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "llvm/ADT/StringSet.h"
#include <string>

namespace clang{
namespace tooling{
namespace dependencies{

/// The full dependencies and module graph for a specific input.
struct FullDependencies {
  /// The context in which these modules are valid.
  std::string ContextHash;
  /// The output paths the compiler would write to if this was a -c invocation.
  std::vector<std::string> OutputPaths;
  /// Files that the input directly depends on.
  std::vector<std::string> DirectFileDependencies;
  /// Modules that the input directly imports.
  std::vector<std::string> DirectModuleDependencies;
  /// The Clang modules this input transitively depends on that have not already
  /// been reported.
  std::vector<ModuleDeps> ClangModuleDeps;
};

/// The high-level implementation of the dependency discovery tool that runs on
/// an individual worker thread.
class DependencyScanningTool {
public:
  /// Construct a dependency scanning tool.
  ///
  /// \param Compilations     The reference to the compilation database that's
  /// used by the clang tool.
  DependencyScanningTool(DependencyScanningService &Service);

  /// Print out the dependency information into a string using the dependency
  /// file format that is specified in the options (-MD is the default) and
  /// return it.
  ///
  /// \returns A \c StringError with the diagnostic output if clang errors
  /// occurred, dependency file contents otherwise.
  llvm::Expected<std::string>
  getDependencyFile(const tooling::CompilationDatabase &Compilations,
                    StringRef CWD);

  /// Collect the full module depenedency graph for the input, ignoring any
  /// modules which have already been seen.
  ///
  /// \returns a \c StringError with the diagnostic output if clang errors
  /// occurred, \c FullDependencies otherwise.
  llvm::Expected<FullDependencies>
  getFullDependencies(const tooling::CompilationDatabase &Compilations,
                      StringRef CWD, const llvm::StringSet<> &AlreadySeen);

private:
  DependencyScanningWorker Worker;
};

} // end namespace dependencies
} // end namespace tooling
} // end namespace clang

#endif // LLVM_CLANG_TOOLING_DEPENDENCY_SCANNING_TOOL_H
