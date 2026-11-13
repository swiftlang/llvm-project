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

  auto swift_mm = std::make_unique<swift::ExplicitSwiftModuleMap>();
  auto cas_swift_mm = std::make_unique<swift::ExplicitSwiftModuleMap>();
  auto clang_mm = std::make_unique<swift::ExplicitClangModuleMap>();
  auto cas_clang_mm = std::make_unique<swift::ExplicitClangModuleMap>();

  auto addSwiftEntry = [&](const auto &entry) {
    if (entry.getValue().moduleCacheKey)
      cas_swift_mm->insert({entry.getKey(), entry.getValue()});
    else
      swift_mm->insert({entry.getKey(), entry.getValue()});
  };
  auto addClangEntry = [&](const auto &entry) {
    if (entry.getValue().moduleCacheKey)
      cas_clang_mm->insert({entry.getKey(), entry.getValue()});
    else
      clang_mm->insert({entry.getKey(), entry.getValue()});
  };

  llvm::for_each(*MainSwiftModuleMap, addSwiftEntry);
  llvm::for_each(*ExplicitSwiftModuleMap, addSwiftEntry);
  llvm::for_each(*ExplicitClangModuleMap, addClangEntry);

  std::unique_ptr<swift::ExplicitCASModuleLoader> casml;
  if ((cas_swift_mm->size() || cas_clang_mm->size()) && cas && action_cache) {
    casml = swift::ExplicitCASModuleLoader::create(
        ctx, *cas, *action_cache, tracker, loadMode, ExplicitSwiftModuleMapPath,
        ExplicitSwiftModuleInputs, IgnoreSwiftSourceInfoFile,
        std::move(cas_swift_mm), std::move(cas_clang_mm));
  }
  auto esml = swift::ExplicitSwiftModuleLoader::create(
      ctx, tracker, loadMode, ExplicitSwiftModuleMapPath,
      ExplicitSwiftModuleInputs, IgnoreSwiftSourceInfoFile,
      std::move(swift_mm), std::move(clang_mm));
  auto loader = std::make_unique<LLDBExplicitSwiftModuleLoader>(
      ctx, cas, action_cache, tracker, loadMode, IgnoreSwiftSourceInfoFile,
      std::move(casml), std::move(esml));
  if (!MainSwiftModuleMap->empty())
    loader->m_main_module = MainSwiftModuleMap->begin()->getKey().str();
  return loader;
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
    ModuleVersionInfo *versionInfo, bool isTestableImport,
    bool isSourceCanImport) {
  if (!enabled())
    return false;

  if (m_casml &&
      llvm::cast<swift::SerializedModuleLoaderBase>(m_casml.get())
          ->canImportModule(named, loc, versionInfo, isTestableImport,
                            isSourceCanImport))
    return true;
  return llvm::cast<swift::SerializedModuleLoaderBase>(m_esml.get())
      ->canImportModule(named, loc, versionInfo, isTestableImport,
                        isSourceCanImport);
}

swift::ModuleDecl *
LLDBExplicitSwiftModuleLoader::loadModule(swift::SourceLoc importLoc,
                                          swift::ImportPath::Module path,
                                          bool AllowMemoryCache) {
  if (!enabled())
    return nullptr;

  swift::ModuleDecl *decl = nullptr;
  if (m_casml)
    decl = m_casml->loadModule(importLoc, path, AllowMemoryCache);
  if (!decl)
    decl = m_esml->loadModule(importLoc, path, AllowMemoryCache);
  // Simulate the effect of the BypassResilience flag in the
  // MemoryBufferSerializedModuleLoader. The main module was compiled
  // together with the code being debugged, so its layout can be
  // trusted and the debugger can access private members directly
  // rather than through dispatch thunks.
  if (decl && decl->getName().str() == m_main_module &&
      !decl->isBuiltFromInterface())
    decl->setBypassResilience();
  return decl;
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
