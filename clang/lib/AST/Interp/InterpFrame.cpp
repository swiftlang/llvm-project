//===--- InterpFrame.cpp - Call Frame implementation for the VM -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InterpFrame.h"
#include "Function.h"
#include "InterpStack.h"
#include "InterpState.h"
#include "PrimType.h"
#include "Program.h"
#include "clang/AST/DeclCXX.h"

using namespace clang;
using namespace clang::interp;

InterpFrame::InterpFrame(InterpState &S, Function *Func, InterpFrame *Caller,
                         CodePtr RetPC, Pointer &&This)
    : Caller(Caller), S(S), Func(Func), This(std::move(This)), RetPC(RetPC),
      ArgSize(Func ? Func->getArgSize() : 0),
      Args(static_cast<char *>(S.Stk.top())), FrameOffset(S.Stk.size()) {
  if (Func) {
    assert(Func->getSize() != 0 && "empty function");
    if (unsigned FrameSize = Func->getFrameSize()) {
      Locals = std::make_unique<char[]>(FrameSize);
      for (auto &Scope : Func->scopes()) {
        for (auto &Local : Scope.locals()) {
          Block *B = new (localBlock(Local.Offset)) Block(Local.Desc);
          B->invokeCtor();
        }
      }
    }
  }
}

InterpFrame::~InterpFrame() {
  for (auto &Param : Params)
    S.deallocate(reinterpret_cast<Block *>(Param.second.get()));
}

void InterpFrame::construct() {
  assert(Func && Func->isConstructor() && "not a constructor");

  if (auto *ThisBlock = This.asBlock()) {
    if (ThisBlock->isBaseClass()) {
      // Base classes must set their initialised flag.
      ThisBlock->initialize();
    } else if (!ThisBlock->isRoot() && !ThisBlock->inArray()) {
      // Records in unions must be activated.
      if (auto *R = ThisBlock->getBase().getRecord()) {
        if (R->isUnion())
          ThisBlock->activate();
      }
    }
  } else if (!S.checkingPotentialConstantExpression()) {
    llvm_unreachable("invalid pointer");
  }
}

void InterpFrame::destroy(unsigned Idx) {
  for (auto &Local : Func->getScope(Idx).locals()) {
    S.deallocate(reinterpret_cast<Block *>(localBlock(Local.Offset)));
  }
}

void InterpFrame::popArgs() {
  for (PrimType Ty : Func->args_reverse())
    TYPE_SWITCH(Ty, S.Stk.discard<T>());
}

template <typename T>
static void print(llvm::raw_ostream &OS, const T &V, ASTContext &, QualType) {
  OS << V;
}

template <>
void print(llvm::raw_ostream &OS, const Pointer &P, ASTContext &Ctx,
           QualType Ty) {
  P.print(OS, Ctx, Ty);
}

template <>
void print(llvm::raw_ostream &OS, const FnPointer &P, ASTContext &,
           QualType Ty) {
  if (const ValueDecl *Decl = P.asValueDecl()) {
    if (Ty->isReferenceType()) {
      OS << Decl->getName();
    } else {
      OS << "&" << Decl->getName();
    }
  } else {
    OS << "nullptr";
    return;
  }
}

template <>
void print(llvm::raw_ostream &OS, const MemberPointer &P, ASTContext &,
           QualType) {
  if (const ValueDecl *VD = P.getDecl())
    OS << '&' << *cast<CXXRecordDecl>(VD->getDeclContext()) << "::" << *VD;
  else
    OS << "0";
  return;
}

void InterpFrame::describe(llvm::raw_ostream &OS) {
  const FunctionDecl *F = getCallee();
  auto *M = dyn_cast<CXXMethodDecl>(F);
  if (M && M->isInstance() && !isa<CXXConstructorDecl>(F)) {
    print(OS, This, S.getASTCtx(), S.getASTCtx().getRecordType(M->getParent()));
    OS << "->";
  }
  OS << *F << "(";
  unsigned Off = Func->hasRVO() ? primSize(PT_Ptr) : 0;
  for (unsigned I = 0, N = F->getNumParams(); I < N; ++I) {
    QualType Ty = F->getParamDecl(I)->getType();

    PrimType PrimTy;
    if (llvm::Optional<PrimType> T = S.Ctx.classify(Ty)) {
      PrimTy = *T;
    } else {
      PrimTy = PT_Ptr;
    }

    TYPE_SWITCH(PrimTy, print(OS, stackRef<T>(Off), S.getASTCtx(), Ty));
    Off += align(primSize(PrimTy));
    if (I + 1 != N)
      OS << ", ";
  }
  OS << ")";
}

Frame *InterpFrame::getCaller() const {
  if (Caller->Caller)
    return Caller;
  return S.getSplitFrame();
}

SourceLocation InterpFrame::getCallLocation() const {
  if (!Caller->Func)
    return S.getSource(nullptr, {}).getLoc();
  return S.getSource(Caller->Func, RetPC - sizeof(uintptr_t)).getLoc();
}

CodePtr InterpFrame::getRetPCDiag() const {
  if (!Caller->Func)
    return {};
  return RetPC - sizeof(uintptr_t);
}

const FunctionDecl *InterpFrame::getCallee() const {
  return Func ? Func->getDecl() : nullptr;
}

Pointer InterpFrame::getLocalPointer(unsigned Offset) {
  assert(Offset < Func->getFrameSize() && "Invalid local offset.");
  char *Addr = Locals.get() + Offset - sizeof(Block);
  return Pointer(reinterpret_cast<Block *>(Addr));
}

Block *InterpFrame::getParamBlock(unsigned Off) {
  // Return the block if it was created previously.
  auto Pt = Params.find(Off);
  if (Pt != Params.end()) {
    return reinterpret_cast<Block *>(Pt->second.get());
  }

  // Allocate memory to store the parameter and the block metadata.
  const auto &Desc = Func->getParamDescriptor(Off);
  size_t BlockSize = sizeof(Block) + Desc.second->getAllocSize();
  auto Memory = std::make_unique<char[]>(BlockSize);
  auto *B = new (Memory.get()) Block(Desc.second);

  // Copy the initial value.
  TYPE_SWITCH(Desc.first, new (B->data()) T(stackRef<T>(Off)));

  // Record the param.
  Params.insert({Off, std::move(Memory)});
  return B;
}
