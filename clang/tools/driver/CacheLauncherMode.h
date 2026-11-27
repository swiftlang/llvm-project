//===-- CacheLauncherMode.h - clang-cache driver mode -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_DRIVER_CACHELAUNCHERMODE_H
#define LLVM_CLANG_TOOLS_DRIVER_CACHELAUNCHERMODE_H

#include "clang/Basic/LLVM.h"

namespace llvm {
class StringSaver;
}

namespace clang {

class DiagnosticsEngine;

/// Invoked at the beginning of a "/path/to/clang-cache /path/to/clang -c ..."
/// invocation. If "/path/to/clang" points to the same clang binary as
/// "/path/to/clang-cache" then the arguments will be adjusted for compilation
/// caching, and the function will return, otherwise the "/path/to/clang -c ..."
/// invocation will be invoked as a separate process and its exit value will be
/// returned.
///
/// \returns \p None if the arguments got adjusted, or the exit code to return.
std::optional<int>
handleClangCacheInvocation(SmallVectorImpl<const char *> &Args,
                           llvm::StringSaver &Saver);

/// Get the time-trace granularity in microseconds for clang-cache.
///
/// The granularity is taken from the environment variable
/// CLANG_CACHE_TIME_TRACE_GRANULARITY, or is 500 if that is unset.
///
/// Returns \p std::nullopt if the environment variable CLANG_CACHE_TIME_TRACE
/// is unset or the granularity is not a number. A granularity that is not a
/// number is reported to \p Diags, if given.
std::optional<unsigned>
getClangCacheTimeTraceGranularity(DiagnosticsEngine *Diags = nullptr);

} // namespace clang

#endif
