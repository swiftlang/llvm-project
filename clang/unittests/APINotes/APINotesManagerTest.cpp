//===- unittests/APINotes/APINotesManagerTest.cpp - APINotesManager tests -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/APINotes/APINotesManager.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/Module.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/ModuleMap.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace clang;
using namespace clang::api_notes;
using testing::Eq;
using testing::Optional;
using testing::StrEq;

namespace {

class APINotesManagerTest : public ::testing::Test {
protected:
  APINotesManagerTest()
      : FS(llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>()),
        FileMgr(FileMgrOpts, FS),
        Diags(DiagnosticIDs::create(), DiagOpts, new IgnoringDiagConsumer()),
        SrcMgr(Diags, FileMgr),
        HdrSearch(HdrSearchOpts, SrcMgr, Diags, LangOpts, /*Target=*/nullptr),
        Map(SrcMgr, Diags, LangOpts, /*Target=*/nullptr, HdrSearch),
        NotesManager(SrcMgr, LangOpts) {}

  Module *createModule(StringRef Name, StringRef Dir) {
    auto *M = Map.createModule(Name, /*Parent=*/nullptr,
                               /*IsFramework=*/false, /*IsExplicit=*/false);
    FS->addFile((Dir + "/module.modulemap").str(), 0,
                llvm::MemoryBuffer::getMemBuffer(""));
    M->Directory = FileMgr.getOptionalDirectoryRef(Dir);
    EXPECT_TRUE(M->Directory);
    return M;
  }

  void addAPINotesFile(StringRef Dir, StringRef Name, StringRef Global,
                       StringRef Type) {
    FS->addFile((Dir + "/" + Name + ".apinotes").str(), 0,
                llvm::MemoryBuffer::getMemBufferCopy(
                    getAPINotesYAML(Name, Global, Type)));
  }

  static std::string getAPINotesYAML(StringRef Name, StringRef Global,
                                     StringRef Type) {
    return ("Name: " + Name + "\nGlobals:\n  - Name: " + Global +
            "\n    Type: " + Type + "\n")
        .str();
  }

  std::optional<std::string> lookupType(const Module *M, StringRef Global) {
    auto Info = NotesManager.lookupGlobalVariable(M, Global, SourceLocation());
    if (!Info)
      return std::nullopt;
    return Info->getType();
  }

  IntrusiveRefCntPtr<llvm::vfs::InMemoryFileSystem> FS;
  FileSystemOptions FileMgrOpts;
  FileManager FileMgr;
  DiagnosticOptions DiagOpts;
  DiagnosticsEngine Diags;
  SourceManager SrcMgr;
  LangOptions LangOpts;
  HeaderSearchOptions HdrSearchOpts;
  HeaderSearch HdrSearch;
  ModuleMap Map;
  APINotesManager NotesManager;
};

TEST_F(APINotesManagerTest, LoadAPINotesFromFile) {
  auto *A = createModule("A", "/include/A");
  addAPINotesFile("/include/A", "A", "MACRO", "unsigned");

  EXPECT_THAT(lookupType(A, "MACRO"), Optional(StrEq("unsigned")));
  EXPECT_THAT(lookupType(A, "OTHER"), Eq(std::nullopt));
}

TEST_F(APINotesManagerTest, LoadAPINotesFromImportedModule) {
  auto *A = createModule("A", "/include/A");
  auto *Sub = Map.createModule("Sub", A, /*IsFramework=*/false,
                               /*IsExplicit=*/false);
  addAPINotesFile("/include/A", "A", "MACRO", "from_file");

  auto Notes = getAPINotesYAML("A", "MACRO", "from_loader");
  auto Calls = 0u;
  NotesManager.setImportedModuleAPINotesLoader(
      [&](const Module &M) -> std::optional<llvm::SmallVector<StringRef, 2>> {
        ++Calls;
        EXPECT_EQ(&M, A);
        return llvm::SmallVector<StringRef, 2>{Notes};
      });

  EXPECT_THAT(lookupType(A, "MACRO"), Optional(StrEq("from_loader")));
  EXPECT_THAT(lookupType(Sub, "MACRO"), Optional(StrEq("from_loader")));
  EXPECT_THAT(lookupType(A, "OTHER"), Eq(std::nullopt));
  EXPECT_EQ(Calls, 1u);
}

TEST_F(APINotesManagerTest, LoadNothingFromImportedModule) {
  auto *A = createModule("A", "/include/A");
  addAPINotesFile("/include/A", "A", "MACRO", "from_file");

  NotesManager.setImportedModuleAPINotesLoader(
      [](const Module &) -> std::optional<llvm::SmallVector<StringRef, 2>> {
        return llvm::SmallVector<StringRef, 2>{};
      });

  EXPECT_THAT(lookupType(A, "MACRO"), Eq(std::nullopt));
}

TEST_F(APINotesManagerTest, LoadAPINotesFromFileOrImportedModule) {
  auto *A = createModule("A", "/include/A");
  auto *B = createModule("B", "/include/B");
  addAPINotesFile("/include/A", "A", "MACRO", "from_file_a");
  addAPINotesFile("/include/B", "B", "MACRO", "from_file_b");

  auto Notes = getAPINotesYAML("A", "MACRO", "from_loader");
  NotesManager.setImportedModuleAPINotesLoader(
      [&](const Module &M) -> std::optional<llvm::SmallVector<StringRef, 2>> {
        if (&M == A)
          return llvm::SmallVector<StringRef, 2>{Notes};
        return std::nullopt;
      });

  EXPECT_THAT(lookupType(A, "MACRO"), Optional(StrEq("from_loader")));
  EXPECT_THAT(lookupType(B, "MACRO"), Optional(StrEq("from_file_b")));
}

} // namespace
