//===- PTHManager.h - Manager object for PTH processing ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the PTHManager interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEX_PTHMANAGER_H
#define LLVM_CLANG_LEX_PTHMANAGER_H

#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CAS/CASFileSystem.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/OnDiskHashTable.h"
#include <memory>

namespace llvm {

class MemoryBuffer;

} // namespace llvm

namespace clang {

class DiagnosticsEngine;
class FileSystemStatCache;
class Preprocessor;
class PTHLexer;
class PTHManager;

class PTHHandler {
  friend class PTHLexer;

public:
  std::unique_ptr<PTHLexer> createLexer(PTHManager &PTHM, FileID FID);

  PTHHandler() = delete;
  struct NullHandlerTag {};

  PTHHandler(NullHandlerTag) {}

  explicit PTHHandler(PTHManager &PTHM, DiagnosticsEngine &Diags,
                      StringRef File, StringRef PTH);

private:
  void initialize(PTHManager &PTHM, DiagnosticsEngine &Diags, StringRef File);

  Optional<StringRef> PTH;

  MutableArrayRef<IdentifierInfo *> IdentifierInfoCache;

  /// Array representing the mapping from persistent IDs to the data offset
  /// within the PTH file containing the identifier string.
  const unsigned char *IdentifierTable = nullptr;

  /// The base offset within the PTH memory buffer that contains the cached
  /// strings for literals and raw identifiers.
  const unsigned char *StringsTable = nullptr;

  /// The base offset for the preprocessor conditionals.
  const unsigned char *PPCondTable = nullptr;

  /// Base offset for the token stream.
  const unsigned char *TokenStream = nullptr;
};

/// FIXME: Main work is figuring out what to do with IdentiferInfoLookup.
///
/// - New design has 1 PTH file per input file (including headers).
/// - PTHManager originally designed to just have one PTH for all files.
/// - Identifier tables need to be merged somehow... maybe.
///   - Or maybe we can get away with dropping the info.
/// - IdentifierTable itself has a top-level cache of IdentifierInfo.
///   - First time a token is needed, it will call "get".
/// - Long-term, we might want identifier tables merged in an "on-disk" (in the
///   CAS) format.
class PTHManager {
  friend class PTHHandler;

  llvm::DenseMap<const FileEntry *, PTHHandler *> Handlers;

  PTHHandler NullHandler = PTHHandler::NullHandlerTag();
  llvm::SpecificBumpPtrAllocator<PTHHandler> HandlerAlloc;

  /// Allocator for pointers to IdentifierInfo caches.
  llvm::SpecificBumpPtrAllocator<IdentifierInfo *> IdentifierInfoCacheAlloc;

  llvm::cas::CASDB &CAS;
  IntrusiveRefCntPtr<llvm::cas::CASFileSystemBase> FS;
  LangOptions CanonicalLangOpts;
  Optional<llvm::cas::CASID> SerializedLangOpts;
  Optional<llvm::cas::CASID> ClangVersion;
  Optional<llvm::cas::CASID> Operation;

  /// PP - The Preprocessor object that will use this PTHManager to create
  ///  PTHLexer objects.
  Preprocessor *PP = nullptr;

  PTHHandler *createHandler(StringRef Filename, llvm::cas::CASID PTH);
  Expected<llvm::cas::CASID> computePTH(llvm::cas::CASID InputFile);

public:
  // The current PTH version.
  enum { Version = 11 };

  PTHManager(const PTHManager &) = delete;
  PTHManager &operator=(const PTHManager &) = delete;
  ~PTHManager();
  PTHManager() = delete;

  PTHManager(IntrusiveRefCntPtr<llvm::cas::CASFileSystemBase> FS,
             Preprocessor &PP);

  void setPreprocessor(Preprocessor *pp) { PP = pp; }

  /// createLexer - Return a PTHLexer that "lexes" the cached tokens for the
  ///  specified file.  This method returns nullptr if no cached tokens exist.
  ///  It is the responsibility of the caller to 'delete' the returned object.
  std::unique_ptr<PTHLexer> createLexer(FileID FID);
};

} // namespace clang

#endif // LLVM_CLANG_LEX_PTHMANAGER_H
