//===--- SwiftCodeCompletion.cpp - Code Completion for Swift ----*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "SwiftCodeCompletion.h"

#include "swift/AST/DiagnosticSuppression.h"
#include "swift/AST/SourceFile.h"
#include "swift/IDE/CodeCompletion.h"
#include "swift/IDE/CodeCompletionCache.h"
#include "swift/Parse/CodeCompletionCallbacks.h"
#include "swift/Subsystems.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

using namespace llvm;
using namespace swift;
using namespace ide;

static std::string toInsertableString(CodeCompletionResult *Result) {
  using ChunkKind = CodeCompletionString::Chunk::ChunkKind;
  std::string Str;
  auto chunks = Result->getCompletionString()->getChunks();
  for (unsigned i = 0; i < chunks.size(); ++i) {
    auto outerChunk = chunks[i];

    // Consume the whole call parameter, keep track of which piece of the call
    // parameter we are in, and emit only pieces of the call parameter that
    // should be inserted into the code buffer.
    if (outerChunk.is(ChunkKind::CallParameterBegin)) {
      ++i;
      auto callParameterSection = ChunkKind::CallParameterBegin;
      bool hasParameterName = false;
      for (; i < chunks.size(); ++i) {
        auto innerChunk = chunks[i];

        // Exit this loop when we're at the end of the call parameter.
        if (innerChunk.endsPreviousNestedGroup(outerChunk.getNestingLevel())) {
          --i;
          break;
        }

        // Keep track of what part of the call parameter we are in.
        if (innerChunk.is(ChunkKind::CallParameterName) ||
            innerChunk.is(ChunkKind::CallParameterInternalName) ||
            innerChunk.is(ChunkKind::CallParameterColon) ||
            innerChunk.is(ChunkKind::CallParameterType) ||
            innerChunk.is(ChunkKind::CallParameterClosureType))
          callParameterSection = innerChunk.getKind();

        if (callParameterSection == ChunkKind::CallParameterName)
          hasParameterName = true;

        // Never emit these parts of the call parameter.
        if (callParameterSection == ChunkKind::CallParameterInternalName ||
            callParameterSection == ChunkKind::CallParameterType ||
            callParameterSection == ChunkKind::CallParameterClosureType)
          continue;

        // Do not emit a colon when the parameter is unnamed.
        if (!hasParameterName && callParameterSection == ChunkKind::CallParameterColon)
          continue;

        if (innerChunk.hasText() && !innerChunk.isAnnotation())
          Str += innerChunk.getText();
      }
      continue;
    }

    if (outerChunk.hasText() && !outerChunk.isAnnotation())
      Str += outerChunk.getText();
  }
  return Str;
}

static std::string toDisplayString(CodeCompletionResult *Result) {
  std::string Str;
  for (auto C : Result->getCompletionString()->getChunks()) {
    if (C.getKind() ==
        CodeCompletionString::Chunk::ChunkKind::BraceStmtWithCursor) {
      Str += ' ';
      continue;
    }
    if (!C.isAnnotation() && C.hasText()) {
      Str += C.getText();
      continue;
    }
    if (C.getKind() == CodeCompletionString::Chunk::ChunkKind::TypeAnnotation) {
      if (Result->getKind() == CodeCompletionResult::Declaration) {
        switch (Result->getAssociatedDeclKind()) {
        case CodeCompletionDeclKind::Module:
        case CodeCompletionDeclKind::PrecedenceGroup:
        case CodeCompletionDeclKind::Class:
        case CodeCompletionDeclKind::Struct:
        case CodeCompletionDeclKind::Enum:
          continue;

        case CodeCompletionDeclKind::EnumElement:
          Str += ": ";
          break;

        case CodeCompletionDeclKind::Protocol:
        case CodeCompletionDeclKind::TypeAlias:
        case CodeCompletionDeclKind::AssociatedType:
        case CodeCompletionDeclKind::GenericTypeParam:
        case CodeCompletionDeclKind::Constructor:
        case CodeCompletionDeclKind::Destructor:
          continue;

        case CodeCompletionDeclKind::Subscript:
        case CodeCompletionDeclKind::StaticMethod:
        case CodeCompletionDeclKind::InstanceMethod:
        case CodeCompletionDeclKind::PrefixOperatorFunction:
        case CodeCompletionDeclKind::PostfixOperatorFunction:
        case CodeCompletionDeclKind::InfixOperatorFunction:
        case CodeCompletionDeclKind::FreeFunction:
          Str += " -> ";
          break;

        case CodeCompletionDeclKind::StaticVar:
        case CodeCompletionDeclKind::InstanceVar:
        case CodeCompletionDeclKind::LocalVar:
        case CodeCompletionDeclKind::GlobalVar:
          Str += ": ";
          break;
        }
      } else {
        Str += ": ";
      }
      Str += C.getText();
    }
  }
  return Str;
}

