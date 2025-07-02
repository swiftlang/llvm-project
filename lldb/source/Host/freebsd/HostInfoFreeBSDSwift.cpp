//===-- HostInfoFreeBSD.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/freebsd/HostInfoFreeBSD.h"
#include "llvm/Support/FormatVariadic.h"
#include <cstdio>
#include <cstring>
#include <optional>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

using namespace lldb_private;

/// The SDK is the directory where the system C headers, libraries, can be found.
/// On FreeBSD this is simply the root directory.
llvm::Expected<llvm::StringRef> HostInfoFreeBSD::GetSDKRoot(SDKOptions options) {
  return "/";
}
