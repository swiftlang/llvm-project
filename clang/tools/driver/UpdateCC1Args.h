//===- UpdateCC1Args.h - Helper for updating -cc1 -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// FIXME: Having this file here is a hack; this stuff probably needs to move to
// the Frontend library, likely in CompilerInvocation.

#include "clang/Basic/Stack.h"
#include "clang/Driver/CC1DepScanDClient.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace opt {
class Arg;
class ArgList;
} // namespace opt
} // namespace llvm

namespace clang {

class DiagnosticsEngine;

namespace tooling {
namespace dependencies {
class DependencyScanningTool;
} // end namespace dependencies
} // end namespace tooling

llvm::Expected<llvm::cas::CASID>
updateCC1Args(tooling::dependencies::DependencyScanningTool &Tool,
              DiagnosticConsumer &DiagsConsumer, const char *Exec,
              ArrayRef<const char *> InputArgs, StringRef WorkingDirectory,
              SmallVectorImpl<const char *> &OutputArgs,
              const cc1depscand::DepscanPrefixMapping &PrefixMapping,
              llvm::function_ref<const char *(const Twine &)> SaveArg);

llvm::Expected<llvm::cas::CASID>
updateCC1Args(const char *Exec, ArrayRef<const char *> InputArgs,
              SmallVectorImpl<const char *> &OutputArgs,
              const cc1depscand::DepscanPrefixMapping &PrefixMapping,
              llvm::function_ref<const char *(const Twine &)> SaveArg);

void CC1ScanDeps(const llvm::opt::Arg &A, const char *Exec,
                 SmallVectorImpl<const char *> &CC1Args,
                 DiagnosticsEngine &Diag, const llvm::opt::ArgList &Args);

} // end namespace clang
