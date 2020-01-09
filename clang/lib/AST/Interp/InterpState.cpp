//===--- InterpState.cpp - Interpreter for the constexpr VM -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InterpState.h"
#include "Function.h"
#include "InterpFrame.h"
#include "InterpStack.h"
#include "Opcode.h"
#include "Program.h"
#include "State.h"
#include "clang/AST/ExprCXX.h"
#include <limits>

using namespace clang;
using namespace clang::interp;

using APSInt = llvm::APSInt;

InterpState::InterpState(State &Parent, Program &P, InterpStack &Stk,
                         Context &Ctx, SourceMapper *M)
    : Parent(Parent), M(M), P(P), Stk(Stk), Ctx(Ctx), Current(nullptr),
      CallStackDepth(Parent.getCallStackDepth() + 1) {}

InterpState::~InterpState() {
  while (Current) {
    InterpFrame *Next = Current->Caller;
    delete Current;
    Current = Next;
  }

  while (DeadBlocks) {
    DeadBlock *Next = DeadBlocks->Next;
    free(DeadBlocks);
    DeadBlocks = Next;
  }
}

Frame *InterpState::getCurrentFrame() {
  if (Current && Current->Caller) {
    return Current;
  } else {
    return Parent.getCurrentFrame();
  }
}

bool InterpState::reportOverflow(const Expr *E, const llvm::APSInt &Value) {
  QualType Type = E->getType();
  CCEDiag(E, diag::note_constexpr_overflow) << Value << Type;
  return noteUndefinedBehavior();
}

void InterpState::deallocate(Block *B) {
  Descriptor *Desc = B->getDescriptor();
  if (B->hasPointers()) {
    size_t Size = B->getSize();

    // Allocate a new block, transferring over pointers.
    char *Memory = reinterpret_cast<char *>(malloc(sizeof(DeadBlock) + Size));
    auto *D = new (Memory) DeadBlock(DeadBlocks, B);

    // Move data from one block to another.
    if (Desc->MoveFn)
      Desc->MoveFn(B, B->data(), D->data(), Desc);
  } else {
    // Free storage, if necessary.
    if (Desc->DtorFn)
      Desc->DtorFn(B, B->data(), Desc);
  }
}

SourceInfo InterpState::getSource(CodePtr PC) const {
  for (InterpFrame *F = Current; F; F = F->getInterpCaller()) {
    Function *Fn = F->getFunction();
    if (!Fn)
      break;
    if (!Fn->isDefaulted())
      return getSource(Fn, PC);
    PC = F->getRetPCDiag();
  }
  return getSource(nullptr, {});
}

const BlockPointer *InterpState::CheckLive(CodePtr PC, const Pointer &Ptr,
                                           AccessKinds AK, bool IsPolymorphic) {
  if (auto *BlockPtr = Ptr.asBlock()) {
    // Block pointer - live if not out of lifetime.
    if (!BlockPtr->isLive()) {
      bool IsTemp = BlockPtr->isTemporary();
      const SourceInfo &I = getSource(PC);
      FFDiag(I, diag::note_constexpr_lifetime_ended, 1) << AK << !IsTemp;

      if (IsTemp)
        Note(BlockPtr->getDeclLoc(), diag::note_constexpr_temporary_here);
      else
        Note(BlockPtr->getDeclLoc(), diag::note_declared_at);
      return nullptr;
    }
    return BlockPtr;
  } else if (auto *ExternPtr = Ptr.asExtern()) {
    // Extern pointer - not live.
    if (IsPolymorphic) {
      const SourceInfo &I = getSource(PC);
      QualType StarThisType =
          getASTCtx().getLValueReferenceType(ExternPtr->getFieldType());
      FFDiag(I, diag::note_constexpr_polymorphic_unknown_dynamic_type)
          << AK
          << ExternPtr->getAsString(getASTCtx(), StarThisType);
    } else if (!checkingPotentialConstantExpression()) {
      auto *VD = ExternPtr->getDecl();
      const SourceInfo &I = getSource(PC);
      FFDiag(I, diag::note_constexpr_ltor_non_constexpr, 1) << VD;
      Note(VD->getLocation(), diag::note_declared_at);
    }
    return nullptr;
  } else {
    // Null pointer - not live.
    const SourceInfo &I = getSource(PC);
    FFDiag(I, diag::note_constexpr_access_null) << AK;
    return nullptr;
  }
}

