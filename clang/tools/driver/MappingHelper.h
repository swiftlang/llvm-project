//===- MappingHelper.h - Helper for updating mappings ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// FIXME: Having this file here is a total hack; this stuff probably needs to
// move to the Frontend library, likely in CompilerInvocation.

#include "clang/Basic/Stack.h"
#include "clang/Driver/CC1DepScanDClient.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {

namespace tooling {
namespace dependencies {
class DependencyScanningTool;
} // end namespace dependencies
} // end namespace tooling

llvm::Expected<llvm::cas::CASID>
updateCC1Args(llvm::cas::CachingOnDiskFileSystem &FS,
              tooling::dependencies::DependencyScanningTool &Tool,
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

static llvm::Error computeSingleMapping(
    llvm::StringSaver &Saver, llvm::cas::CachingOnDiskFileSystem &FS,
    StringRef Old, StringRef New,
    SmallVectorImpl<std::pair<StringRef, StringRef>> &ComputedMapping) {
  SmallString<256> RealPath = Old;
  if (std::error_code EC = FS.getRealPath(Old, RealPath))
    return createFileError(Old, EC);
  if (RealPath != Old)
    Old = Saver.save(StringRef(RealPath));
  ComputedMapping.push_back({Old, New});
  return llvm::Error::success();
}

static llvm::Error computeSDKMapping(
    llvm::StringSaver &Saver, llvm::cas::CachingOnDiskFileSystem &FS,
    const CompilerInvocation &Invocation, StringRef New,
    SmallVectorImpl<std::pair<StringRef, StringRef>> &ComputedMapping) {
  StringRef SDK = Invocation.getHeaderSearchOpts().Sysroot;
  if (SDK.empty())
    return llvm::Error::success();

  // Need a new copy of the string since the invocation will be modified.
  SDK = Saver.save(SDK);
  return computeSingleMapping(Saver, FS, SDK, New, ComputedMapping);
}

static llvm::Error computeToolchainMapping(
    llvm::StringSaver &Saver, llvm::cas::CachingOnDiskFileSystem &FS,
    StringRef ClangPath, StringRef New,
    SmallVectorImpl<std::pair<StringRef, StringRef>> &ComputedMapping) {
  // Look up from clang for the toolchain, assuming clang is at
  // <toolchain>/usr/bin/clang. Return a shallower guess if the directories
  // don't match.
  //
  // FIXME: Should this append ".." instead of calling parent_path?
  StringRef Guess = llvm::sys::path::parent_path(ClangPath);
  for (StringRef Dir : {"bin", "usr"}) {
    if (llvm::sys::path::filename(Guess) != Dir)
      break;
    Guess = llvm::sys::path::parent_path(Guess);
  }
  return computeSingleMapping(Saver, FS, Guess, New, ComputedMapping);
}

static llvm::Error computeFullMapping(
    llvm::StringSaver &Saver, llvm::cas::CachingOnDiskFileSystem &FS,
    StringRef ClangPath, const CompilerInvocation &Invocation,
    const cc1depscand::DepscanPrefixMapping &DepscanMapping,
    SmallVectorImpl<std::pair<StringRef, StringRef>> &ComputedMapping) {
  if (DepscanMapping.NewSDKPath)
    if (llvm::Error E = computeSDKMapping(
            Saver, FS, Invocation, *DepscanMapping.NewSDKPath, ComputedMapping))
      return E;

  if (DepscanMapping.NewToolchainPath)
    if (llvm::Error E = computeToolchainMapping(
            Saver, FS, ClangPath, *DepscanMapping.NewToolchainPath,
            ComputedMapping))
      return E;

  for (StringRef Map : DepscanMapping.PrefixMap) {
    size_t Equals = Map.find('=');
    if (Equals == StringRef::npos)
      continue; // FIXME: Should have been checked already, but we should error?
    StringRef Old = Map.take_front(Equals);
    StringRef New = Map.drop_front(Equals + 1);
    if (llvm::Error E =
            computeSingleMapping(Saver, FS, Old, New, ComputedMapping))
      return E;
  }

  // Sort in reverse lexicographic order, so that deeper paths are prioritized
  // over their parent paths. For example, if both the source and build
  // directories are remapped and one is nested inside the other, the nested
  // one should be checked first.
  std::stable_sort(ComputedMapping.begin(), ComputedMapping.end(),
                   [](const std::pair<StringRef, StringRef> &LHS,
                      const std::pair<StringRef, StringRef> &RHS) {
    return LHS.first > RHS.first;
  });

  return llvm::Error::success();
}

static StringRef remapPath(StringRef Path, llvm::StringSaver &Saver,
                           ArrayRef<std::pair<StringRef, StringRef>> Mapping) {
  // Replace with the first match.
  for (auto Map : Mapping) {
    StringRef Old = Map.first;
    StringRef New = Map.second;
    StringRef Suffix = Path;
    if (!Suffix.consume_front(Old))
      continue;

    // Matches exactly.
    if (Suffix.empty())
      return New;

    if (!llvm::sys::path::is_separator(Suffix.front()))
      continue;

    // Drop the separator, append, and return.
    SmallString<256> Absolute = New;
    llvm::sys::path::append(Absolute, Suffix.drop_front());
    return Saver.save(StringRef(Absolute));
  }

  // Didn't find a match.
  return Path;
}

static void updateCompilerInvocation(
    CompilerInvocation &Invocation, llvm::StringSaver &Saver,
    llvm::cas::CachingOnDiskFileSystem &FS, std::string RootID,
    StringRef CASWorkingDirectory,
    ArrayRef<std::pair<StringRef, StringRef>> ComputedMapping) {
  // Fix the CAS options.
  auto &CASOpts = Invocation.getCASOpts();
  CASOpts.Kind = CASOptions::BuiltinCAS; // FIXME: Don't override.
  CASOpts.CASFileSystemRootID = RootID;
  CASOpts.CASFileSystemWorkingDirectory = CASWorkingDirectory.str();
  CASOpts.CASFileSystemResultCache = true; // FIXME: Don't always set.
  CASOpts.BuiltinPath =
      llvm::cas::getDefaultOnDiskCASStableID(); // FIXME: Don't override.

  // If there are no mappings, we're done. Otherwise, continue and remap
  // everything.
  if (ComputedMapping.empty())
    return;

  // Turn off dependency outputs. Should have already been emitted.
  Invocation.getDependencyOutputOpts().OutputFile.clear();

  auto remap = [&](StringRef In) -> Optional<StringRef> {
    if (In.empty())
      return In;

    // FIXME: This converts relative paths to absolute paths. That's not ideal.
    // For relative paths, it'd be better to recreate the relative path in the
    // remapped world.
    SmallString<256> RealPath;
    if (std::error_code EC = FS.getRealPath(In, RealPath))
      return None;

    // Don't modify paths that don't get remapped at all.
    StringRef Out = remapPath(RealPath, Saver, ComputedMapping);
    return RealPath == Out ? In : Out;
  };

  // Returns "false" on success, "true" if the path doesn't exist.
  auto remapInPlace = [&](std::string &S) -> bool {
    auto NewS = remap(S);
    if (!NewS)
      return true;
    S = NewS->str();
    return false;
  };

  auto remapInPlaceOrClear = [&](std::string &S) {
    if (remapInPlace(S))
      S.clear();
  };

  auto remapInPlaceOrFilterOutWith = [&](auto &Vector, auto Remapper) {
    Vector.erase(llvm::remove_if(Vector, Remapper), Vector.end());
  };

  auto remapInPlaceOrFilterOut = [&](std::vector<std::string> &Vector) {
    remapInPlaceOrFilterOutWith(Vector, remapInPlace);
  };

  // Fix the CAS options first.
  if (remapInPlace(CASOpts.CASFileSystemWorkingDirectory))
    return; // If we can't remap the working directory, skip everything else.

  // Remap header search.
  auto &HeaderSearchOpts = Invocation.getHeaderSearchOpts();
  remapInPlaceOrClear(HeaderSearchOpts.Sysroot);
  remapInPlaceOrFilterOutWith(HeaderSearchOpts.UserEntries,
                              [&](HeaderSearchOptions::Entry &Entry) {
                                if (Entry.IgnoreSysRoot)
                                  return remapInPlace(Entry.Path);
                                return false;
                              });
  remapInPlaceOrFilterOutWith(
      HeaderSearchOpts.SystemHeaderPrefixes,
      [&](HeaderSearchOptions::SystemHeaderPrefix &Prefix) {
        return remapInPlace(Prefix.Prefix);
      });
  remapInPlaceOrClear(HeaderSearchOpts.ResourceDir);
  remapInPlaceOrClear(HeaderSearchOpts.ModuleCachePath);
  remapInPlaceOrClear(HeaderSearchOpts.ModuleUserBuildPath);
  for (auto I = HeaderSearchOpts.PrebuiltModuleFiles.begin(),
            E = HeaderSearchOpts.PrebuiltModuleFiles.end();
       I != E;) {
    auto Current = I++;
    if (remapInPlace(Current->second))
      HeaderSearchOpts.PrebuiltModuleFiles.erase(Current);
  }
  remapInPlaceOrFilterOut(HeaderSearchOpts.PrebuiltModulePaths);
  remapInPlaceOrFilterOut(HeaderSearchOpts.VFSOverlayFiles);

  // Frontend options.
  auto &FrontendOpts = Invocation.getFrontendOpts();
  remapInPlaceOrFilterOutWith(
      FrontendOpts.Inputs, [&](FrontendInputFile &Input) {
        if (Input.isBuffer())
          return false; // FIXME: Can this happen when parsing command-line?

        Optional<StringRef> RemappedFile = remap(Input.getFile());
        if (!RemappedFile)
          return true;
        if (RemappedFile != Input.getFile())
          Input = FrontendInputFile(*RemappedFile, Input.getKind(),
                                    Input.isSystem());
        return false;
      });

  // Skip the output file. That's not the input CAS filesystem.
  //   remapInPlaceOrClear(OutputFile); <-- this doesn't make sense.

  remapInPlaceOrClear(FrontendOpts.CodeCompletionAt.FileName);

  // Don't remap plugins (for now), since we don't know how to remap their
  // arguments. Maybe they should be loaded outside of the CAS filesystem?
  // Maybe we should error?
  //
  //  remapInPlaceOrFilterOut(FrontendOpts.Plugins);

  remapInPlaceOrFilterOut(FrontendOpts.ModuleMapFiles);
  remapInPlaceOrFilterOut(FrontendOpts.ModuleFiles);
  remapInPlaceOrFilterOut(FrontendOpts.ModulesEmbedFiles);
  remapInPlaceOrFilterOut(FrontendOpts.ASTMergeFiles);
  remapInPlaceOrClear(FrontendOpts.OverrideRecordLayoutsFile);
  remapInPlaceOrClear(FrontendOpts.StatsFile);

  // Filesystem options.
  auto &FileSystemOpts = Invocation.getFileSystemOpts();
  remapInPlaceOrClear(FileSystemOpts.WorkingDir);

  // Code generation options.
  auto &CodeGenOpts = Invocation.getCodeGenOpts();
  remapInPlaceOrClear(CodeGenOpts.DebugCompilationDir);
  remapInPlaceOrClear(CodeGenOpts.CoverageCompilationDir);
}
}
