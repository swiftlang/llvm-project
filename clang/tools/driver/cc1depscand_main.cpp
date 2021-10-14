//===- cc1depscand_main.cpp - Clang CC1 Dependency Scanning Daemon --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MappingHelper.h"
#include "clang/Basic/DiagnosticDriver.h"
#include "clang/Basic/DiagnosticFrontend.h"
#include "clang/Basic/Stack.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Config/config.h"
#include "clang/Driver/CC1DepScanDProtocol.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/FrontendTool/Utils.h"
#include "clang/Tooling/DependencyScanning/DependencyScanningService.h"
#include "clang/Tooling/DependencyScanning/DependencyScanningTool.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/BuryPointer.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <sys/file.h> // FIXME: Unix-only. Not portable.

#ifdef CLANG_HAVE_RLIMITS
#include <sys/resource.h>
#endif

using namespace clang;
using namespace llvm::opt;

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

static void reportAsFatalIfError(llvm::Error E) {
  if (E)
    llvm::report_fatal_error(std::move(E));
}

template <typename T> static T reportAsFatalIfError(Expected<T> ValOrErr) {
  if (!ValOrErr)
    reportAsFatalIfError(ValOrErr.takeError());
  return std::move(*ValOrErr);
}

namespace {
class OneOffCompilationDatabase : public tooling::CompilationDatabase {
public:
  OneOffCompilationDatabase() = delete;
  template <class... ArgsT>
  OneOffCompilationDatabase(ArgsT &&... Args)
      : Command(std::forward<ArgsT>(Args)...) {}

  std::vector<tooling::CompileCommand>
  getCompileCommands(StringRef FilePath) const override {
    return {Command};
  }

  std::vector<tooling::CompileCommand> getAllCompileCommands() const override {
    return {Command};
  }

private:
  tooling::CompileCommand Command;
};
}

namespace {
class SharedStream {
public:
  SharedStream(raw_ostream &OS) : OS(OS) {}
  void applyLocked(llvm::function_ref<void(raw_ostream &OS)> Fn) {
    std::unique_lock<std::mutex> LockGuard(Lock);
    Fn(OS);
    OS.flush();
  }

private:
  std::mutex Lock;
  raw_ostream &OS;
};
} // namespace

// FIXME: This is a copy of Command::writeResponseFile. Command is too deeply
// tied with clang::Driver to use directly.
static void writeResponseFile(raw_ostream &OS,
                              SmallVectorImpl<const char *> &Arguments) {
  for (const auto *Arg : Arguments) {
    OS << '"';

    for (; *Arg != '\0'; Arg++) {
      if (*Arg == '\"' || *Arg == '\\') {
        OS << '\\';
      }
      OS << *Arg;
    }

    OS << "\" ";
  }
}