bool InterpState::CheckSubobject(CodePtr PC, const Pointer &Ptr,
                                 CheckSubobjectKind CSK) {
  if (!Ptr.isOnePastEnd())
    return true;
  FFDiag(getSource(PC), diag::note_constexpr_past_end_subobject) << CSK;
  return false;
}

bool InterpState::CheckField(CodePtr PC, const Pointer &Ptr) {
 if (Ptr.isZero()) {
    CCEDiag(getSource(PC), diag::note_constexpr_null_subobject) << CSK_Field;
    return true;
  }
  return CheckSubobject(PC, Ptr, CSK_Field);
}

bool InterpState::CheckRange(CodePtr PC, const Pointer &Ptr, AccessKinds AK) {
  if (!Ptr.isOnePastEnd())
    return true;
  const SourceInfo &I = getSource(PC);
  FFDiag(I, diag::note_constexpr_access_past_end) << AK;
  return false;
}

bool InterpState::CheckRange(CodePtr PC, const BlockPointer &Ptr,
                             AccessKinds AK) {
  if (!Ptr.isOnePastEnd())
    return true;
  const SourceInfo &I = getSource(PC);
  FFDiag(I, diag::note_constexpr_access_past_end) << AK;
  return false;
}

bool InterpState::CheckRange(CodePtr PC, const ExternPointer &Ptr,
                             AccessKinds AK) {
  if (!Ptr.isOnePastEnd())
    return true;
  const SourceInfo &I = getSource(PC);
  FFDiag(I, diag::note_constexpr_access_past_end) << AK;
  return false;
}

const BlockPointer *InterpState::CheckLoad(CodePtr PC, const Pointer &Ptr,
                                           AccessKinds AK) {
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK)) {
    if (CheckLoad(PC, *BlockPtr, AK))
      return BlockPtr;
  }
  return nullptr;
}

bool InterpState::CheckLoad(CodePtr PC, const BlockPointer &Ptr,
                            AccessKinds AK) {
  if (!CheckRange(PC, Ptr, AK))
    return false;
  if (!CheckInitialized(PC, Ptr, AK))
    return false;
  if (!CheckActive(PC, Ptr, AK))
    return false;
  if (!CheckTemporary(PC, Ptr, AK))
    return false;
  if (!CheckMutable(PC, Ptr, AK))
    return false;
  return true;
}

const BlockPointer *InterpState::CheckStore(CodePtr PC, const Pointer &Ptr,
                                            AccessKinds AK) {
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK)) {
    if (CheckStore(PC, *BlockPtr, AK))
      return BlockPtr;
  }
  return nullptr;
}

bool InterpState::CheckStore(CodePtr PC, const BlockPointer &Ptr,
                             AccessKinds AK) {
  if (!CheckRange(PC, Ptr, AK))
    return false;
  if (!CheckGlobal(PC, Ptr))
    return false;
  if (!CheckConst(PC, Ptr))
    return false;
  return true;
}

const BlockPointer *InterpState::CheckLoadStore(CodePtr PC, const Pointer &Ptr,
                                                AccessKinds AK) {
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK)) {
    if (CheckLoadStore(PC, *BlockPtr, AK))
      return BlockPtr;
  }
  return nullptr;
}

