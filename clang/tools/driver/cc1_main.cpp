//===-- cc1_main.cpp - Clang CC1 Compiler Frontend ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the entry point to the clang -cc1 functionality, which implements the
// core compiler functionality along with a number of additional tools for
// demonstration and testing purposes.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/DiagnosticCAS.h"
#include "clang/Basic/Stack.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/Version.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Config/config.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/FrontendTool/Utils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASFileSystem.h"
#include "llvm/CAS/CASOutputBackend.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/BuryPointer.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TimeProfiler.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <cstdio>

#ifdef CLANG_HAVE_RLIMITS
#include <sys/resource.h>
#endif

using namespace clang;
using namespace llvm::opt;

//===----------------------------------------------------------------------===//
// Main driver
//===----------------------------------------------------------------------===//

static void LLVMErrorHandler(void *UserData, const std::string &Message,
                             bool GenCrashDiag) {
  DiagnosticsEngine &Diags = *static_cast<DiagnosticsEngine*>(UserData);

  Diags.Report(diag::err_fe_error_backend) << Message;

  // Run the interrupt handlers to make sure any special cleanups get done, in
  // particular that we remove files registered with RemoveFileOnSignal.
  llvm::sys::RunInterruptHandlers();

  // We cannot recover from llvm errors.  When reporting a fatal error, exit
  // with status 70 to generate crash diagnostics.  For BSD systems this is
  // defined as an internal software error.  Otherwise, exit with status 1.
  llvm::sys::Process::Exit(GenCrashDiag ? 70 : 1);
}

#ifdef CLANG_HAVE_RLIMITS
#if defined(__linux__) && defined(__PIE__)
static size_t getCurrentStackAllocation() {
  // If we can't compute the current stack usage, allow for 512K of command
  // line arguments and environment.
  size_t Usage = 512 * 1024;
  if (FILE *StatFile = fopen("/proc/self/stat", "r")) {
    // We assume that the stack extends from its current address to the end of
    // the environment space. In reality, there is another string literal (the
    // program name) after the environment, but this is close enough (we only
    // need to be within 100K or so).
    unsigned long StackPtr, EnvEnd;
    // Disable silly GCC -Wformat warning that complains about length
    // modifiers on ignored format specifiers. We want to retain these
    // for documentation purposes even though they have no effect.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
    if (fscanf(StatFile,
               "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu "
               "%*lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu %*ld %*lu %*lu "
               "%*lu %*lu %lu %*lu %*lu %*lu %*lu %*lu %*llu %*lu %*lu %*d %*d "
               "%*u %*u %*llu %*lu %*ld %*lu %*lu %*lu %*lu %*lu %*lu %lu %*d",
               &StackPtr, &EnvEnd) == 2) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
      Usage = StackPtr < EnvEnd ? EnvEnd - StackPtr : StackPtr - EnvEnd;
    }
    fclose(StatFile);
  }
  return Usage;
}

#include <alloca.h>

LLVM_ATTRIBUTE_NOINLINE
static void ensureStackAddressSpace() {
  // Linux kernels prior to 4.1 will sometimes locate the heap of a PIE binary
  // relatively close to the stack (they are only guaranteed to be 128MiB
  // apart). This results in crashes if we happen to heap-allocate more than
  // 128MiB before we reach our stack high-water mark.
  //
  // To avoid these crashes, ensure that we have sufficient virtual memory
  // pages allocated before we start running.
  size_t Curr = getCurrentStackAllocation();
  const int kTargetStack = DesiredStackSize - 256 * 1024;
  if (Curr < kTargetStack) {
    volatile char *volatile Alloc =
        static_cast<volatile char *>(alloca(kTargetStack - Curr));
    Alloc[0] = 0;
    Alloc[kTargetStack - Curr - 1] = 0;
  }
}
#else
static void ensureStackAddressSpace() {}
#endif

/// Attempt to ensure that we have at least 8MiB of usable stack space.
static void ensureSufficientStack() {
  struct rlimit rlim;
  if (getrlimit(RLIMIT_STACK, &rlim) != 0)
    return;

  // Increase the soft stack limit to our desired level, if necessary and
  // possible.
  if (rlim.rlim_cur != RLIM_INFINITY &&
      rlim.rlim_cur < rlim_t(DesiredStackSize)) {
    // Try to allocate sufficient stack.
    if (rlim.rlim_max == RLIM_INFINITY ||
        rlim.rlim_max >= rlim_t(DesiredStackSize))
      rlim.rlim_cur = DesiredStackSize;
    else if (rlim.rlim_cur == rlim.rlim_max)
      return;
    else
      rlim.rlim_cur = rlim.rlim_max;

    if (setrlimit(RLIMIT_STACK, &rlim) != 0 ||
        rlim.rlim_cur != DesiredStackSize)
      return;
  }

  // We should now have a stack of size at least DesiredStackSize. Ensure
  // that we can actually use that much, if necessary.
  ensureStackAddressSpace();
}
#else
static void ensureSufficientStack() {}
#endif