int cc1depscan_main(ArrayRef<const char *> Argv, const char *Argv0,
                    void *MainAddr) {
  // FIXME:: Dedup fixme code.
  SmallString<128> DiagsBuffer;
  llvm::raw_svector_ostream DiagsOS(DiagsBuffer);
  auto DiagsConsumer = std::make_unique<TextDiagnosticPrinter>(
      DiagsOS, new DiagnosticOptions(), false);
  DiagnosticsEngine Diags(new DiagnosticIDs(), new DiagnosticOptions());
  Diags.setClient(DiagsConsumer.get(), /*ShouldOwnClient=*/false);

  // FIXME: Create a new OptionFlag group for cc1depscan.
  const OptTable &Opts = clang::driver::getDriverOptTable();
  unsigned MissingArgIndex, MissingArgCount;
  auto Args = Opts.ParseArgs(Argv, MissingArgIndex, MissingArgCount);
  if (MissingArgCount) {
    Diags.Report(diag::err_drv_missing_argument)
        << Args.getArgString(MissingArgIndex) << MissingArgCount;
    return 1;
  }

  StringRef WorkingDirectory;
  if (auto *Arg =
          Args.getLastArg(clang::driver::options::OPT_working_directory))
    WorkingDirectory = Arg->getValue();

  cc1depscand::AutoPrefixMapping AutoMapping;
  for (auto &Arg :
       Args.getAllArgValues(clang::driver::options::OPT_fdepscan_prefix_map_EQ))
    AutoMapping.PrefixMap.push_back(Arg);
  if (auto *Arg = Args.getLastArg(
          clang::driver::options::OPT_fdepscan_prefix_map_sdk_EQ))
    AutoMapping.NewSDKPath = Arg->getValue();
  if (auto *Arg = Args.getLastArg(
          clang::driver::options::OPT_fdepscan_prefix_map_toolchain_EQ))
    AutoMapping.NewToolchainPath = Arg->getValue();

  // Create the compiler invocation.
  auto Invocation = std::make_shared<CompilerInvocation>();
  auto *CC1Args = Args.getLastArg(clang::driver::options::OPT_cc1_args);
  if (!CC1Args) {
    llvm::errs() << "missing -cc1-args option\n";
    return 1;
  }

  if (!CompilerInvocation::CreateFromArgs(*Invocation, CC1Args->getValues(),
                                          Diags, Argv0)) {
    llvm::errs() << "cannot create compiler invocation\n";
    return 1;
  }

  llvm::BumpPtrAllocator Alloc;
  llvm::StringSaver Saver(Alloc);

  std::unique_ptr<llvm::cas::CASDB> CAS = reportAsFatalIfError(
      llvm::cas::createOnDiskCAS(llvm::cas::getDefaultOnDiskCASPath()));
  IntrusiveRefCntPtr<llvm::cas::CachingOnDiskFileSystem> FS =
      llvm::cantFail(llvm::cas::createCachingOnDiskFileSystem(*CAS));
  tooling::dependencies::DependencyScanningService Service(
      tooling::dependencies::ScanningMode::MinimizedSourcePreprocessing,
      tooling::dependencies::ScanningOutputFormat::Tree, FS,
      /*ReuseFileManager=*/false,
      /*SkipExcludedPPRanges=*/true);
  tooling::dependencies::DependencyScanningTool Tool(Service);

  SmallVector<std::pair<StringRef, StringRef>> ComputedMapping;
  if (auto E = computeFullMapping(Saver, *FS, Argv0, *Invocation, AutoMapping,
                                  ComputedMapping)) {
    llvm::errs() << "failed to compute mapping: "
                 << llvm::toString(std::move(E)) << "\n";
    return 1;
  }

  llvm::Expected<llvm::cas::CASID> Root =
      Tool.getDependencyTreeFromCompilerInvocation(
          std::make_shared<CompilerInvocation>(*Invocation), WorkingDirectory,
          *DiagsConsumer, [&](StringRef Path) {
            return remapPath(Path, Saver, ComputedMapping);
          });

  if (!Root) {
    llvm::errs() << "depscan failed\n";
    return 1;
  }

  std::string RootID = llvm::cantFail(CAS->convertCASIDToString(*Root));
  updateCompilerInvocation(*Invocation, Saver, *FS, RootID, WorkingDirectory,
                           ComputedMapping);

  SmallVector<const char *> NewArgs;
  Invocation->generateCC1CommandLine(
      NewArgs, [&](const llvm::Twine &Arg) { return Saver.save(Arg).begin(); });

  auto *OutputArg = Args.getLastArg(clang::driver::options::OPT_o);

  StringRef OutputPath = OutputArg? OutputArg->getValue() : "-";

  // FIXME: Use OutputBackend to OnDisk only now.
  auto OutputBackend =
      llvm::makeIntrusiveRefCnt<llvm::vfs::OnDiskOutputBackend>();
  auto OutputFile =
      OutputBackend->createFile(OutputPath, llvm::vfs::OutputConfig()
                                                .setText(true)
                                                .setTextWithCRLF(true)
                                                .setCrashCleanup(false)
                                                .setAtomicWrite(false));
  if (!OutputFile) {
    Diags.Report(diag::err_fe_unable_to_open_output)
        << OutputArg->getValue() << llvm::toString(OutputFile.takeError());
    return 1;
  }

  writeResponseFile(*(*OutputFile)->getOS(), NewArgs);

  if (auto Err = (*OutputFile)->close()) {
    llvm::errs() << "failed closing outputfile: "
                 << llvm::toString(std::move(Err)) << "\n";
    return 1;
  }
  return 0;
}