bool InterpState::CheckLoadStore(CodePtr PC, const BlockPointer &Ptr,
                                 AccessKinds AK) {
  if (!CheckRange(PC, Ptr, AK))
    return false;
  if (!CheckInitialized(PC, Ptr, AK))
    return false;
  if (!CheckActive(PC, Ptr, AK))
    return false;
  if (!CheckTemporary(PC, Ptr, AK))
    return false;
  if (!CheckMutable(PC, Ptr, AK))
    return false;
  if (!CheckGlobal(PC, Ptr))
    return false;
  if (!CheckConst(PC, Ptr))
    return false;
  return true;
}

const BlockPointer *InterpState::CheckInit(CodePtr PC, const Pointer &Ptr) {
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK_Assign)) {
    if (CheckRange(PC, *BlockPtr, AK_Assign))
      return BlockPtr;
  }
  return nullptr;
}

const BlockPointer *InterpState::CheckObject(CodePtr PC, const Pointer &Ptr) {
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK_MemberCall)) {
    if (CheckRange(PC, *BlockPtr, AK_MemberCall))
      return BlockPtr;
  }
  return nullptr;
}

const BlockPointer *InterpState::CheckTypeid(CodePtr PC, const Pointer &Ptr) {
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK_TypeId, /*isPolymorphic=*/true)) {
    if (CheckRange(PC, *BlockPtr, AK_TypeId))
      return BlockPtr;
  }
  return nullptr;
}

bool InterpState::CheckInvoke(CodePtr PC, const Pointer &Ptr) {
  if (auto *ExternPtr = Ptr.asExtern()) {
    if (CheckRange(PC, *ExternPtr, AK_MemberCall))
      return true;
  }
  if (auto *BlockPtr = CheckLive(PC, Ptr, AK_MemberCall)) {
    if (CheckRange(PC, *BlockPtr, AK_MemberCall))
      return true;
  }
  return false;
}

bool InterpState::CheckCallable(CodePtr PC, const FunctionDecl *FD) {
  if (auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
    if (MD->isVirtual()) {
      if (!getLangOpts().CPlusPlus2a) {
        CCEDiag(getSource(PC), diag::note_constexpr_virtual_call);
        return false;
      }
    }
  }

  if (!FD->isConstexpr()) {
    if (getLangOpts().CPlusPlus11) {
      // If this function is not constexpr because it is an inherited
      // non-constexpr constructor, diagnose that directly.
      auto *CD = dyn_cast<CXXConstructorDecl>(FD);
      if (CD && CD->isInheritingConstructor()) {
        auto *Inherited = CD->getInheritedConstructor().getConstructor();
        if (!Inherited->isConstexpr())
          FD = CD = Inherited;
      }

      // FIXME: If FD is an implicitly-declared special member function
      // or an inheriting constructor, we should be much more explicit about why
      // it's not constexpr.
      if (CD && CD->isInheritingConstructor())
        FFDiag(getSource(PC), diag::note_constexpr_invalid_inhctor, 1)
            << CD->getInheritedConstructor().getConstructor()->getParent();
      else
        FFDiag(getSource(PC), diag::note_constexpr_invalid_function, 1)
            << FD->isConstexpr() << (bool)CD << FD;
      Note(FD->getLocation(), diag::note_declared_at);
    } else {
      FFDiag(getSource(PC), diag::note_invalid_subexpr_in_const_expr);
    }
    return false;
  }

  return true;
}

bool InterpState::CheckThis(CodePtr PC, const Pointer &This) {
  if (!This.isZero())
    return true;

  const SourceInfo &I = getSource(PC);

  bool IsImplicit = false;
  if (auto *E = dyn_cast_or_null<CXXThisExpr>(I.asExpr()))
    IsImplicit = E->isImplicit();

  if (getLangOpts().CPlusPlus11) {
    FFDiag(I, diag::note_constexpr_this) << IsImplicit;
  } else {
    FFDiag(I);
  }

  return false;
}