/// Print supported cpus of the given target.
static int PrintSupportedCPUs(std::string TargetStr) {
  std::string Error;
  const llvm::Target *TheTarget =
      llvm::TargetRegistry::lookupTarget(TargetStr, Error);
  if (!TheTarget) {
    llvm::errs() << Error;
    return 1;
  }

  // the target machine will handle the mcpu printing
  llvm::TargetOptions Options;
  std::unique_ptr<llvm::TargetMachine> TheTargetMachine(
      TheTarget->createTargetMachine(TargetStr, "", "+cpuhelp", Options, None));
  return 0;
}

static Optional<llvm::cas::CASID>
createResultCacheKey(llvm::cas::CASDB &CAS, DiagnosticsEngine &Diags,
                     StringRef RootIDString, ArrayRef<const char *> Argv) {
  // FIXME: currently correct since the main executable is always in the root
  // from scanning, but we should probably make it explicit here...
  Expected<llvm::cas::CASID> RootID = CAS.parseCASID(RootIDString);
  if (!RootID) {
    llvm::consumeError(RootID.takeError());
    Diags.Report(diag::err_cas_cannot_parse_root_id) << RootIDString;
    return None;
  }

  SmallString<256> CommandLine;
  for (StringRef Arg : Argv) {
    CommandLine.append(Arg);
    CommandLine.push_back(0);
  }

  llvm::cas::HierarchicalTreeBuilder Builder;
  Builder.push(*RootID, llvm::cas::TreeEntry::Tree, "filesystem");
  Builder.push(llvm::cantFail(CAS.createBlob(CommandLine)),
               llvm::cas::TreeEntry::Regular, "command-line");
  Builder.push(llvm::cantFail(CAS.createBlob("-cc1")),
               llvm::cas::TreeEntry::Regular, "computation");

  // FIXME: The version is maybe insufficient...
  Builder.push(llvm::cantFail(CAS.createBlob(getClangFullVersion())),
               llvm::cas::TreeEntry::Regular, "version");

  return llvm::cantFail(Builder.create(CAS)).getID();
}

static int replayResult(llvm::cas::CASDB &CAS, llvm::cas::CASID ResultID) {
  std::unique_ptr<llvm::vfs::FileSystem> FS;
  if (Expected<std::unique_ptr<llvm::vfs::FileSystem>> ExpectedFS =
          llvm::cas::createCASFileSystem(CAS, ResultID))
    FS = std::move(*ExpectedFS);
  else
    llvm::report_fatal_error(ExpectedFS.takeError());

  // FIXME: portable? Maybe, since CASFileSystem maybe will always be posix?
  std::unique_ptr<llvm::MemoryBuffer> Errs;
  if (auto ContentOrErr = FS->getBufferForFile("/stderr"))
    Errs = std::move(*ContentOrErr);
  else
    llvm::report_fatal_error(
        llvm::createStringError(ContentOrErr.getError(), "CAS error accessing stderr"));
  llvm::errs() << Errs->getBuffer();

  // FIXME: portable? Maybe, since CASFileSystem maybe will always be posix?
  std::error_code EC;
  for (llvm::vfs::recursive_directory_iterator I(*FS, "/outputs", EC), E;
       I != E && !EC; I.increment(EC)) {
    std::unique_ptr<llvm::MemoryBuffer> Content;
    StringRef CASPath = I->path();
    auto ContentOrErr = FS->getBufferForFile(CASPath);
    if (!ContentOrErr)
      continue; // Skip over directories.
    Content = std::move(*ContentOrErr);

    // FIXME: This doesn't seem totally right, but it seems to "work" given
    // that the outputs are being stored at the root right now (not in a
    // working directory).
    StringRef OutputPath = CASPath.drop_front(StringRef("/outputs/").size());

    std::unique_ptr<llvm::FileOutputBuffer> Output;
    if (Expected<std::unique_ptr<llvm::FileOutputBuffer>> ExpectedOutput =
            llvm::FileOutputBuffer::create(OutputPath, Content->getBufferSize()))
      Output = std::move(*ExpectedOutput);
    else
      llvm::report_fatal_error(ExpectedOutput.takeError());
    llvm::copy(Content->getBuffer(), Output->getBufferStart());
    if (llvm::Error E = Output->commit())
      llvm::report_fatal_error(std::move(E));
  }
  return 0;
}

namespace {
class raw_mirroring_ostream : public llvm::raw_ostream {
  llvm::raw_ostream &Base;
  std::unique_ptr<llvm::raw_ostream> Reflection;

