//===-- SwiftDemangle.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Swift demangling for core lldb. SwiftLanguageRuntime installs the
// implementation, so tools that link core lldb without the Swift plugins do not
// link the Swift demangler. Until it is installed, nothing is demangled.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_CORE_SWIFTDEMANGLE_H
#define LLDB_CORE_SWIFTDEMANGLE_H

#ifdef LLDB_ENABLE_SWIFT

#include "lldb/Core/DemangledNameInfo.h"
#include "lldb/Utility/ConstString.h"
#include "llvm/ADT/StringRef.h"

#include <string>
#include <utility>

namespace lldb_private {

class SymbolContext;

namespace SwiftDemangle {

enum DemangleMode { eSimplified, eTypeName, eDisplayTypeName };

struct Callbacks {
  bool (*IsSwiftMangledName)(llvm::StringRef name);
  std::pair<std::string, DemangledNameInfo> (*TrackedDemangleSymbolAsString)(
      llvm::StringRef symbol, DemangleMode mode, const SymbolContext *sc);
  std::string (*DemangleSymbolAsString)(llvm::StringRef symbol,
                                        DemangleMode mode,
                                        const SymbolContext *sc);
  bool (*ExtractFunctionBasenameFromMangled)(ConstString mangled,
                                             ConstString &basename,
                                             bool &is_method);
  bool (*IsAnySwiftAsyncFunctionSymbol)(llvm::StringRef name);
  bool (*IsSwiftAsyncAwaitResumePartialFunctionSymbol)(llvm::StringRef name);
};

void SetCallbacks(const Callbacks *callbacks);

bool IsSwiftMangledName(llvm::StringRef name);

std::pair<std::string, DemangledNameInfo>
TrackedDemangleSymbolAsString(llvm::StringRef symbol, DemangleMode mode,
                              const SymbolContext *sc = nullptr);

std::string DemangleSymbolAsString(llvm::StringRef symbol, DemangleMode mode,
                                   const SymbolContext *sc = nullptr);

bool ExtractFunctionBasenameFromMangled(ConstString mangled,
                                        ConstString &basename, bool &is_method);

bool IsAnySwiftAsyncFunctionSymbol(llvm::StringRef name);

bool IsSwiftAsyncAwaitResumePartialFunctionSymbol(llvm::StringRef name);

} // namespace SwiftDemangle
} // namespace lldb_private

#endif // LLDB_ENABLE_SWIFT

#endif // LLDB_CORE_SWIFTDEMANGLE_H