bool InterpState::CheckPure(CodePtr PC, const CXXMethodDecl *MD) {
  if (!MD->isPure())
    return true;
  const SourceInfo &I = getSource(PC);
  FFDiag(I, diag::note_constexpr_pure_virtual_call, 1) << MD;
  Note(MD->getLocation(), diag::note_declared_at);
  return false;
}

bool InterpState::CheckInitialized(CodePtr PC, const BlockPointer &Ptr,
                                   AccessKinds AK) {
  if (Ptr.isInitialized())
    return true;
  if (!checkingPotentialConstantExpression()) {
    const SourceInfo &I = getSource(PC);
    FFDiag(I, diag::note_constexpr_access_uninit) << AK << false;
  }
  return false;
}

bool InterpState::CheckActive(CodePtr PC, const BlockPointer &Ptr,
                              AccessKinds AK) {
  if (Ptr.isActive())
    return true;

  // Get the inactive field descriptor.
  const FieldDecl *InactiveField = Ptr.getField();

  // Walk up the pointer chain to find the union which is not active.
  std::function<bool(const BlockPointer &)> Diagnose;
  Diagnose = [this, PC, AK, InactiveField, &Diagnose](const BlockPointer &Ptr) {
    const BlockPointer &U = Ptr.getBase();
    if (!U.isActive()) {
      return Diagnose(U);
    }

    // Find the active field of the union.
    Record *R = U.getRecord();
    assert(R && R->isUnion() && "Not a union");
    const FieldDecl *ActiveField = nullptr;
    for (unsigned I = 0, N = R->getNumFields(); I < N; ++I) {
      const BlockPointer &Field = U.atField(R->getField(I)->getOffset());
      if (Field.isActive()) {
        ActiveField = Field.getField();
        break;
      }
    }

    const SourceInfo &I = getSource(PC);
    FFDiag(I, diag::note_constexpr_access_inactive_union_member)
        << AK << InactiveField << !ActiveField << ActiveField;
    return false;
  };

  return Diagnose(Ptr);
}

bool InterpState::CheckTemporary(CodePtr PC, const BlockPointer &Ptr,
                                 AccessKinds AK) {
  if (auto ID = Ptr.getDeclID()) {
    if (!Ptr.isStaticTemporary())
      return true;

    if (Ptr.getDeclType().isConstQualified())
      return true;

    if (P.getCurrentDecl() == ID)
      return true;

    const SourceInfo &I = getSource(PC);
    FFDiag(I, diag::note_constexpr_access_static_temporary, 1) << AK;
    Note(Ptr.getDeclLoc(), diag::note_constexpr_temporary_here);
    return false;
  }
  return true;
}

bool InterpState::CheckGlobal(CodePtr PC, const BlockPointer &Ptr) {
  if (auto ID = Ptr.getDeclID()) {
    if (!Ptr.isStatic())
      return true;

    if (P.getCurrentDecl() == ID)
      return true;

    FFDiag(getSource(PC), diag::note_constexpr_modify_global);
    return false;
  }
  return true;
}

bool InterpState::CheckMutable(CodePtr PC, const BlockPointer &Ptr,
                               AccessKinds AK) {
  if (!Ptr.isMutable())
    return true;

  const FieldDecl *Field = Ptr.getField();
  const SourceInfo &I = getSource(PC);
  FFDiag(I, diag::note_constexpr_access_mutable, 1) << AK << Field;
  Note(Field->getLocation(), diag::note_declared_at);
  return false;
}

bool InterpState::CheckConst(CodePtr PC, const BlockPointer &Ptr) {
  // TODO: implement a proper check.
  if (Current && Current->getFunction()->isConstructor())
    return true;

  if (!Ptr.isConst())
    return true;

  const QualType Ty = Ptr.getFieldType();
  const SourceInfo &I = getSource(PC);
  FFDiag(I, diag::note_constexpr_modify_const_type) << Ty;
  return false;
}
