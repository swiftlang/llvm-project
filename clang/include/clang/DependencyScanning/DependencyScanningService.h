//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_DEPENDENCYSCANNING_DEPENDENCYSCANNINGSERVICE_H
#define LLVM_CLANG_DEPENDENCYSCANNING_DEPENDENCYSCANNINGSERVICE_H

#include "clang/Basic/AtomicLineLogger.h"
#include "clang/CAS/CASOptions.h"
#include "clang/DependencyScanning/DependencyScanningFilesystem.h"
#include "clang/DependencyScanning/InProcessModuleCache.h"
#include "llvm/ADT/BitmaskEnum.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/CASFileSystem.h"

#include <variant>

namespace clang {
namespace dependencies {

/// The mode in which the dependency scanner will operate to find the
/// dependencies.
enum class ScanningMode {
  /// This mode is used to compute the dependencies by running the preprocessor
  /// over the source files.
  CanonicalPreprocessing,

  /// This mode is used to compute the dependencies by running the preprocessor
  /// with special kind of lexing after scanning header and source files to get
  /// the minimum necessary preprocessor directives for evaluating includes.
  DependencyDirectivesScan,
};

#define DSS_LAST_BITMASK_ENUM(Id)                                              \
  LLVM_MARK_AS_BITMASK_ENUM(Id), All = llvm::NextPowerOf2(Id) - 1

enum class ScanningOptimizations {
  None = 0,

  /// Remove unused header search paths including header maps.
  HeaderSearch = 1,

  /// Remove warnings from system modules.
  SystemWarnings = (1 << 1),

  /// Remove unused -ivfsoverlay arguments.
  VFS = (1 << 2),

  /// Canonicalize -D and -U options.
  Macros = (1 << 3),

  /// Ignore the compiler's working directory if it is safe.
  IgnoreCWD = (1 << 4),

  DSS_LAST_BITMASK_ENUM(IgnoreCWD),

  // The build system needs to be aware that the current working
  // directory is ignored. Without a good way of notifying the build
  // system, it is less risky to default to off.
  Default = All & (~IgnoreCWD),

  FullIncludeTreeIrrelevant = HeaderSearch | VFS,
};

#undef DSS_LAST_BITMASK_ENUM

/// The function invoked to create each worker's VFS. This function and the
/// VFS itself must be thread-safe whenever using multiple workers concurrently
/// or whenever \c DependencyScanningServiceOptions::AsyncScanModules is true.
using MakeVFSFn = std::function<IntrusiveRefCntPtr<llvm::vfs::FileSystem>()>;

/// Scan for a regular compilation that reads its inputs from the file system.
struct RegularCompilation {
  /// Creates the VFS the scanner reads inputs from. The resulting compilation
  /// reads the same paths, so they must be accessible to it too.
  MakeVFSFn MakeVFS = [] { return llvm::vfs::createPhysicalFileSystem(); };
};
/// Scan for a compilation that reads its inputs from a CAS-based include tree.
struct IncludeTreeCompilation {
  /// The CAS configuration.
  CASOptions CASOpts;
  /// The CAS database to use.
  std::shared_ptr<cas::ObjectStore> CAS;
  /// The CAS cache to use.
  std::shared_ptr<cas::ActionCache> Cache;
  /// Creates the VFS the scanner reads inputs from. Their contents are stored
  /// in \c CAS as they are read.
  MakeVFSFn MakeVFS = [] { return llvm::vfs::createPhysicalFileSystem(); };
};

/// The function invoked to create each worker's CAS-backed VFS, with the same
/// thread-safety requirements as \c MakeVFSFn.
using MakeCASFSFn =
    std::function<IntrusiveRefCntPtr<llvm::cas::CASBackedFileSystem>()>;

/// Scan for a compilation that reads its inputs from a CAS-based include tree,
/// reading them from a filesystem that already knows their CAS references.
///
/// The scanner uses those references instead of storing the contents in the
/// CAS itself. Since the files may exist nowhere but in the CAS, there is no
/// equivalent for a regular compilation.
struct CASFSIncludeTreeCompilation {
  /// The CAS configuration.
  CASOptions CASOpts;
  /// The CAS database to use. Must be the CAS the filesystems created by
  /// \c MakeCASFS are backed by.
  std::shared_ptr<cas::ObjectStore> CAS;
  /// The CAS cache to use.
  std::shared_ptr<cas::ActionCache> Cache;
  /// Creates the VFS the scanner reads inputs from.
  MakeCASFSFn MakeCASFS;
};

/// What compilation the scan is for, and where it reads its inputs from.
using CompilationMode = std::variant<RegularCompilation, IncludeTreeCompilation,
                                     CASFSIncludeTreeCompilation>;

bool shouldCacheNegativeStatsDefault();

/// \return true if failed stats for files with this name should be cached,
///         false otherwise.
bool shouldCacheNegativeStatsForPath(StringRef Path);

/// The configuration knobs for the dependency scanning service.
struct DependencyScanningServiceOptions {
  DependencyScanningServiceOptions();

