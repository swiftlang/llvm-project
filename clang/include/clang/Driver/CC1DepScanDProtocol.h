//===- CC1DepScanDProtocol.h - Communications for -cc1depscand ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_DRIVER_CC1DEPSCANDPROTOCOL_H
#define LLVM_CLANG_DRIVER_CC1DEPSCANDPROTOCOL_H

#include "clang/Basic/LLVM.h"
#include "clang/Driver/CC1DepScanDClient.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/StringSaver.h"
#include <spawn.h>      // FIXME: Unix-only. Not portable.
#include <sys/socket.h> // FIXME: Unix-only. Not portable.
#include <sys/types.h>  // FIXME: Unix-only. Not portable.
#include <sys/un.h>     // FIXME: Unix-only. Not portable.
#include <unistd.h>     // FIXME: Unix-only. Not portable.

namespace clang {
namespace cc1depscand {

StringRef getBasePath();

int createSocket();
int connectToSocket(StringRef BasePath, int Socket);
int bindToSocket(StringRef BasePath, int Socket);
void unlinkBoundSocket(StringRef BasePath);
int acceptSocket(int Socket);

class CC1DepScanDProtocol {
public:
  llvm::Error getMessage(size_t BytesToRead, SmallVectorImpl<char> &Bytes) {
    Bytes.clear();
    while (BytesToRead) {
      constexpr size_t BufferSize = 4096;
      char Buffer[BufferSize];
      size_t BytesRead = ::read(Socket, static_cast<void *>(Buffer),
                                std::min(BytesToRead, BufferSize));
      if (BytesRead == -1ull)
        return llvm::errorCodeToError(
            std::error_code(errno, std::generic_category()));
      Bytes.append(Buffer, Buffer + BytesRead);

      if (!BytesRead || BytesRead > BytesToRead)
        return llvm::errorCodeToError(
            std::error_code(EMSGSIZE, std::generic_category()));
      BytesToRead -= BytesRead;
    }
    return llvm::Error::success();
  }

  llvm::Error putMessage(size_t BytesToWrite, const char *Bytes) {
    while (BytesToWrite) {
      size_t BytesWritten = ::write(Socket, Bytes, BytesToWrite);
      if (BytesWritten == -1ull)
        return llvm::errorCodeToError(
            std::error_code(errno, std::generic_category()));

      if (!BytesWritten || BytesWritten > BytesToWrite)
        return llvm::errorCodeToError(
            std::error_code(EMSGSIZE, std::generic_category()));
      BytesToWrite -= BytesWritten;
      Bytes += BytesWritten;
    }
    return llvm::Error::success();
  }

  template <class NumberT> llvm::Error getNumber(NumberT &Number) {
    // FIXME: Assumes endianness matches.
    if (llvm::Error E = getMessage(sizeof(Number), Message))
      return E;
    ::memcpy(&Number, Message.begin(), sizeof(Number));
    return llvm::Error::success();
  }
  template <class NumberT> llvm::Error putNumber(NumberT Number) {
    // FIXME: Assumes endianness matches.
    return putMessage(sizeof(Number), reinterpret_cast<const char *>(&Number));
  }

  enum ResultKind {
    ErrorResult,
    SuccessResult,
    InvalidResult, // Last...
  };
  llvm::Error putResultKind(ResultKind Result) {
    return putNumber(unsigned(Result));
  }
  llvm::Error getResultKind(ResultKind &Result) {
    unsigned Read;
    if (llvm::Error E = getNumber(Read))
      return E;
    if (Read >= InvalidResult)
      Result = InvalidResult;
    else
      Result = ResultKind(Read);
    return llvm::Error::success();
  }

  /// Read a string. \p String will be a null-terminated string owned by \p
  /// Saver.
  llvm::Error getString(llvm::StringSaver &Saver, StringRef &String) {
    size_t Length;
    if (llvm::Error E = getNumber(Length))
      return E;
    if (llvm::Error E = getMessage(Length, Message))
      return E;
    String = Saver.save(StringRef(Message));
    return llvm::Error::success();
  }
  llvm::Error putString(StringRef String) {
    if (llvm::Error E = putNumber(String.size()))
      return E;
    return putMessage(String.size(), String.begin());
  }

  llvm::Error putDepscanPrefixMapping(const DepscanPrefixMapping &Mapping);
  llvm::Error getDepscanPrefixMapping(llvm::StringSaver &Saver,
                                      DepscanPrefixMapping &Mapping);

  llvm::Error putArgs(ArrayRef<const char *> Args);
  llvm::Error getArgs(llvm::StringSaver &Saver,
                      SmallVectorImpl<const char *> &Args);

  llvm::Error putCommand(StringRef WorkingDirectory,
                         ArrayRef<const char *> Args,
                         const DepscanPrefixMapping &Mapping);
  llvm::Error getCommand(llvm::StringSaver &Saver, StringRef &WorkingDirectory,
                         SmallVectorImpl<const char *> &Args,
                         DepscanPrefixMapping &Mapping);

  llvm::Error putAutoArgEdits(ArrayRef<AutoArgEdit> Edits);
  llvm::Error getAutoArgEdits(llvm::StringSaver &Saver, SmallVectorImpl<AutoArgEdit> &Edits);

  llvm::Error putScanResult(StringRef RootID, ArrayRef<AutoArgEdit> Edits);
  llvm::Error getScanResult(llvm::StringSaver &Saver,
                            StringRef &RootID, SmallVectorImpl<AutoArgEdit> &Edits);

  explicit CC1DepScanDProtocol(int Socket) : Socket(Socket) {}
  CC1DepScanDProtocol() = delete;

private:
  int Socket;
  SmallString<128> Message;
};

} // namespace cc1depscand
} // namespace clang

#endif // LLVM_CLANG_DRIVER_CC1DEPSCANDPROTOCOL_H
