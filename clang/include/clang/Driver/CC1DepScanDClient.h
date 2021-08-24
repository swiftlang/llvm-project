//===- CC1DepScanDClient.h - Client API for -cc1depscand ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_DRIVER_CC1DEPSCANDCLIENT_H
#define LLVM_CLANG_DRIVER_CC1DEPSCANDCLIENT_H

#include <clang/Basic/LLVM.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include <vector>

namespace llvm {
namespace opt {

class ArgList;

}
}
namespace clang {
namespace cc1depscand {

struct AutoPrefixMapping {
  Optional<StringRef> NewSDKPath;
  Optional<StringRef> NewToolchainPath;
  SmallVector<StringRef> PrefixMap;
};

struct AutoArgEdit {
  uint32_t Index = -1u;
  StringRef NewArg;
};

/// Need DriverArgs just to get access to ArgList::MakeArgString(). This should
/// be changed to a function ref.
void addCC1ScanDepsArgs(const char *Exec, SmallVectorImpl<const char *> &Argv,
                        const AutoPrefixMapping &Mapping,
                        llvm::function_ref<const char *(const Twine &)> SaveArg);

} // namespace cc1depscand
} // namespace clang

#endif // LLVM_CLANG_DRIVER_CC1DEPSCANDCLIENT_H