  /// Whether to use optimized dependency directive scan or full preprocessing.
  ScanningMode Mode = ScanningMode::DependencyDirectivesScan;
  /// What compilation to scan for, and where to read its inputs from.
  CompilationMode Compilation = RegularCompilation{};
  /// How to optimize resulting explicit module command lines.
  ScanningOptimizations OptimizeArgs = ScanningOptimizations::Default;
  /// Whether the scanner should emit warnings.
  bool EmitWarnings = true;
  /// Whether to make reported file paths absolute.
  bool ReportAbsolutePaths = true;
  /// Whether to report modules visible from modules that are imported directly.
  bool ReportVisibleModules = false;
  /// Whether the resulting command lines should load explicit PCMs eagerly.
  bool EagerLoadModules = false;
  /// Whether to trace VFS accesses during the scan.
  bool TraceVFS = false;
  /// Whether to scan modules asynchronously.
  bool AsyncScanModules = false;
  /// The build session timestamp for validate-once-per-build-session logic.
  std::time_t BuildSessionTimestamp; // = std::chrono::system_clock::now();
  /// Whether to automatically flush the module cache from memory to disk at the
  /// end of the service lifetime.
  bool FlushModuleCache = true;
  /// Whether the caching VFS should cache missing filesystem entries.
  bool CacheNegativeStats = shouldCacheNegativeStatsDefault();
  /// Whether scanning modules validate their directory deps against paths
  /// reported via \c DependencyScanningService::addInvalidatedPath instead of
  /// the filesystem.
  bool ValidateAgainstInvalidatedPaths = false;
  /// Whether the scanner should mimic the behavior of the compilation itself.
  /// This enables generation of dependency files, and emission of full
  /// diagnostics.
  bool AsCompilation = false;
  /// The path to a log file, which logs timing of actions performed by
  /// the dependency scanner.
  std::string LogPath;
};

/// The dependency scanning service contains shared configuration and state that
/// is used by the individual dependency scanning workers.
class DependencyScanningService {
public:
  explicit DependencyScanningService(DependencyScanningServiceOptions Opts);

  ~DependencyScanningService() {
    if (Opts.FlushModuleCache)
      ModCacheEntries.flush();
  }

  const DependencyScanningServiceOptions &getOpts() const { return Opts; }

  DependencyScanningFilesystemSharedCache &getSharedCache() {
    return SharedCache;
  }

  ModuleCacheEntries &getModuleCacheEntries() { return ModCacheEntries; }

  /// Add a path that changed since the previous scan, so that cached scanning
  /// modules depending on it are rebuilt. Requires
  /// \c ValidateAgainstInvalidatedPaths.
  ///
  /// Only directories are currently checked. \p Path is compared textually
  /// against \c ModuleDeps::DirectoryDeps, so it must be spelled the same way:
  /// absolute, with dots removed, and without resolving symlinks. It is never
  /// accessed on disk. Modules built since \c BuildSessionTimestamp are
  /// considered up to date with it.
  void addInvalidatedPath(StringRef Path) {
    assert(Opts.ValidateAgainstInvalidatedPaths &&
           "invalidated paths are only checked when validating against them");
    ModCacheEntries.addInvalidatedPath(Path);
  }

  CASOptions getCASOpts() const;
  std::shared_ptr<cas::ObjectStore> getCAS() const;
  std::shared_ptr<cas::ActionCache> getActionCache() const;
  AtomicLineLogger &getLogger() { return Logger; }

private:
  /// The options customizing dependency scanning behavior.
  DependencyScanningServiceOptions Opts;
  /// The global file system cache.
  DependencyScanningFilesystemSharedCache SharedCache;
  /// The global module cache entries.
  ModuleCacheEntries ModCacheEntries;
  /// The logger of dependency scanning actions.
  AtomicLineLogger Logger;
};

} // end namespace dependencies
} // end namespace clang

#endif // LLVM_CLANG_DEPENDENCYSCANNING_DEPENDENCYSCANNINGSERVICE_H
