//===- CC1DepScanDProtocol.cpp - Communications for -cc1depscand ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Driver/CC1DepScanDProtocol.h"
#include "clang/Driver/CC1DepScanDClient.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include <sys/socket.h> // FIXME: Unix-only. Not portable.
#include <sys/types.h>  // FIXME: Unix-only. Not portable.
#include <sys/un.h>     // FIXME: Unix-only. Not portable.

using namespace clang;
using namespace clang::cc1depscand;
using namespace llvm;

int cc1depscand::createSocket() { return ::socket(AF_UNIX, SOCK_STREAM, 0); }

int cc1depscand::acceptSocket(int Socket) {
  sockaddr_un DataAddress;
  socklen_t DataLength;
  return ::accept(Socket, reinterpret_cast<sockaddr *>(&DataAddress),
                  &DataLength);
}

static sockaddr_un configureAddress(StringRef BasePath) {
  sockaddr_un Address;
  SmallString<128> SocketPath = BasePath;
  SocketPath.append(".socket");

  Address.sun_family = AF_UNIX;
  if (SocketPath.size() >= sizeof(Address.sun_path))
    llvm::cantFail(llvm::errorCodeToError(
        std::error_code(ENAMETOOLONG, std::generic_category())));
  ::strncpy(Address.sun_path, SocketPath.c_str(), sizeof(Address.sun_path));
  return Address;
}

void cc1depscand::unlinkBoundSocket(StringRef BasePath) {
  SmallString<128> SocketPath = BasePath;
  SocketPath.append(".socket");
  ::unlink(SocketPath.c_str());
}

int cc1depscand::connectToSocket(StringRef BasePath, int Socket) {
  sockaddr_un Address = configureAddress(BasePath);
  return ::connect(Socket, reinterpret_cast<sockaddr *>(&Address),
                   sizeof(Address));
}
int cc1depscand::bindToSocket(StringRef BasePath, int Socket) {
  sockaddr_un Address = configureAddress(BasePath);
  if (int Failure = ::bind(Socket, reinterpret_cast<sockaddr *>(&Address),
                           sizeof(Address))) {
    if (errno == EADDRINUSE) {
      unlinkBoundSocket(BasePath);
      Failure = ::bind(Socket, reinterpret_cast<sockaddr *>(&Address),
                       sizeof(Address));
    }
    if (Failure)
      return Failure;
  }

  // FIXME: shouldn't compute socket path twice. Also, not sure this is working
  // on crashes.
  SmallString<128> SocketPath = BasePath;
  SocketPath.append(".socket");
  llvm::sys::RemoveFileOnSignal(SocketPath);
  return 0;
}

StringRef cc1depscand::getBasePath() {
  static Optional<SmallString<128>> BasePath = None;

  // FIXME: Not thread-safe, but does the driver need threads?
  if (BasePath)
    return *BasePath;

  // Construct the path.
  //
  // FIXME: Using ppid is non-portable and totally unsound. Can't check pgrp,
  // since ninja sets a different pgrp for each clang invocation. Probably we
  // should add the CASID of the clang executable, which maybe needs to be
  // computed anyway...
  //
  // FIXME: We should also check that the parent is an actual build system
  // (like ninja) to avoid merging jobs in simple shell invocations, where the
  // sources may have changed in between.
  BasePath.emplace();
  llvm::sys::path::system_temp_directory(/*ErasedOnReboot=*/true, *BasePath);
  llvm::sys::path::append(*BasePath, "cc1scand." + Twine(::getppid()));

  // Ensure null-termination.
  (void)BasePath->c_str();
  return *BasePath;
}

namespace {

class FileDescriptor {
public:
  operator int() const { return FD; }

  FileDescriptor() = default;
  explicit FileDescriptor(int FD) : FD(FD) {}

  FileDescriptor(FileDescriptor &&X) : FD(X) { X.FD = -1; }
  FileDescriptor &operator=(FileDescriptor &&X) {
    close();
    FD = X.FD;
    X.FD = -1;
    return *this;
  }

  FileDescriptor(const FileDescriptor &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;

  ~FileDescriptor() { close(); }

private:
  void close() {
    if (FD == -1)
      return;
    ::close(FD);
    FD = -1;
  }
  int FD = -1;
};

class OpenSocket : public FileDescriptor {
public:
  static Expected<OpenSocket> create(StringRef BasePath);

  OpenSocket() = delete;

