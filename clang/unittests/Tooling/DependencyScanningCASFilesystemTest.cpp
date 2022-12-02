//===- unittest/Tooling/DependencyScanningCASFilesystemTest.cpp -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Tooling/DependencyScanning/DependencyScanningCASFilesystem.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Testing/CAS/CASHelpers.h"
#include "llvm/Testing/Support/SupportHelpers.h"
#include "gtest/gtest.h"

using namespace clang;
using namespace clang::cas;
using namespace clang::cas;
using namespace clang::tooling::dependencies;
using llvm::cas::unittest::ErroringCAS;
using llvm::unittest::TempDir;
using llvm::unittest::TempFile;
using llvm::unittest::TempLink;

TEST(DependencyScanningCASFilesystem, FilenameSpelling) {
  TempDir TestDir("DependencyScanningCASFilesystemTest", /*Unique=*/true);
  TempFile TestFile(TestDir.path("File.h"), "", "#define FOO\n");
  TempLink TestLink("File.h", TestDir.path("SymFile.h"));

  std::unique_ptr<ObjectStore> CAS = llvm::cas::createInMemoryCAS();
  std::unique_ptr<ActionCache> Cache = llvm::cas::createInMemoryActionCache();
  auto CacheFS = llvm::cantFail(llvm::cas::createCachingOnDiskFileSystem(*CAS));
  DependencyScanningCASFilesystem FS(CacheFS, *Cache);

  DiagnosticConsumer CountingConsumer;
  DiagnosticsEngine Diags(new DiagnosticIDs{}, new DiagnosticOptions{},
                          &CountingConsumer, /*ShouldOwnClient=*/false);
  FS.Diagnostics = &Diags;

  EXPECT_EQ(FS.status(TestFile.path()).getError(), std::error_code());
  auto Directives = FS.getDirectiveTokens(TestFile.path());
  ASSERT_TRUE(Directives);
  EXPECT_EQ(Directives->size(), 2u);
  auto DirectivesDots = FS.getDirectiveTokens(TestDir.path("././File.h"));
  ASSERT_TRUE(DirectivesDots);
  EXPECT_EQ(DirectivesDots->size(), 2u);
  auto DirectivesSymlink = FS.getDirectiveTokens(TestLink.path());
  ASSERT_TRUE(DirectivesSymlink);
  EXPECT_EQ(DirectivesSymlink->size(), 2u);
  EXPECT_EQ(CountingConsumer.getNumErrors(), 0u);
  EXPECT_EQ(CountingConsumer.getNumWarnings(), 0u);
}

TEST(DependencyScanningCASFilesystem, CASErrors) {
  TempDir TestDir("DependencyScanningCASFilesystemTest", /*Unique=*/true);
  TempFile TestFile(TestDir.path("File.h"), "", "#define FOO\n");
  TempLink TestLink("File.h", TestDir.path("SymFile.h"));

  // Trigger CAS errors after 0, 1, ... operations and ensure there are no
  // crashes and that errors are reported.
  unsigned AllowCASOps = 0, MaxCASOps = 100;
  unsigned DirectiveFailures = 0;
  for (; AllowCASOps < MaxCASOps; ++AllowCASOps) {
    std::unique_ptr<ErroringCAS> CAS =
        std::make_unique<ErroringCAS>(llvm::cas::createInMemoryCAS());
    std::unique_ptr<ActionCache> Cache = llvm::cas::createInMemoryActionCache();
    auto CacheFS =
        llvm::cantFail(llvm::cas::createCachingOnDiskFileSystem(*CAS));
    DependencyScanningCASFilesystem FS(CacheFS, *Cache);
    DiagnosticConsumer CountingConsumer;
    DiagnosticsEngine Diags(new DiagnosticIDs{}, new DiagnosticOptions{},
                            &CountingConsumer, /*ShouldOwnClient=*/false);
    FS.Diagnostics = &Diags;

    // FIXME: There's a crash here to fix; for now prewarm the cache.
    // EXPECT_EQ(CacheFS->status(TestFile.path()).getError(),
    // std::error_code());

    // Allow I CAS operations before error.
    CAS->setErrorCounter(AllowCASOps);

    // Trigger scan.
    if (FS.status(TestFile.path()).getError()) {
      // Failed before scan; CachingOnDiskFileSystem reports no diagnostics.
      EXPECT_EQ(CountingConsumer.getNumErrors(), 0u);
      EXPECT_EQ(CountingConsumer.getNumWarnings(), 0u);
      continue;
    }

    if (auto Directives = FS.getDirectiveTokens(TestFile.path())) {
      // If we have directives, there should have been no CAS errors.
      EXPECT_EQ(CountingConsumer.getNumErrors(), 0u);
      EXPECT_EQ(CountingConsumer.getNumWarnings(), 0u);
      EXPECT_EQ(Directives->size(), 2u);
      // Once we finish successfully, we're done.
      break;
    }

    // CAS error should be captured as diagnostic.
    EXPECT_EQ(CountingConsumer.getNumErrors(), 1u);
    EXPECT_EQ(CountingConsumer.getNumWarnings(), 0u);
    DirectiveFailures += 1;
  }

  EXPECT_GT(DirectiveFailures, 5u) << "scanning requires more CAS ops";
  EXPECT_LT(AllowCASOps, MaxCASOps) << "scanning never succeeded";
}
