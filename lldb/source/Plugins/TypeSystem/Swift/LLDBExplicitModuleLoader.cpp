//===--LLDBExplicitModuleLoader.cpp --------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===---------------------------------------------------------------------===//

#include "LLDBExplicitModuleLoader.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"

#include "swift/Serialization/SerializedModuleLoader.h"
#include <swift/Frontend/ModuleInterfaceLoader.h>

namespace lldb_private {

LLDBExplicitSwiftModuleLoader::LLDBExplicitSwiftModuleLoader(
    swift::ASTContext &ctx, std::shared_ptr<llvm::cas::ObjectStore> cas,
    std::shared_ptr<llvm::cas::ActionCache> action_cache,
    swift::DependencyTracker *tracker, swift::ModuleLoadingMode loadMode,
    bool IgnoreSwiftSourceInfoFile,
    std::unique_ptr<swift::ExplicitCASModuleLoader> casml,
    std::unique_ptr<swift::ExplicitSwiftModuleLoader> esml)
    : swift::SerializedModuleLoaderBase(ctx, tracker, loadMode,
                                        IgnoreSwiftSourceInfoFile),
      m_cas(cas), m_action_cache(action_cache), m_casml(std::move(casml)),
      m_esml(std::move(esml)) {}

template <typename Map, typename Entry>
void sortEntry(Map &cas_map, Map &esml_map, const Entry &entry) {
  if (entry.getValue().moduleCacheKey)
    cas_map.insert({entry.getKey(), entry.getValue()});
  else
    esml_map.insert({entry.getKey(), entry.getValue()});
}

std::unique_ptr<LLDBExplicitSwiftModuleLoader>
LLDBExplicitSwiftModuleLoader::create(
    swift::ASTContext &ctx, std::shared_ptr<llvm::cas::ObjectStore> cas,
    std::shared_ptr<llvm::cas::ActionCache> action_cache,
    swift::DependencyTracker *tracker, swift::ModuleLoadingMode loadMode,
    llvm::StringRef ExplicitSwiftModuleMapPath,
    const llvm::StringMap<std::string> &ExplicitSwiftModuleInputs,
    bool IgnoreSwiftSourceInfoFile,
    std::unique_ptr<swift::ExplicitSwiftModuleMap> MainSwiftModuleMap,
    std::unique_ptr<swift::ExplicitSwiftModuleMap> ExplicitSwiftModuleMap,
    std::unique_ptr<swift::ExplicitClangModuleMap> ExplicitClangModuleMap) {
  if (!MainSwiftModuleMap || !ExplicitSwiftModuleMap || !ExplicitClangModuleMap)
    return {};

  std::unique_ptr<swift::ExplicitSwiftModuleMap> cas_swift_map, esml_swift_map;
  std::unique_ptr<swift::ExplicitClangModuleMap> cas_clang_map, esml_clang_map;

  // Filter the map for the approriate module loader. In practice, an
  // explicit module map should be either all CAS or all paths, so it
  // may be possible to optimize this into a check of the first
  // element and and assigment.
  cas_swift_map = std::make_unique<swift::ExplicitSwiftModuleMap>();
  esml_swift_map = std::make_unique<swift::ExplicitSwiftModuleMap>();
  cas_clang_map = std::make_unique<swift::ExplicitClangModuleMap>();
  esml_clang_map = std::make_unique<swift::ExplicitClangModuleMap>();

  for (auto &entry : *MainSwiftModuleMap)
    sortEntry(*cas_swift_map, *esml_swift_map, entry);

  for (auto &entry : *ExplicitSwiftModuleMap)
    sortEntry(*cas_swift_map, *esml_swift_map, entry);

  for (auto &entry : *ExplicitClangModuleMap)
    sortEntry(*cas_clang_map, *esml_clang_map, entry);

  std::unique_ptr<swift::ExplicitCASModuleLoader> casml;
  if (cas && action_cache) {
    casml = swift::ExplicitCASModuleLoader::create(
        ctx, *cas, *action_cache, tracker, loadMode, ExplicitSwiftModuleMapPath,
        ExplicitSwiftModuleInputs, IgnoreSwiftSourceInfoFile,
        std::move(cas_swift_map), std::move(cas_clang_map));
  } else if (cas_swift_map->size() || cas_clang_map->size()) {
    LLDB_LOG(
        GetLog(LLDBLog::Types),
        "explicit Swift module map contains cache keys, but no CAS available");
  }
  auto esml = swift::ExplicitSwiftModuleLoader::create(
      ctx, tracker, loadMode, ExplicitSwiftModuleMapPath,
      ExplicitSwiftModuleInputs, IgnoreSwiftSourceInfoFile,
      std::move(esml_swift_map), std::move(esml_clang_map));
  return std::make_unique<LLDBExplicitSwiftModuleLoader>(
      ctx, cas, action_cache, tracker, loadMode, IgnoreSwiftSourceInfoFile,
      std::move(casml), std::move(esml));
}

bool LLDBExplicitSwiftModuleLoader::enabled() const {
  return Target::GetGlobalProperties().GetSwiftAllowExplicitModules();
}

void LLDBExplicitSwiftModuleLoader::collectVisibleTopLevelModuleNames(
    llvm::SmallVectorImpl<swift::Identifier> &names) const {
  if (m_casml)
    m_casml->collectVisibleTopLevelModuleNames(names);
  m_esml->collectVisibleTopLevelModuleNames(names);
}

std::error_code LLDBExplicitSwiftModuleLoader::findModuleFilesInDirectory(
    swift::ImportPath::Element ModuleID,
    const swift::SerializedModuleBaseName &BaseName,
    llvm::SmallVectorImpl<char> *ModuleInterfacePath,
    llvm::SmallVectorImpl<char> *ModuleInterfaceSourcePath,
    std::unique_ptr<llvm::MemoryBuffer> *ModuleBuffer,
    std::unique_ptr<llvm::MemoryBuffer> *ModuleDocBuffer,
    std::unique_ptr<llvm::MemoryBuffer> *ModuleSourceInfoBuffer,
    bool IsCanImportLookup, bool IsFramework, bool IsTestableDependencyLookup) {
  // This is a protected member and probably also not useful.
  return {};
}
bool LLDBExplicitSwiftModuleLoader::canImportModule(
    swift::ImportPath::Module named, swift::SourceLoc loc,
    ModuleVersionInfo *versionInfo, bool isTestableImport) {
  if (!enabled())
    return false;

  if (m_casml &&
      llvm::cast<swift::SerializedModuleLoaderBase>(m_casml.get())
          ->canImportModule(named, loc, versionInfo, isTestableImport))
    return true;
  return llvm::cast<swift::SerializedModuleLoaderBase>(m_esml.get())
      ->canImportModule(named, loc, versionInfo, isTestableImport);
}

swift::ModuleDecl *
LLDBExplicitSwiftModuleLoader::loadModule(swift::SourceLoc importLoc,
                                          swift::ImportPath::Module path,
                                          bool AllowMemoryCache) {
  if (!enabled())
    return nullptr;

  if (m_casml)
    if (swift::ModuleDecl *decl =
            m_casml->loadModule(importLoc, path, AllowMemoryCache))
      return decl;
  return m_esml->loadModule(importLoc, path, AllowMemoryCache);
}

void LLDBExplicitSwiftModuleLoader::loadExtensions(
    swift::NominalTypeDecl *nominal, unsigned previousGeneration) {
  if (!enabled())
    return;

  if (m_casml)
    m_casml->loadExtensions(nominal, previousGeneration);
  m_esml->loadExtensions(nominal, previousGeneration);
}

void LLDBExplicitSwiftModuleLoader::loadObjCMethods(
    swift::NominalTypeDecl *typeDecl, swift::ObjCSelector selector,
    bool isInstanceMethod, unsigned previousGeneration,
    llvm::TinyPtrVector<swift::AbstractFunctionDecl *> &methods) {
  if (!enabled())
    return;

  if (m_casml)
    m_casml->loadObjCMethods(typeDecl, selector, isInstanceMethod,
                             previousGeneration, methods);
  m_esml->loadObjCMethods(typeDecl, selector, isInstanceMethod,
                          previousGeneration, methods);
}

void LLDBExplicitSwiftModuleLoader::loadDerivativeFunctionConfigurations(
    swift::AbstractFunctionDecl *originalAFD, unsigned previousGeneration,
    llvm::SetVector<swift::AutoDiffConfig> &results) {
  if (!enabled())
    return;

  if (m_casml)
    m_casml->loadDerivativeFunctionConfigurations(originalAFD,
                                                  previousGeneration, results);
  m_esml->loadDerivativeFunctionConfigurations(originalAFD, previousGeneration,
                                               results);
}

void LLDBExplicitSwiftModuleLoader::verifyAllModules() {
  if (!enabled())
    return;

  if (m_casml)
    m_casml->verifyAllModules();
  m_esml->verifyAllModules();
}

swift::ExplicitSwiftModuleMap *
LLDBExplicitSwiftModuleLoader::getExplicitSwiftModuleMap() {
  if (!enabled())
    return nullptr;

  if (m_casml)
    return m_casml->getExplicitSwiftModuleMap();
  return m_esml->getExplicitSwiftModuleMap();
}

swift::ExplicitClangModuleMap *
LLDBExplicitSwiftModuleLoader::getExplicitClangModuleMap() {
  if (!enabled())
    return nullptr;

  if (m_casml)
    return m_casml->getExplicitClangModuleMap();
  return m_esml->getExplicitClangModuleMap();
}

} // namespace lldb_private