  OpenSocket(OpenSocket &&) = default;
  OpenSocket &operator=(OpenSocket &&Socket) = default;

private:
  explicit OpenSocket(int FD) : FileDescriptor(FD) {}
};

class ScanDaemon : public OpenSocket {
public:
  static Expected<ScanDaemon> create(StringRef BasePath, const char *Arg0);

  static Expected<ScanDaemon> constructAndShakeHands(StringRef BasePath,
                                                     const char *Arg0);

private:
  static Expected<ScanDaemon> launchDaemon(StringRef BasePath,
                                           const char *Arg0);
  static Expected<ScanDaemon> connectToDaemon(StringRef BasePath,
                                              bool ShouldWait);
  static Expected<ScanDaemon> connectToExistingDaemon(StringRef BasePath) {
    return connectToDaemon(BasePath, /*ShouldWait=*/false);
  }
  static Expected<ScanDaemon> connectToJustLaunchedDaemon(StringRef BasePath) {
    return connectToDaemon(BasePath, /*ShouldWait=*/true);
  }

  Error shakeHands() const;

  explicit ScanDaemon(OpenSocket S) : OpenSocket(std::move(S)) {}
};
}

Expected<OpenSocket> OpenSocket::create(StringRef BasePath) {
  OpenSocket Socket(::socket(AF_UNIX, SOCK_STREAM, 0));
  if (Socket == -1)
    return llvm::errorCodeToError(
        std::error_code(errno, std::generic_category()));
  return std::move(Socket);
}

Expected<ScanDaemon> ScanDaemon::connectToDaemon(StringRef BasePath,
                                                 bool ShouldWait) {
  Expected<OpenSocket> Socket = OpenSocket::create(BasePath);
  if (!Socket)
    return Socket.takeError();

  // Wait up to 30 seconds.
  constexpr int MaxWait = 30 * 1000 * 1000;
  int NextBackoff = 0;
  int TotalBackoff = 0;
  while (TotalBackoff < MaxWait) {
    TotalBackoff += NextBackoff;
    if (NextBackoff > 0) {
      if (!ShouldWait)
        break;
      ::usleep(NextBackoff);
    }
    ++NextBackoff;

    // The daemon owns the .pid. Try to connect to the server with the .socket.
    if (!cc1depscand::connectToSocket(BasePath, *Socket))
      return ScanDaemon(std::move(*Socket));

    if (errno != ENOENT && errno != ECONNREFUSED)
      return llvm::errorCodeToError(
          std::error_code(errno, std::generic_category()));
  }

  return llvm::errorCodeToError(
      std::error_code(ENOENT, std::generic_category()));
}

Expected<ScanDaemon> ScanDaemon::launchDaemon(StringRef BasePath,
                                              const char *Arg0) {
  std::string BasePathCStr = BasePath.str();
  const char *Args[] = {
      Arg0,
      "-cc1depscand",
      "-run", // -launch if we want the daemon to fork itself.
      BasePathCStr.c_str(),
      nullptr,
  };

  const char *ArgsTestMode[] = {
      Arg0,
      "-cc1depscand",
      "-run", // -launch if we want the daemon to fork itself.
      BasePathCStr.c_str(),
      "-shutdown",
      nullptr,
  };

  static bool LaunchTestDaemon =
      llvm::sys::Process::GetEnv("__CLANG_TEST_CC1DEPSCAND_SHUTDOWN")
          .hasValue();

  char **LaunchArgs = LaunchTestDaemon ? const_cast<char **>(ArgsTestMode)
                                       : const_cast<char **>(Args);

  // Only do it the first time.
  LaunchTestDaemon = false;

  ::pid_t Pid;
  int EC = ::posix_spawn(&Pid, Args[0], /*file_actions=*/nullptr,
                         /*attrp=*/nullptr, LaunchArgs,
                         /*envp=*/nullptr);
  if (EC)
    return llvm::errorCodeToError(std::error_code(EC, std::generic_category()));

  // If we use -launch above, we should do the following here:
  //
  // int Status;
  // if (::waitpid(Pid, &Status, 0) != Pid)
  //   return createStringError("failed to spawn clang -cc1depscand");
  // if (!WIFEXITED(Status) || WEXITSTATUS(Status))
  //   return createStringError("clang -cc1depscand failed to launch");

  return connectToJustLaunchedDaemon(BasePath);
}

Expected<ScanDaemon> ScanDaemon::create(StringRef BasePath, const char *Arg0) {
  if (Expected<ScanDaemon> Daemon = connectToExistingDaemon(BasePath))
    return Daemon;
  else
    llvm::consumeError(Daemon.takeError()); // FIXME: Sometimes return.

  return launchDaemon(BasePath, Arg0);
}

Expected<ScanDaemon> ScanDaemon::constructAndShakeHands(StringRef BasePath,
    const char *Arg0) {
  auto Daemon = ScanDaemon::create(BasePath, Arg0);
  if (!Daemon)
    return Daemon.takeError();

  // If handshake failed, try relaunch the daemon.
  if (auto E = Daemon->shakeHands()) {
    logAllUnhandledErrors(std::move(E), llvm::errs(),
                          "Restarting daemon due to error: ");

    auto NewDaemon = launchDaemon(BasePath, Arg0);
    // If recover failed, return Error.
    if (!NewDaemon)
      return NewDaemon.takeError();
    if (auto NE = NewDaemon->shakeHands())
      return std::move(NE);

    return NewDaemon;
  }

  return Daemon;
}

Error ScanDaemon::shakeHands() const {
  cc1depscand::CC1DepScanDProtocol Comms(*this);
  cc1depscand::CC1DepScanDProtocol::ResultKind Result;
  if (auto E = Comms.getResultKind(Result))
    return E;

  if (Result != cc1depscand::CC1DepScanDProtocol::SuccessResult)
    return llvm::errorCodeToError(
        std::error_code(ENOTCONN, std::generic_category()));

  return llvm::Error::success();
}

static void reportAsFatalIfError(llvm::Error E) {
  if (!E)
    return;
  llvm::logAllUnhandledErrors(std::move(E), llvm::errs());
  abort();
}

template <typename T> static T reportAsFatalIfError(Expected<T> ValOrErr) {
  if (!ValOrErr)
    reportAsFatalIfError(ValOrErr.takeError());

  return std::move(*ValOrErr);
}

llvm::Error CC1DepScanDProtocol::putArgs(ArrayRef<const char *> Args) {
  // Construct the args block.
  SmallString<256> ArgsBlock;
  for (const char *Arg : Args) {
    ArgsBlock.append(Arg);
    ArgsBlock.push_back(0);
  }
  return putString(ArgsBlock);
}

llvm::Error CC1DepScanDProtocol::getArgs(llvm::StringSaver &Saver,
                                         SmallVectorImpl<const char *> &Args) {
  StringRef ArgsBlock;
  if (llvm::Error E = getString(Saver, ArgsBlock))
    return E;

  // Parse the args block.
  assert(Args.empty());
  for (auto I = ArgsBlock.begin(), B = I, E = ArgsBlock.end(); I != E; ++I)
    if (I == B || !I[-1])
      Args.push_back(I);

  return Error::success();
}

llvm::Error CC1DepScanDProtocol::putCommand(StringRef WorkingDirectory,
                                            ArrayRef<const char *> Args,
                                            const AutoPrefixMapping &Mapping) {
  if (llvm::Error E = putString(WorkingDirectory))
    return E;
  if (llvm::Error E = putArgs(Args))
    return E;
  return putAutoPrefixMapping(Mapping);
}

llvm::Error CC1DepScanDProtocol::getCommand(llvm::StringSaver &Saver,
                                            StringRef &WorkingDirectory,
                                            SmallVectorImpl<const char *> &Args,
                                            AutoPrefixMapping &Mapping) {
  if (llvm::Error E = getString(Saver, WorkingDirectory))
    return E;
  if (llvm::Error E = getArgs(Saver, Args))
    return E;
  return getAutoPrefixMapping(Saver, Mapping);
}

llvm::Error CC1DepScanDProtocol::putAutoPrefixMapping(const AutoPrefixMapping &Mapping) {
  // Construct the message.
  SmallString<256> FullMapping;
  if (Mapping.NewSDKPath)
    FullMapping.append(*Mapping.NewSDKPath);
  FullMapping.push_back(0);
  if (Mapping.NewToolchainPath)
    FullMapping.append(*Mapping.NewToolchainPath);
  FullMapping.push_back(0);
  for (StringRef Map : Mapping.PrefixMap) {
    FullMapping.append(Map);
    FullMapping.push_back(0);
  }
  return putString(FullMapping);
}

llvm::Error CC1DepScanDProtocol::getAutoPrefixMapping(llvm::StringSaver &Saver,
                                                      AutoPrefixMapping &Mapping) {
  StringRef FullMapping;
  if (llvm::Error E = getString(Saver, FullMapping))
    return E;

  // Parse the mapping.
  size_t Count = 0;
  for (auto I = FullMapping.begin(), B = I, E = FullMapping.end();
        I != E; ++I) {
    if (I != B && I[-1])
      continue; // Wait for null-terminator.
    StringRef Map = I;
    switch (Count++) {
    case 0:
      if (!Map.empty())
        Mapping.NewSDKPath = Map;
      break;
    case 1:
      if (!Map.empty())
        Mapping.NewToolchainPath = Map;
      break;
    default:
      Mapping.PrefixMap.push_back(Map);
      break;
    }
  }
  return llvm::Error::success();
}

llvm::Error CC1DepScanDProtocol::putAutoArgEdits(ArrayRef<AutoArgEdit> Edits) {
  SmallString<256> AllEdits;
  size_t NumEdits = Edits.size();
  if (Error E = putNumber(NumEdits))
    return E;
  for (auto &Edit : Edits) {
    if (Error E = putNumber(Edit.Index))
      return E;
    if (Error E = putString(Edit.NewArg))
      return E;
  }
  return Error::success();
}

llvm::Error CC1DepScanDProtocol::getAutoArgEdits(llvm::StringSaver &Saver,
                                                 SmallVectorImpl<AutoArgEdit> &Edits) {
  size_t NumEdits = 0;
  if (Error E = getNumber(NumEdits))
    return E;
  while (NumEdits--) {
    Edits.emplace_back();
    if (Error E = getNumber(Edits.back().Index))
      return E;
    if (Error E = getString(Saver, Edits.back().NewArg))
      return E;
  }
  return Error::success();
}

llvm::Error CC1DepScanDProtocol::putScanResult(StringRef RootID, ArrayRef<AutoArgEdit> Edits) {
  if (Error E = putString(RootID))
    return E;
  if (Error E = putAutoArgEdits(Edits))
    return E;
  return Error::success();
}

llvm::Error CC1DepScanDProtocol::getScanResult(llvm::StringSaver &Saver,
                                               StringRef &RootID, SmallVectorImpl<AutoArgEdit> &Edits) {
  if (Error E = getString(Saver, RootID))
    return E;
  if (Error E = getAutoArgEdits(Saver, Edits))
    return E;
  return Error::success();
}

void cc1depscand::addCC1ScanDepsArgs(const char *Exec, SmallVectorImpl<const char *> &Argv,
                                     const AutoPrefixMapping &Mapping,
                                     llvm::function_ref<const char *(const Twine &)> SaveArg) {
  SmallString<128> BasePath = cc1depscand::getBasePath();

  // FIXME: Skip some of this if -fcas-fs has been passed.
  SmallString<128> WorkingDirectory;
  reportAsFatalIfError(
      llvm::errorCodeToError(llvm::sys::fs::current_path(WorkingDirectory)));

  //llvm::dbgs() << "connecting to daemon...\n";
  ScanDaemon Daemon =
      reportAsFatalIfError(ScanDaemon::constructAndShakeHands(BasePath, Exec));
  cc1depscand::CC1DepScanDProtocol Comms(Daemon);

  //llvm::dbgs() << "sending request...\n";
  reportAsFatalIfError(Comms.putCommand(WorkingDirectory, Argv, Mapping));

  llvm::BumpPtrAllocator Alloc;
  llvm::StringSaver Saver(Alloc);
  SmallVector<AutoArgEdit> ArgEdits;
  SmallVector<const char *> NewArgs;
  cc1depscand::CC1DepScanDProtocol::ResultKind Result;
  reportAsFatalIfError(Comms.getResultKind(Result));
  if (Result != cc1depscand::CC1DepScanDProtocol::SuccessResult)
    llvm::report_fatal_error("-cc1depscand failed");
  reportAsFatalIfError(Comms.getArgs(Saver, NewArgs));
  //llvm::dbgs() << "got answer...\n";

  // FIXME: Avoid this duplication.
  Argv.resize(NewArgs.size() + 1);
  for (int I = 0, E = NewArgs.size(); I != E; ++I)
    Argv[I + 1] = SaveArg(NewArgs[I]);

  // llvm::dbgs() << "running:";
  // for (const char *Arg : Argv)
  //   llvm::dbgs() << " " << Arg;
  // llvm::dbgs() << "\n";

  // llvm::dbgs() << "compiling...\n";
}
