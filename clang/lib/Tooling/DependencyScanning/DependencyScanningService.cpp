//===- DependencyScanningService.cpp - clang-scan-deps service ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Tooling/DependencyScanning/DependencyScanningService.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang;
using namespace tooling;
using namespace dependencies;

DependencyScanningService::DependencyScanningService(
    ScanningMode Mode, ScanningOutputFormat Format,
    IntrusiveRefCntPtr<llvm::cas::CachingOnDiskFileSystem> SharedFS,
    llvm::cas::ActionCache &Cache, bool ReuseFileManager,
    bool SkipExcludedPPRanges, bool OverrideCASTokenCache)
    : Mode(Mode), Format(Format), ReuseFileManager(ReuseFileManager),
      SkipExcludedPPRanges(SkipExcludedPPRanges),
      OverrideCASTokenCache(OverrideCASTokenCache),
      SharedFS(std::move(SharedFS)), Cache(Cache) {
  if (!this->SharedFS)
    this->SharedFS = llvm::cantFail(llvm::cas::createCachingOnDiskFileSystem(
        llvm::cas::createInMemoryCAS()));

  // Initialize targets for object file support.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
}

DependencyScanningService::~DependencyScanningService() = default;
