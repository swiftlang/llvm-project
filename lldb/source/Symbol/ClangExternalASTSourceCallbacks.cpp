//===-- ClangExternalASTSourceCallbacks.cpp ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Symbol/ClangExternalASTSourceCallbacks.h"
#include "lldb/Symbol/TypeSystemClang.h"

#include "clang/AST/Decl.h"

using namespace lldb_private;

char ClangExternalASTSourceCallbacks::ID;

void ClangExternalASTSourceCallbacks::CompleteType(clang::TagDecl *tag_decl) {
  m_ast.CompleteTagDecl(tag_decl);
}

void ClangExternalASTSourceCallbacks::CompleteType(
    clang::ObjCInterfaceDecl *objc_decl) {
  m_ast.CompleteObjCInterfaceDecl(objc_decl);
}

bool ClangExternalASTSourceCallbacks::layoutRecordType(
    const clang::RecordDecl *Record, uint64_t &Size, uint64_t &Alignment,
    llvm::DenseMap<const clang::FieldDecl *, uint64_t> &FieldOffsets,
    llvm::DenseMap<const clang::CXXRecordDecl *, clang::CharUnits> &BaseOffsets,
    llvm::DenseMap<const clang::CXXRecordDecl *, clang::CharUnits>
        &VirtualBaseOffsets) {
  return m_ast.LayoutRecordType(Record, Size, Alignment, FieldOffsets,
                                BaseOffsets, VirtualBaseOffsets);
}

void ClangExternalASTSourceCallbacks::FindExternalLexicalDecls(
    const clang::DeclContext *decl_ctx,
    llvm::function_ref<bool(clang::Decl::Kind)> IsKindWeWant,
    llvm::SmallVectorImpl<clang::Decl *> &decls) {
  if (decl_ctx) {
    clang::TagDecl *tag_decl = llvm::dyn_cast<clang::TagDecl>(
        const_cast<clang::DeclContext *>(decl_ctx));
    if (tag_decl)
      CompleteType(tag_decl);
  }
}

unsigned ClangExternalASTSourceCallbacks::takeOwnership(clang::Module *module) {
  m_modules.emplace_back(std::unique_ptr<clang::Module>(module));
  unsigned id = m_modules.size();
  m_ids.insert({module, id});
  return id;
}

llvm::Optional<clang::ExternalASTSource::ASTSourceDescriptor>
ClangExternalASTSourceCallbacks::getSourceDescriptor(unsigned id) {
  if (id && id <= m_modules.size())
    return {*m_modules[id - 1]};
  return {};
}

unsigned ClangExternalASTSourceCallbacks::getIDForModule(clang::Module *module) {
  return { m_ids[module] };
}
