//===-- SwiftDemangle.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/SwiftDemangle.h"

using namespace lldb_private;

static const SwiftDemangle::Callbacks *g_callbacks = nullptr;

void SwiftDemangle::SetCallbacks(const Callbacks *callbacks) {
  g_callbacks = callbacks;
}

bool SwiftDemangle::IsSwiftMangledName(llvm::StringRef name) {
  return g_callbacks && g_callbacks->IsSwiftMangledName(name);
}

std::pair<std::string, DemangledNameInfo>
SwiftDemangle::TrackedDemangleSymbolAsString(llvm::StringRef symbol,
                                             DemangleMode mode,
                                             const SymbolContext *sc) {
  if (!g_callbacks)
    return {};
  return g_callbacks->TrackedDemangleSymbolAsString(symbol, mode, sc);
}

std::string SwiftDemangle::DemangleSymbolAsString(llvm::StringRef symbol,
                                                  DemangleMode mode,
                                                  const SymbolContext *sc) {
  if (!g_callbacks)
    return {};
  return g_callbacks->DemangleSymbolAsString(symbol, mode, sc);
}

bool SwiftDemangle::ExtractFunctionBasenameFromMangled(ConstString mangled,
                                                       ConstString &basename,
                                                       bool &is_method) {
  return g_callbacks && g_callbacks->ExtractFunctionBasenameFromMangled(
                            mangled, basename, is_method);
}

bool SwiftDemangle::IsAnySwiftAsyncFunctionSymbol(llvm::StringRef name) {
  return g_callbacks && g_callbacks->IsAnySwiftAsyncFunctionSymbol(name);
}

bool SwiftDemangle::IsSwiftAsyncAwaitResumePartialFunctionSymbol(
    llvm::StringRef name) {
  return g_callbacks &&
         g_callbacks->IsSwiftAsyncAwaitResumePartialFunctionSymbol(name);
}
