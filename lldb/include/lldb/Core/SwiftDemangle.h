//===-- SwiftDemangle.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A leaf Swift symbol *demangling* helper that lives in lldbCore and links
// only against the standalone Swift demangling library (swiftDemangling). It
// intentionally does NOT depend on the heavy Swift language runtime plugin or
// the Swift compiler.
//
// The single heavy operation that demangling can (optionally) perform --
// resolving the dynamic types bound to a function's generic parameters -- is
// routed through an installable function-pointer hook. The Swift language
// runtime plugin installs that hook when it initializes; tools that don't load
// the plugin (e.g. lldb-server) simply fall back to printing the static
// generic parameter name.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_CORE_SWIFTDEMANGLE_H
#define LLDB_CORE_SWIFTDEMANGLE_H

#ifdef LLDB_ENABLE_SWIFT

#include "lldb/Core/DemangledNameInfo.h"

#include "swift/Demangling/ManglingFlavor.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace lldb_private {

class ConstString;
class ExecutionContext;
class SymbolContext;

namespace SwiftDemangle {

/// A pair of {depth, index} describing a generic parameter (archetype).
using ArchetypePath = std::pair<uint64_t, uint64_t>;

/// The signature of the hook that resolves the names of the generic parameters
/// bound in a function's context. This is the one demangling operation that
/// requires the Swift language runtime (and therefore the Swift compiler), so
/// it is provided by the Swift language runtime plugin at initialization time.
using GenericParameterNameResolver =
    void (*)(const SymbolContext &sc, const ExecutionContext *exe_ctx,
             swift::Mangle::ManglingFlavor flavor,
             llvm::DenseMap<ArchetypePath, llvm::StringRef> &dict);

/// Install the hook used to resolve dynamic generic parameter names. Called by
/// the Swift language runtime plugin from its Initialize(). Passing nullptr
/// clears the hook.
void SetGenericParameterNameResolver(GenericParameterNameResolver resolver);

/// The demangling flavor to use when printing.
enum DemangleMode { eSimplified, eTypeName, eDisplayTypeName };

/// Returns true if \p name is a Swift mangled name. Pure swift::Demangle, no
/// runtime required.
bool IsSwiftMangledName(llvm::StringRef name);

/// Demangle \p symbol into a human-readable string. When \p sc is non-null,
/// generic parameters are resolved: if the runtime hook has been installed it
/// is consulted, otherwise the static generic parameter name is used.
///
/// If \p tracking is true, the returned optional carries a DemangledNameInfo
/// describing the ranges of the various parts of the demangled name.
std::pair<std::string, std::optional<DemangledNameInfo>>
DemangleSymbolAsString(llvm::StringRef symbol, DemangleMode mode, bool tracking,
                       const SymbolContext *sc, const ExecutionContext *exe_ctx);

/// Convenience overload returning just the demangled string.
std::string DemangleSymbolAsString(llvm::StringRef symbol, DemangleMode mode,
                                   const SymbolContext *sc = nullptr,
                                   const ExecutionContext *exe_ctx = nullptr);

/// Demangle \p symbol and return both the string and its DemangledNameInfo.
std::pair<std::string, DemangledNameInfo>
TrackedDemangleSymbolAsString(llvm::StringRef symbol, DemangleMode mode,
                              const SymbolContext *sc = nullptr,
                              const ExecutionContext *exe_ctx = nullptr);

/// Extract the function basename out of a mangled Swift name. Pure
/// swift::Demangle, no runtime required.
bool ExtractFunctionBasenameFromMangled(ConstString mangled,
                                        ConstString &basename, bool &is_method);

/// Return true if \p name is a Swift async function, await resume partial
/// function, or suspend resume partial function symbol. Pure swift::Demangle,
/// no runtime required.
bool IsAnySwiftAsyncFunctionSymbol(llvm::StringRef name);

/// Return true if \p name is a Swift async await resume partial function
/// symbol. Pure swift::Demangle, no runtime required.
bool IsSwiftAsyncAwaitResumePartialFunctionSymbol(llvm::StringRef name);

} // namespace SwiftDemangle
} // namespace lldb_private

#endif // LLDB_ENABLE_SWIFT

#endif // LLDB_CORE_SWIFTDEMANGLE_H
