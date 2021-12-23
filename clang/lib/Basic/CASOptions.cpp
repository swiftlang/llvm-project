//===- CASOptions.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/CASOptions.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticCAS.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/Support/Error.h"

using namespace clang;

std::shared_ptr<llvm::cas::CASDB>
CASOptions::getOrCreateCAS(DiagnosticsEngine &Diags) const {
  if (std::shared_ptr<llvm::cas::CASDB> CAS = CachedCAS)
    return CAS;

  /// Ensure all threads see the same value on a race.
  auto getOrSet = [&](std::shared_ptr<llvm::cas::CASDB> RHS) {
    std::shared_ptr<llvm::cas::CASDB> Existing;
    return CachedCAS.compare_exchange_strong(Existing, RHS) ? RHS : Existing;
  };

  switch (Kind) {
  case CASOptions::BuiltinCAS: {
    if (BuiltinPath.empty())
      break;
    Expected<std::unique_ptr<llvm::cas::CASDB>> CAS =
        llvm::cas::createOnDiskCAS(BuiltinPath);
    if (CAS)
      return getOrSet(std::move(*CAS));
    Diags.Report(diag::err_cannot_initialize_builtin_cas)
        << toString(CAS.takeError());
    break; // Fallback: in-memory.
  }
  case CASOptions::PluginCAS: {
    // FIXME: Pass on the actual error text from the CAS.
    Expected<std::unique_ptr<llvm::cas::CASDB>> CAS =
        llvm::cas::createPluginCAS(PluginPath, PluginArgs);
    if (CAS)
      return getOrSet(std::move(*CAS));

    // FIXME: Add notes with the plugin arguments?
    Diags.Report(diag::err_cas_plugin_cannot_be_loaded) << PluginPath;
    break; // Fallback: in-memory.
  }
  }

  return getOrSet(llvm::cas::createInMemoryCAS());
}

std::shared_ptr<llvm::cas::ActionCache>
CASOptions::getOrCreateActionCache(const llvm::cas::Namespace &NS,
                                   DiagnosticsEngine &Diags) const {
  if (std::shared_ptr<llvm::cas::ActionCache> ActionCache = CachedActionCache)
    return ActionCache;

  std::shared_ptr<llvm::cas::ActionCache> NewActionCache;
  if (!ActionCachePath.empty()) {
    if (llvm::Error E = llvm::cas::createOnDiskActionCache(NS, ActionCachePath)
                            .moveInto(NewActionCache)) {
      Diags.Report(diag::err_cannot_initialize_action_cache)
          << toString(std::move(E));
    }
  }
  if (!NewActionCache)
    NewActionCache = llvm::cas::createInMemoryActionCache(NS);

  /// Ensure all threads see the same value on a race.
  std::shared_ptr<llvm::cas::ActionCache> Existing;
  return CachedActionCache.compare_exchange_strong(Existing, NewActionCache)
             ? NewActionCache
             : Existing;
}
