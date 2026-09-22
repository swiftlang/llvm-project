//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Git's object layout: the first digest byte names a subdirectory and the
/// remaining 31 bytes name the file inside it. All 256 subdirectories are
/// created up front so the insert path never calls mkdir.
///
//===----------------------------------------------------------------------===//

#include "CASBackends.h"
#include "KeyValueCAS.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"

#include <cerrno>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casbench;

namespace {

constexpr char HexDigits[] = "0123456789abcdef";

class FileCAS : public KeyValueCAS {
public:
  FileCAS(const BackendOptions &Opts) : Opts(Opts) {}

  Error open(StringRef Dir, bool ForWrite) {
    Root.assign(Dir.begin(), Dir.end());
    if (!ForWrite)
      return Error::success();

    for (unsigned I = 0; I != 256; ++I) {
      SmallString<256> Sub(Root);
      char Name[3] = {HexDigits[I >> 4], HexDigits[I & 0xF], '\0'};
      sys::path::append(Sub, Name);
      if (std::error_code EC = sys::fs::create_directories(Sub))
        return createFileError(Sub, EC);
    }
    return Error::success();
  }

  Error putBlob(ArrayRef<uint8_t> Key, ArrayRef<uint8_t> Blob) final {
    SmallString<256> Path;
    buildPath(Key, Path);
#ifndef _WIN32
    if (Opts.Durable)
      return writeDurably(Path, Blob);

    int FD = ::open(Path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (FD < 0) {
      // Content addressed: an existing file already holds these exact bytes.
      if (errno == EEXIST)
        return Error::success();
      return createFileError(Path, errnoAsErrorCode());
    }
    Error E = writeAll(FD, Blob);
    ::close(FD);
    return E;
#else
    std::error_code EC;
    raw_fd_ostream OS(Path, EC, sys::fs::CD_CreateNew, sys::fs::FA_Write,
                      sys::fs::OF_None);
    if (EC == std::errc::file_exists)
      return Error::success();
    if (EC)
      return createFileError(Path, EC);
    OS.write(reinterpret_cast<const char *>(Blob.data()), Blob.size());
    OS.close();
    if (OS.has_error())
      return createFileError(Path, OS.error());
    return Error::success();
#endif
  }

  Expected<std::optional<ArrayRef<uint8_t>>>
  getBlob(ArrayRef<uint8_t> Key) final {
    SmallString<256> Path;
    buildPath(Key, Path);
#ifndef _WIN32
    int FD = ::open(Path.c_str(), O_RDONLY);
    if (FD < 0) {
      if (errno == ENOENT)
        return std::nullopt;
      return createFileError(Path, errnoAsErrorCode());
    }
    auto CloseFD = scope_exit([&] { ::close(FD); });

    struct stat SB;
    if (::fstat(FD, &SB) != 0)
      return createFileError(Path, errnoAsErrorCode());

    // Unlike LMDB there is no mapping to point into, so read into the
    // store's own buffer.
    MutableArrayRef<uint8_t> Buf = scratch(size_t(SB.st_size));
    size_t Offset = 0;
    while (Offset != Buf.size()) {
      ssize_t N = ::read(FD, Buf.data() + Offset, Buf.size() - Offset);
      if (N < 0) {
        if (errno == EINTR)
          continue;
        return createFileError(Path, errnoAsErrorCode());
      }
      if (N == 0)
        return createStringError(errc::io_error, "%s: short read",
                                 Path.c_str());
      Offset += size_t(N);
    }
    return ArrayRef<uint8_t>(Buf);
#else
    auto Buf = MemoryBuffer::getFile(Path, /*IsText=*/false,
                                     /*RequiresNullTerminator=*/false);
    if (!Buf) {
      if (Buf.getError() == std::errc::no_such_file_or_directory)
        return std::nullopt;
      return createFileError(Path, Buf.getError());
    }
    return copyToScratch(arrayRefFromStringRef<uint8_t>((*Buf)->getBuffer()));
#endif
  }

  Expected<bool> containsBlob(ArrayRef<uint8_t> Key) final {
    SmallString<256> Path;
    buildPath(Key, Path);
    return sys::fs::exists(Path);
  }

private:
  /// <root>/<first digest byte in hex>/<remaining 31 bytes in hex>
  void buildPath(ArrayRef<uint8_t> Key, SmallString<256> &Out) const {
    Out.assign(Root);
    Out.push_back('/');
    Out.push_back(HexDigits[Key[0] >> 4]);
    Out.push_back(HexDigits[Key[0] & 0xF]);
    Out.push_back('/');
    for (size_t I = 1, E = Key.size(); I != E; ++I) {
      Out.push_back(HexDigits[Key[I] >> 4]);
      Out.push_back(HexDigits[Key[I] & 0xF]);
    }
  }

#ifndef _WIN32
  Error writeAll(int FD, ArrayRef<uint8_t> Blob) {
    size_t Offset = 0;
    while (Offset != Blob.size()) {
      ssize_t N = ::write(FD, Blob.data() + Offset, Blob.size() - Offset);
      if (N < 0) {
        if (errno == EINTR)
          continue;
        return errorCodeToError(errnoAsErrorCode());
      }
      Offset += size_t(N);
    }
    return Error::success();
  }

  /// Durable mode: write to a temporary, fsync, then rename into place, which
  /// is what a real content store would have to do.
  Error writeDurably(StringRef Path, ArrayRef<uint8_t> Blob) {
    if (sys::fs::exists(Path))
      return Error::success();

    SmallString<256> Tmp(Path);
    Tmp += ".tmp";
    int FD = ::open(Tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (FD < 0)
      return createFileError(Tmp, errnoAsErrorCode());
    Error E = writeAll(FD, Blob);
    if (!E && ::fsync(FD) != 0)
      E = errorCodeToError(errnoAsErrorCode());
    ::close(FD);
    if (E) {
      ::unlink(Tmp.c_str());
      return E;
    }
    SmallString<256> Dest(Path);
    if (::rename(Tmp.c_str(), Dest.c_str()) != 0) {
      std::error_code EC = errnoAsErrorCode();
      ::unlink(Tmp.c_str());
      return createFileError(Path, EC);
    }
    return Error::success();
  }
#endif

  BackendOptions Opts;
  SmallString<256> Root;
};

} // end anonymous namespace

Expected<std::unique_ptr<ObjectStore>>
llvm::casbench::createFileCAS(StringRef Dir, bool ForWrite,
                              const BackendOptions &Opts) {
  auto CAS = std::make_unique<FileCAS>(Opts);
  if (Error E = CAS->open(Dir, ForWrite))
    return std::move(E);
  return std::unique_ptr<ObjectStore>(std::move(CAS));
}