namespace lldb_private {

class CodeCompletionConsumer : public SimpleCachingCodeCompletionConsumer {
  CompletionResponse &Response;

public:
  CodeCompletionConsumer(CompletionResponse &Response) : Response(Response) {}

  void handleResults(MutableArrayRef<CodeCompletionResult *> Results) override {
    CodeCompletionContext::sortCompletionResults(Results);
    for (auto *Result : Results) {
      Response.Matches.push_back(
          {toDisplayString(Result), toInsertableString(Result)});
    }
  }
};

/// Returns the offset of the completion prefix.
///
/// The Swift compiler completion functions only complete full
/// identifiers/keyword, so we must strip this prefix off of the entered code
/// before asking for a completion.
///
/// For example:
///
///   EnteredCode = "foo.ba"
///     indices      012345
///   FindCompletionOffset = 4
///
///   EnteredCode = "foo."
///     indices      0123
///   FindCompletionOffset = 4
///
///   EnteredCode = "foo"
///     indices      012
///   FindCompletionOffset = 0
static unsigned FindCompletionPrefixOffset(StringRef EnteredCode) {
  for (int i = EnteredCode.size() - 1; i >= 0; --i) {
    char c = EnteredCode[i];
    if (c != '_' && !std::isalnum(c))
      return i + 1;
  }
  return 0;
}

CompletionResponse
SwiftCompleteCode(SwiftASTContext &SwiftCtx,
                  SwiftPersistentExpressionState &PersistentExpressionState,
                  StringRef EnteredCode) {
  Status error;
  ASTContext &ctx = *SwiftCtx.GetASTContext();

  // Remove the completion prefix from the entered code, and allocate a
  // null-terminated buffer for it.
  unsigned completionOffset = FindCompletionPrefixOffset(EnteredCode);
  std::string completionCodeTerminated =
      EnteredCode.substr(0, completionOffset);
  completionCodeTerminated += '\0';
  const unsigned completionCodeBufferID = ctx.SourceMgr.addMemBufferCopy(
      completionCodeTerminated, "<Completion Input>");

  // Set up an ImplicitImportInfo with a list of modules that have been imported
  // in previous REPL executions.
  swift::ImplicitImportInfo importInfo;
  importInfo.StdlibKind = swift::ImplicitStdlibKind::Stdlib;
  for (const ConstString moduleName :
       PersistentExpressionState.GetHandLoadedModules()) {
    SourceModule moduleInfo;
    moduleInfo.path.push_back(moduleName);
    ModuleDecl *module = SwiftCtx.GetModule(moduleInfo, error);
    if (!module)
      continue;
    importInfo.AdditionalModules.emplace_back(module, /*exported*/ false);
  }

  // Set up the following AST:
  //
  //   completionsModule
  //     completionCodeFile - file containing the code that we're completing
  //     persistentDeclFile - file containing persistent declarations
  //
  // (Persistent declarations are declarations from previous REPL executions).
  auto *completionsModule =
      ModuleDecl::create(ctx.getIdentifier("CodeCompletion"), ctx, importInfo);
  auto &completionCodeFile = *new (ctx) SourceFile(
      *completionsModule, SourceFileKind::REPL, completionCodeBufferID);
  completionsModule->addFile(completionCodeFile);
  auto &persistentDeclFile = *new (ctx) SourceFile(
      *completionsModule, SourceFileKind::Library,
      ctx.SourceMgr.addMemBufferCopy("", "<Persistent Decls>"));
  completionsModule->addFile(persistentDeclFile);
  persistentDeclFile.ASTStage = SourceFile::TypeChecked;
  swift::performImportResolution(persistentDeclFile);

  // Parse `completionCodeFile`. Note that the Swift compiler completion
  // infrastructure requires that we set the code completion point before
  // parsing the file, so we set it now.
  ctx.SourceMgr.setCodeCompletionPoint(completionCodeBufferID,
                                       completionOffset);

  // Set the decls in `persistentDeclFile` to the persistent declarations.
  {
    // We exclude persistent decls that are also present in
    // `completionCodeFile`, so that declarations that have been re-declared in
    // the user's current code take precedence over persistent declarations.
    DenseMap<Identifier, SmallVector<ValueDecl *, 1>> newDecls;
    for (auto *decl : completionCodeFile.getTopLevelDecls())
      if (auto *newValueDecl = dyn_cast<ValueDecl>(decl))
        newDecls.FindAndConstruct(newValueDecl->getBaseName().getIdentifier())
            .second.push_back(newValueDecl);

    auto containedInNewDecls = [&](Decl *oldDecl) -> bool {
      auto *oldValueDecl = dyn_cast<ValueDecl>(oldDecl);
      if (!oldValueDecl)
        return false;
      auto newDeclsLookup =
          newDecls.find(oldValueDecl->getBaseName().getIdentifier());
      if (newDeclsLookup == newDecls.end())
        return false;
      for (auto *newDecl : newDeclsLookup->second)
        if (conflicting(newDecl->getOverloadSignature(),
                        oldValueDecl->getOverloadSignature()))
          return true;
      return false;
    };

    std::vector<Decl *> persistentDecls;
    PersistentExpressionState.GetAllDecls(persistentDecls);
    for (auto *decl : persistentDecls) {
      if (containedInNewDecls(decl))
        continue;
      persistentDeclFile.addTopLevelDecl(decl);
    }
  }

  // Set up `response` to collect results, and set up a callback handler that
  // puts the results into `response`.
  CompletionResponse response;
  CodeCompletionConsumer consumer(response);
  CodeCompletionCache completionCache;
  CodeCompletionContext completionContext(completionCache);
  std::unique_ptr<CodeCompletionCallbacksFactory> completionCallbacksFactory(
      ide::makeCodeCompletionCallbacksFactory(completionContext, consumer));

  // The prefix of the identifer/keyword that is being completed.
  response.Prefix = EnteredCode.substr(completionOffset);

  // Actually ask for completions.
  {
    DiagnosticTransaction diagTxn(ctx.Diags);
    swift::performImportResolution(completionCodeFile);
    swift::bindExtensions(completionCodeFile);
    performCodeCompletionSecondPass(completionCodeFile,
                                    *completionCallbacksFactory);
  }

  // Filter the matches for those matching the prefix.
  std::vector<CompletionMatch> filteredMatches;
  for (auto &Match : response.Matches) {
    if (!StringRef(Match.Insertable).startswith(response.Prefix))
      continue;
    filteredMatches.push_back(
        {Match.Display, Match.Insertable.substr(response.Prefix.size())});
  }
  response.Matches = filteredMatches;

  // Clean up.
  ctx.SourceMgr.clearCodeCompletionPoint();

  return response;
}

} // namespace lldb_private
