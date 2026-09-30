//===- llvm/RemoteCachingService/RemoteCacheServer.h ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_REMOTECACHINGSERVICE_REMOTECACHESERVER_H
#define LLVM_REMOTECACHINGSERVICE_REMOTECACHESERVER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Error.h"

namespace llvm::cas::remote {

class RemoteCacheProvider;

/// A gRPC service for the remote caching protocol.
class RemoteCacheServer {
public:
  /// Returns a gRPC server for the remote caching protocol.
  LLVM_ABI
  RemoteCacheServer(StringRef SocketPath,
                    std::unique_ptr<RemoteCacheProvider> CacheProvider);

  LLVM_ABI ~RemoteCacheServer();

  LLVM_ABI void Start();
  LLVM_ABI void Listen();
  LLVM_ABI void Shutdown();

private:
  class Implementation;

  RemoteCacheServer(std::unique_ptr<RemoteCacheServer::Implementation> Impl);
  std::unique_ptr<RemoteCacheServer::Implementation> Impl;
};

} // namespace llvm::cas::remote

#endif