int cc1depscand_main(ArrayRef<const char *> Argv, const char *Argv0,
                     void *MainAddr) {
  ensureSufficientStack();

  if (Argv.size() < 2)
    llvm::report_fatal_error(
        "clang -cc1depscand: missing command and base-path");

  StringRef Command = Argv[0];
  StringRef BasePath = Argv[1];

  if (Command == "-launch") {
    signal(SIGCHLD, SIG_IGN);
    const char *Args[] = {
        Argv0, "-cc1depscand", "-run", BasePath.begin(), nullptr,
    };
    int IgnoredPid;
    int EC = ::posix_spawn(&IgnoredPid, Args[0], /*file_actions=*/nullptr,
                           /*attrp=*/nullptr, const_cast<char **>(Args),
                           /*envp=*/nullptr);
    if (EC)
      llvm::report_fatal_error("clang -cc1depscand: failed to daemonize");
    ::exit(0);
  }

  if (Command != "-run")
    llvm::report_fatal_error("clang -cc1depscand: unknown command '" + Command +
                             "'");

  // Daemonize.
  if (::signal(SIGHUP, SIG_IGN) == SIG_ERR)
    llvm::report_fatal_error("clang -cc1depscand: failed to ignore SIGHUP");
  if (::setsid() == -1)
    llvm::report_fatal_error("clang -cc1depscand: setsid failed");

  // Check the pidfile.
  SmallString<128> PidPath, LogOutPath, LogErrPath;
  (BasePath + ".pid").toVector(PidPath);
  (BasePath + ".out").toVector(LogOutPath);
  (BasePath + ".err").toVector(LogErrPath);

  auto openAndReplaceFD = [&](int ReplacedFD, StringRef Path) {
    int FD;
    if (std::error_code EC = llvm::sys::fs::openFile(
            Path, FD, llvm::sys::fs::CD_CreateAlways, llvm::sys::fs::FA_Write,
            llvm::sys::fs::OF_None)) {
      // Ignoring error?
      ::close(ReplacedFD);
      return;
    }
    ::dup2(FD, ReplacedFD);
    ::close(FD);
  };
  openAndReplaceFD(1, LogOutPath);
  openAndReplaceFD(2, LogErrPath);

  bool ShouldKeepOutputs = true;
  auto DropOutputs = llvm::make_scope_exit([&]() {
    if (ShouldKeepOutputs)
      return;
    ::unlink(LogOutPath.c_str());
    ::unlink(LogErrPath.c_str());
  });

  int PidFD;
  [&]() {
    if (std::error_code EC = llvm::sys::fs::openFile(
            PidPath, PidFD, llvm::sys::fs::CD_OpenAlways,
            llvm::sys::fs::FA_Write, llvm::sys::fs::OF_None))
      llvm::report_fatal_error("clang -cc1depscand: cannot open pidfile");

    // Try to lock; failure means there's another daemon running.
    if (::flock(PidFD, LOCK_EX | LOCK_NB))
      ::exit(0);

    // FIXME: Should we actually write the pid here? Maybe we don't care.
  }();

  // Clean up the pidfile when we're done.
  auto ClosePidFile = llvm::make_scope_exit([&]() { ::close(PidFD); });

  // Open the socket and start listening.
  int ListenSocket = cc1depscand::createSocket();
  if (ListenSocket == -1)
    llvm::report_fatal_error("clang -cc1depscand: cannot open socket");

  if (cc1depscand::bindToSocket(BasePath, ListenSocket))
    llvm::report_fatal_error(StringRef() +
                             "clang -cc1depscand: cannot bind to socket" +
                             ": " + strerror(errno));
  bool IsBound = true;
  auto RemoveBindFile = [&] {
    assert(IsBound);
    cc1depscand::unlinkBoundSocket(BasePath);
    IsBound = false;
  };
  auto RemoveBindFileAtExit = llvm::make_scope_exit([&]() {
    if (IsBound)
      RemoveBindFile();
  });

  llvm::ThreadPool Pool;
  if (::listen(ListenSocket, /*MaxBacklog=*/Pool.getThreadCount() * 16))
    llvm::report_fatal_error("clang -cc1depscand: cannot listen to socket");

  // FIXME: Should use user-specified CAS, if any.
  std::unique_ptr<llvm::cas::CASDB> CAS = reportAsFatalIfError(
      llvm::cas::createOnDiskCAS(llvm::cas::getDefaultOnDiskCASPath()));

  IntrusiveRefCntPtr<llvm::cas::CachingOnDiskFileSystem> FS =
      llvm::cantFail(llvm::cas::createCachingOnDiskFileSystem(*CAS));
  tooling::dependencies::DependencyScanningService Service(
      tooling::dependencies::ScanningMode::MinimizedSourcePreprocessing,
      tooling::dependencies::ScanningOutputFormat::Tree, FS,
      /*ReuseFileManager=*/false,
      /*SkipExcludedPPRanges=*/true);

  std::atomic<bool> ShutDown(false);
  std::atomic<int> NumRunning(0);

  std::chrono::steady_clock::time_point Start =
      std::chrono::steady_clock::now();
  std::atomic<uint64_t> SecondsSinceLastClose;

  SharedStream SharedOS(llvm::errs());

  for (unsigned I = 0; I < Pool.getThreadCount(); ++I) {
    Pool.async([&Service, &ShutDown, &ListenSocket, &NumRunning, &Start,
                &SecondsSinceLastClose, &CAS, I, FS, Argv0, &SharedOS]() {
      Optional<tooling::dependencies::DependencyScanningTool> Tool;
      SmallString<256> Message;
      while (true) {
        if (ShutDown.load())
          return;

        int Data = cc1depscand::acceptSocket(ListenSocket);
        if (Data == -1)
          continue;

        auto CloseData = llvm::make_scope_exit([&]() { ::close(Data); });
        cc1depscand::CC1DepScanDProtocol Comms(Data);

        {
          ++NumRunning;

          // Check again for shutdown, since the main thread could have
          // requested it before we created the service.
          //
          // FIXME: Return Optional<ServiceReference> from the map, handling
          // this condition in getOrCreateService().
          if (ShutDown.load())
            return; // FIXME: Tell the client about this?
        }
        auto StopRunning = llvm::make_scope_exit([&]() {
          SecondsSinceLastClose.store(
              std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::steady_clock::now() - Start)
                  .count());
          --NumRunning;
        });

        llvm::BumpPtrAllocator Alloc;
        llvm::StringSaver Saver(Alloc);
        StringRef WorkingDirectory;
        SmallVector<const char *> Args;
        cc1depscand::AutoPrefixMapping AutoMapping;
        if (llvm::Error E =
                Comms.getCommand(Saver, WorkingDirectory, Args, AutoMapping)) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            OS << I << ": failed to get command\n";
            logAllUnhandledErrors(std::move(E), OS);
          });
          continue; // FIXME: Tell the client something went wrong.
        }

        auto printScannedCC1 = [&](raw_ostream &OS) {
          OS << I << ": scanned -cc1:";
          for (const char *Arg : Args)
            OS << " " << Arg;
          OS << "\n";
        };

        // Is this safe to reuse? Or does DependendencyScanningWorkerFileSystem
        // make some bad assumptions about relative paths?
        if (!Tool)
          Tool.emplace(Service);

        // auto DiagsConsumer = std::make_unique<IgnoringDiagConsumer>();
        SmallString<128> DiagsBuffer;
        llvm::raw_svector_ostream DiagsOS(DiagsBuffer);
        auto DiagsConsumer = std::make_unique<TextDiagnosticPrinter>(
            DiagsOS, new DiagnosticOptions(), false);
        DiagnosticsEngine Diags(new DiagnosticIDs(), new DiagnosticOptions());
        Diags.setClient(DiagsConsumer.get(), /*ShouldOwnClient=*/false);

        // Create the compiler invocation.
        auto Invocation = std::make_shared<CompilerInvocation>();
        if (!CompilerInvocation::CreateFromArgs(*Invocation, Args, Diags,
                                                Args[0])) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            OS << I << ": failed to create compiler invocation\n";
            OS << DiagsBuffer;
          });
          consumeError(Comms.putResultKind(
              cc1depscand::CC1DepScanDProtocol::ErrorResult));
          continue;
        }
        if (!DiagsBuffer.empty()) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            OS << I << ": diags from compiler invocation\n";
            OS << DiagsBuffer;
          });
          DiagsBuffer.clear();
        }

        SmallVector<std::pair<StringRef, StringRef>> ComputedMapping;
        if (llvm::Error E = computeFullMapping(Saver, *FS, Argv0, *Invocation,
                                               AutoMapping, ComputedMapping)) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            logAllUnhandledErrors(std::move(E), OS);
            OS << I << ": failed to compute mapping:\n";
            OS << "  Inputs were:\n";
            if (AutoMapping.NewSDKPath)
              OS << "    sdk => " << *AutoMapping.NewSDKPath << "\n";
            if (AutoMapping.NewToolchainPath)
              OS << "    toolchain => " << *AutoMapping.NewToolchainPath
                 << "\n";
            for (StringRef Map : AutoMapping.PrefixMap)
              OS << "    [" << Map << "]\n";
            OS << "  Got this far:\n";
            for (auto &Pair : ComputedMapping)
              OS << "     " << Pair.first << " => " << Pair.second << "\n";
          });
          consumeError(Comms.putResultKind(
              cc1depscand::CC1DepScanDProtocol::ErrorResult));
          continue;
        }

        // DependencyScanningWorker::runInvocation modifies the
        // CompilerInvocation it's given, so send in a copy.
        llvm::Expected<llvm::cas::CASID> Root =
            Tool->getDependencyTreeFromCompilerInvocation(
                std::make_shared<CompilerInvocation>(*Invocation),
                WorkingDirectory, *DiagsConsumer, [&](StringRef Path) {
                  return remapPath(Path, Saver, ComputedMapping);
                });
        if (!Root) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            OS << I << ": depscan failed\n";
            logAllUnhandledErrors(Root.takeError(), OS);
            OS << I << ": diags\n";
            OS << DiagsBuffer;
          });
          consumeError(Comms.putResultKind(
              cc1depscand::CC1DepScanDProtocol::ErrorResult));
          continue;
        }
        if (!DiagsBuffer.empty()) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            OS << I << ": diags from depscan\n";
            OS << DiagsBuffer;
          });
          DiagsBuffer.clear();
        }

        std::string RootID = llvm::cantFail(CAS->convertCASIDToString(*Root));
        updateCompilerInvocation(*Invocation, Saver, *FS, RootID,
                                 WorkingDirectory, ComputedMapping);

        SmallVector<const char *> NewArgs;
        Invocation->generateCC1CommandLine(
            NewArgs,
            [&](const llvm::Twine &Arg) { return Saver.save(Arg).begin(); });

        if (llvm::Error E = Comms.putResultKind(
                cc1depscand::CC1DepScanDProtocol::SuccessResult)) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            logAllUnhandledErrors(std::move(E), OS);
          });
          continue; // FIXME: Tell the client something went wrong.
        }

        auto printComputedCC1 = [&](raw_ostream &OS) {
          OS << I << ": sending back new -cc1 args:\n";
          for (const char *Arg : NewArgs)
            OS << " " << Arg;
          OS << "\n";
        };
        if (llvm::Error E = Comms.putArgs(NewArgs)) {
          SharedOS.applyLocked([&](raw_ostream &OS) {
            printScannedCC1(OS);
            printComputedCC1(OS);
            logAllUnhandledErrors(std::move(E), OS);
          });
          continue; // FIXME: Tell the client something went wrong.
        }

        // Done!
#ifndef NDEBUG
        // In +asserts mode, print out -cc1s even on success.
        SharedOS.applyLocked([&](raw_ostream &OS) {
          printScannedCC1(OS);
          printComputedCC1(OS);
        });
#endif
      }
    });
  };

  // Wait for the work to finish.
  const uint64_t SecondsBetweenAttempts = 5;
  const uint64_t SecondsBeforeDestruction = 15;
  uint64_t SleepTime = SecondsBeforeDestruction;
  while (true) {
    ::sleep(SleepTime);
    SleepTime = SecondsBetweenAttempts;

    if (NumRunning.load())
      continue;

    if (ShutDown.load())
      break;

    // Figure out the latest access time that we'll delete.
    uint64_t LastAccessToDestroy =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - Start)
            .count();
    if (LastAccessToDestroy < SecondsBeforeDestruction)
      continue; // In case ::sleep returns slightly early.
    LastAccessToDestroy -= SecondsBeforeDestruction;

    if (LastAccessToDestroy < SecondsSinceLastClose)
      continue;

    ShutDown.store(true);
  }

  RemoveBindFile();
  ::close(ListenSocket);

  return 0;
}