  void write_impl(const char *Ptr, size_t Size) override {
    Base.write(Ptr, Size);
    Reflection->write(Ptr, Size);
  }

  uint64_t current_pos() const override { return Base.tell(); }

public:
  raw_mirroring_ostream(llvm::raw_ostream &Base,
                        std::unique_ptr<llvm::raw_ostream> Reflection)
      : Base(Base), Reflection(std::move(Reflection)) {
    // FIXME: Is this right?
    enable_colors(true);
    SetUnbuffered();
  }

  bool is_displayed() const override { return Base.is_displayed(); }

  bool has_colors() const override { return Base.has_colors(); }
};
} // namespace

int cc1_main(ArrayRef<const char *> Argv, const char *Argv0, void *MainAddr) {
  ensureSufficientStack();

  std::unique_ptr<CompilerInstance> Clang(new CompilerInstance());
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  // Register the support for object-file-wrapped Clang modules.
  auto PCHOps = Clang->getPCHContainerOperations();
  PCHOps->registerWriter(std::make_unique<ObjectFilePCHContainerWriter>());
  PCHOps->registerReader(std::make_unique<ObjectFilePCHContainerReader>());

  // Initialize targets first, so that --version shows registered targets.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Buffer diagnostics from argument parsing so that we can output them using a
  // well formed diagnostic object.
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticBuffer *DiagsBuffer = new TextDiagnosticBuffer;
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);

  // Setup round-trip remarks for the DiagnosticsEngine used in CreateFromArgs.
  if (find(Argv, StringRef("-Rround-trip-cc1-args")) != Argv.end())
    Diags.setSeverity(diag::remark_cc1_round_trip_generated,
                      diag::Severity::Remark, {});

  bool Success = CompilerInvocation::CreateFromArgs(Clang->getInvocation(),
                                                    Argv, Diags, Argv0);

  if (Clang->getFrontendOpts().TimeTrace) {
    llvm::timeTraceProfilerInitialize(
        Clang->getFrontendOpts().TimeTraceGranularity, Argv0);
  }
  // --print-supported-cpus takes priority over the actual compilation.
  if (Clang->getFrontendOpts().PrintSupportedCPUs)
    return PrintSupportedCPUs(Clang->getTargetOpts().Triple);

  // Infer the builtin include path if unspecified.
  if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang->getHeaderSearchOpts().ResourceDir.empty())
    Clang->getHeaderSearchOpts().ResourceDir =
      CompilerInvocation::GetResourcesPath(Argv0, MainAddr);

  // Create the actual diagnostics engine.
  Clang->createDiagnostics();
  if (!Clang->hasDiagnostics())
    return 1;

  // Set an error handler, so that any LLVM backend diagnostics go through our
  // error handler.
  llvm::install_fatal_error_handler(LLVMErrorHandler,
                                  static_cast<void*>(&Clang->getDiagnostics()));

  DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());
  if (!Success)
    return 1;

  // Handle result caching in the CAS.
  Optional<llvm::cas::CASID> ResultCacheKey;
  std::shared_ptr<llvm::cas::CASDB> CAS;
  IntrusiveRefCntPtr<llvm::cas::CASOutputBackend> CASOutputs;
  SmallString<256> ResultDiags;
  std::unique_ptr<llvm::raw_ostream> ResultDiagsOS;
  if (Clang->getInvocation().getCASOpts().CASFileSystemResultCache) {
    CAS = createCASFromCompilerInvocation(Clang->getInvocation(), Clang->getDiagnostics());
    if (!CAS)
      return 1; // Error already emitted.

    // Check the result cache.
    ResultCacheKey = createResultCacheKey(
        *CAS, Clang->getDiagnostics(),
        Clang->getInvocation().getCASOpts().CASFileSystemRootID,
        Argv);
    if (!ResultCacheKey)
      return 1; // Error already emitted.
    Expected<llvm::cas::CASID> Result = CAS->getCachedResult(*ResultCacheKey);
    if (Result) {
      Clang->getDiagnostics().Report(diag::remark_cas_fs_result_cache_hit)
          << llvm::cantFail(CAS->convertCASIDToString(*ResultCacheKey))
          << llvm::cantFail(CAS->convertCASIDToString(*Result));
      int Failed = replayResult(*CAS, std::move(*Result));
      llvm::remove_fatal_error_handler();
      return Failed;
    }
    Clang->getDiagnostics().Report(diag::remark_cas_fs_result_cache_miss)
        << llvm::cantFail(CAS->convertCASIDToString(*ResultCacheKey));
    llvm::consumeError(Result.takeError());

    IntrusiveRefCntPtr<llvm::vfs::OutputBackend> OnDiskOutBackend =
        llvm::makeIntrusiveRefCnt<llvm::vfs::OnDiskOutputBackend>();
    IntrusiveRefCntPtr<llvm::vfs::OutputBackend> NormalFileOutBackend =
        OnDiskOutBackend;
    IntrusiveRefCntPtr<llvm::vfs::OutputBackend> CASIDFileOutBackend;
    if (Clang->getInvocation().getCASOpts().WriteOutputAsCASID) {
      std::string OutputFile =
          Clang->getInvocation().getFrontendOpts().OutputFile;
      // Create a backend that writes normal files contents but excludes the
      // output filename.
      NormalFileOutBackend = llvm::vfs::makeFilteringOutputBackend(
          OnDiskOutBackend,
          [OutputFile](StringRef ResolvedPath, llvm::vfs::OutputConfig) {
            return ResolvedPath != OutputFile;
          });
      // Create a backend that writes files as embedded CASIDs but only for the
      // output filename.
      CASIDFileOutBackend = llvm::vfs::makeFilteringOutputBackend(
          OnDiskOutBackend,
          [OutputFile](StringRef ResolvedPath, llvm::vfs::OutputConfig) {
            return ResolvedPath == OutputFile;
          });
    }

    // Set up the output backend so we can save / cache the result after.
    CASOutputs = llvm::makeIntrusiveRefCnt<llvm::cas::CASOutputBackend>(
        *CAS, std::move(CASIDFileOutBackend));
    Clang->setOutputBackend(llvm::vfs::makeMirroringOutputBackend(
        CASOutputs, std::move(NormalFileOutBackend)));
    ResultDiagsOS = std::make_unique<raw_mirroring_ostream>(
        llvm::errs(), std::make_unique<llvm::raw_svector_ostream>(ResultDiags));

    // FIXME: This should be saving/replaying serialized diagnostics, thus
    // using the current llvm::errs() colour capabilities. We still want to
    // print errors live during this compilation, just also serialize them.
    //
    // However, serialized diagnostics can only be written to a file, not to a
    // raw_ostream. Need to fix that first. Also, maybe the format doesn't need
    // to be bitcode... we just want to serialize them faithfully such that we
    // can decide at output time whether to make the colours pretty.
    Clang->getDiagnostics().setClient(
        new TextDiagnosticPrinter(
            *ResultDiagsOS, &Clang->getInvocation().getDiagnosticOpts()),
        /*ShouldOwnClient=*/true);
  }

  // Execute the frontend actions.
  {
    llvm::TimeTraceScope TimeScope("ExecuteCompiler");
    Success = ExecuteCompilerInvocation(Clang.get());
  }

  // If any timers were active but haven't been destroyed yet, print their
  // results now.  This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());
  llvm::TimerGroup::clearAll();

  if (llvm::timeTraceProfilerEnabled()) {
    SmallString<128> Path(Clang->getFrontendOpts().OutputFile);
    llvm::sys::path::replace_extension(Path, "json");
    if (auto profilerOutput = Clang->createOutputFile(
            Path.str(), /*Binary=*/false, /*RemoveFileOnSignal=*/false,
            /*useTemporary=*/false)) {
      llvm::timeTraceProfilerWrite(*profilerOutput);
      // FIXME(ibiryukov): make profilerOutput flush in destructor instead.
      profilerOutput->flush();
      llvm::timeTraceProfilerCleanup();
      Clang->clearOutputFiles(false);
    }
  }

  // Cache the result.
  //
  // FIXME: Also cache failed builds?
  if (Success && ResultCacheKey) {
    Expected<llvm::cas::CASID> Outputs = CASOutputs->createTree();
    if (!Outputs)
      llvm::report_fatal_error(Outputs.takeError());

    // Hack around llvm::errs() not being captured by the output backend yet.
    Expected<llvm::cas::BlobRef> Errs = CAS->createBlob(ResultDiags);
    if (!Errs)
      llvm::report_fatal_error(Errs.takeError());

    llvm::cas::HierarchicalTreeBuilder Builder;
    Builder.push(*Outputs, llvm::cas::TreeEntry::Tree, "outputs");
    Builder.push(*Errs, llvm::cas::TreeEntry::Regular, "stderr");
    Expected<llvm::cas::CASID> Result = Builder.create(*CAS);
    if (!Result)
      llvm::report_fatal_error(Result.takeError());
    if (llvm::Error E = CAS->putCachedResult(*ResultCacheKey, *Result))
      llvm::report_fatal_error(std::move(E));
  }

  // Our error handler depends on the Diagnostics object, which we're
  // potentially about to delete. Uninstall the handler now so that any
  // later errors use the default handling behavior instead.
  llvm::remove_fatal_error_handler();

  // When running with -disable-free, don't do any destruction or shutdown.
  if (Clang->getFrontendOpts().DisableFree) {
    llvm::BuryPointer(std::move(Clang));
    return !Success;
  }

  return !Success;
}
