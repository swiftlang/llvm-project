//===--- ByteCodeStmtGen.cpp - Code generator for expressions ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ByteCodeStmtGen.h"
#include "ByteCodeEmitter.h"
#include "ByteCodeGenError.h"
#include "Context.h"
#include "Function.h"
#include "PrimType.h"
#include "Program.h"
#include "State.h"
#include "clang/Basic/LLVM.h"
#include "clang/AST/ASTLambda.h"

using namespace clang;
using namespace clang::interp;

namespace clang {
namespace interp {

/// Scope managing label targets.
template <class Emitter> class LabelScope {
public:
protected:
  LabelScope(ByteCodeStmtGen<Emitter> *Ctx) : Ctx(Ctx) {}

  /// ByteCodeStmtGen instance.
  ByteCodeStmtGen<Emitter> *Ctx;
};

/// Sets the context for break/continue statements.
template <class Emitter> class LoopScope final : public LabelScope<Emitter> {
public:
  using LabelTy = typename ByteCodeStmtGen<Emitter>::LabelTy;
  using OptLabelTy = typename ByteCodeStmtGen<Emitter>::OptLabelTy;

  LoopScope(ByteCodeStmtGen<Emitter> *Ctx, LabelTy BreakLabel,
            LabelTy ContinueLabel)
      : LabelScope<Emitter>(Ctx), OldBreakLabel(Ctx->BreakLabel),
        OldContinueLabel(Ctx->ContinueLabel) {
    this->Ctx->BreakLabel = BreakLabel;
    this->Ctx->ContinueLabel = ContinueLabel;
  }

  ~LoopScope() {
    this->Ctx->BreakLabel = OldBreakLabel;
    this->Ctx->ContinueLabel = OldContinueLabel;
  }

private:
  OptLabelTy OldBreakLabel;
  OptLabelTy OldContinueLabel;
};

// Sets the context for a switch scope, mapping labels.
template <class Emitter> class SwitchScope final : public LabelScope<Emitter> {
public:
  using LabelTy = typename ByteCodeStmtGen<Emitter>::LabelTy;
  using OptLabelTy = typename ByteCodeStmtGen<Emitter>::OptLabelTy;
  using CaseMap = typename ByteCodeStmtGen<Emitter>::CaseMap;

  SwitchScope(ByteCodeStmtGen<Emitter> *Ctx, CaseMap &&CaseLabels,
              LabelTy BreakLabel, OptLabelTy DefaultLabel)
      : LabelScope<Emitter>(Ctx), OldBreakLabel(Ctx->BreakLabel),
        OldDefaultLabel(this->Ctx->DefaultLabel),
        OldCaseLabels(std::move(this->Ctx->CaseLabels)) {
    this->Ctx->BreakLabel = BreakLabel;
    this->Ctx->DefaultLabel = DefaultLabel;
    this->Ctx->CaseLabels = std::move(CaseLabels);
  }

  ~SwitchScope() {
    this->Ctx->BreakLabel = OldBreakLabel;
    this->Ctx->DefaultLabel = OldDefaultLabel;
    this->Ctx->CaseLabels = std::move(OldCaseLabels);
  }

private:
  OptLabelTy OldBreakLabel;
  OptLabelTy OldDefaultLabel;
  CaseMap OldCaseLabels;
};

} // namespace interp
} // namespace clang

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitFunc(const FunctionDecl *F) {
  FD = F;
  ReturnType = F->getReturnType();

  // Set up fields and context if a constructor.
  if (auto *MD = dyn_cast<CXXMethodDecl>(F)) {
    const CXXRecordDecl *RD = MD->getParent()->getCanonicalDecl();
    if (auto *R = this->getRecord(RD)) {
      if (auto *CD = dyn_cast<CXXConstructorDecl>(MD)) {
        // If this is a copy or move constructor for a union, emit a special
        // opcode since the operation cannot be represented in the AST easily.
        if (CD->isDefaulted() && CD->isCopyOrMoveConstructor() && RD->isUnion()) {
          if (!this->emitInitUnion(CD))
            return false;
          return this->emitRetCons(CD);
        }

        if (CD->isDelegatingConstructor()) {
          CXXConstructorDecl::init_const_iterator I = CD->init_begin();
          {
            ExprScope<Emitter> InitScope(this);
            if (!this->visitThisInitializer((*I)->getInit()))
              return false;
          }
        } else {
          // Compile all member initializers.
          for (const CXXCtorInitializer *Init : CD->inits()) {
            BlockScope<Emitter> Scope(this);
            if (!visitCtorInit(R, Init))
              return false;
          }
        }
      }
      if (isLambdaCallOperator(MD)) {
        // Retrieve the override for this and the VarDecls
        // captured by a a lambda expression.
        this->CaptureThis = nullptr;
        RD->getCaptureFields(this->CaptureFields, *this->CaptureThis);
      }
    } else {
      return this->bail(RD);
    }
  }

  if (auto *Body = F->getBody())
    if (!visitStmt(Body))
      return false;

  // Emit a guard return to protect against a code path missing one.
  if (isa<CXXConstructorDecl>(F))
    return this->emitRetCons(SourceInfo{});
  else if (F->getReturnType()->isVoidType())
    return this->emitRetVoid(SourceInfo{});
  else
    return this->emitNoRet(SourceInfo{});
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitStmt(const Stmt *S) {
  switch (S->getStmtClass()) {
  case Stmt::CompoundStmtClass:
    return visitCompoundStmt(cast<CompoundStmt>(S));
  case Stmt::DeclStmtClass:
    return visitDeclStmt(cast<DeclStmt>(S));
  case Stmt::ForStmtClass:
    return visitForStmt(cast<ForStmt>(S));
  case Stmt::WhileStmtClass:
    return visitWhileStmt(cast<WhileStmt>(S));
  case Stmt::DoStmtClass:
    return visitDoStmt(cast<DoStmt>(S));
  case Stmt::ReturnStmtClass:
    return visitReturnStmt(cast<ReturnStmt>(S));
  case Stmt::IfStmtClass:
    return visitIfStmt(cast<IfStmt>(S));
  case Stmt::BreakStmtClass:
    return visitBreakStmt(cast<BreakStmt>(S));
  case Stmt::ContinueStmtClass:
    return visitContinueStmt(cast<ContinueStmt>(S));
  case Stmt::SwitchStmtClass:
    return visitSwitchStmt(cast<SwitchStmt>(S));
  case Stmt::CaseStmtClass:
  case Stmt::DefaultStmtClass:
    return visitCaseStmt(cast<SwitchCase>(S));
  case Stmt::CXXForRangeStmtClass:
    return visitCXXForRangeStmt(cast<CXXForRangeStmt>(S));
  case Stmt::AttributedStmtClass:
    return visitAttributedStmt(cast<AttributedStmt>(S));
  case Stmt::NullStmtClass:
    return true;
  default: {
    if (auto *Exp = dyn_cast<Expr>(S))
      return this->discard(Exp);
    return this->bail(S);
  }
  }
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitCompoundStmt(
    const CompoundStmt *CompoundStmt) {
  BlockScope<Emitter> Scope(this);
  for (auto *InnerStmt : CompoundStmt->body()) {
    if (!visitStmt(InnerStmt))
      return false;
  }
  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitDeclStmt(const DeclStmt *DS) {
  for (auto *D : DS->decls()) {
    // Variable declarator.
    if (auto *VD = dyn_cast<VarDecl>(D)) {
      if (!visitVarDecl(VD))
        return false;
      continue;
    }

    // Decomposition declarator.
    if (auto *DD = dyn_cast<DecompositionDecl>(D)) {
      return this->bail(DD);
    }
  }

  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitForStmt(const ForStmt *FS) {
  // Compile the initialisation statement in an outer scope.
  BlockScope<Emitter> OuterScope(this);
  if (auto *Init = FS->getInit())
    if (!visitStmt(Init))
      return false;

  LabelTy LabelStart = this->getLabel();
  LabelTy LabelEnd = this->getLabel();

  // Compile the condition, body and increment in the loop scope.
  this->emitLabel(LabelStart);
  {
    BlockScope<Emitter> InnerScope(this);

    if (auto *Cond = FS->getCond()) {
      if (auto *CondDecl = FS->getConditionVariableDeclStmt())
        if (!visitDeclStmt(CondDecl))
          return false;

      if (!this->visit(Cond))
        return false;

      if (!this->jumpFalse(LabelEnd))
        return false;
    }

    if (auto *Body = FS->getBody()) {
      LabelTy LabelSkip = this->getLabel();
      LoopScope<Emitter> FlowScope(this, LabelEnd, LabelSkip);
      if (!visitStmt(Body))
        return false;
      this->emitLabel(LabelSkip);
    }

    if (auto *Inc = FS->getInc()) {
      ExprScope<Emitter> IncScope(this);
      if (!this->discard(Inc))
        return false;
    }
    if (!this->jump(LabelStart))
      return false;
  }
  this->emitLabel(LabelEnd);
  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitWhileStmt(const WhileStmt *WS) {
  LabelTy LabelStart = this->getLabel();
  LabelTy LabelEnd = this->getLabel();

  this->emitLabel(LabelStart);
  {
    BlockScope<Emitter> InnerScope(this);
    if (auto *CondDecl = WS->getConditionVariableDeclStmt())
      if (!visitDeclStmt(CondDecl))
        return false;

    if (!this->visit(WS->getCond()))
      return false;

    if (!this->jumpFalse(LabelEnd))
      return false;

    {
      LoopScope<Emitter> FlowScope(this, LabelEnd, LabelStart);
      if (!visitStmt(WS->getBody()))
        return false;
    }
    if (!this->jump(LabelStart))
      return false;
  }
  this->emitLabel(LabelEnd);

  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitDoStmt(const DoStmt *DS) {
  LabelTy LabelStart = this->getLabel();
  LabelTy LabelEnd = this->getLabel();
  LabelTy LabelSkip = this->getLabel();

  this->emitLabel(LabelStart);
  {
    {
      LoopScope<Emitter> FlowScope(this, LabelEnd, LabelSkip);
      if (!visitStmt(DS->getBody()))
        return false;
      this->emitLabel(LabelSkip);
    }

    {
      ExprScope<Emitter> CondScope(this);
      if (!this->visitBool(DS->getCond()))
        return false;
    }

    if (!this->jumpTrue(LabelStart))
      return false;
  }
  this->emitLabel(LabelEnd);

  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitReturnStmt(const ReturnStmt *RS) {
  if (const Expr *RE = RS->getRetValue()) {
    assert(!isa<CXXConstructorDecl>(FD) && "constructor cannot return value");
    ExprScope<Emitter> RetScope(this);
    if (ReturnType->isVoidType()) {
      // Void - discard result.
      if (!this->discard(RE))
        return false;
      this->emitCleanup();
      return this->emitRetVoid(RS);
    } else if (llvm::Optional<PrimType> RT = this->classify(ReturnType)) {
      // Primitive types are simply returned.
      if (!this->visit(RE))
        return false;
      this->emitCleanup();
      return this->emitRet(*RT, RS);
    } else {
      // RVO - construct the value in the return location.
      auto ReturnLocation = [this, RE] { return this->emitGetParamPtr(0, RE); };
      if (!this->visitInitializer(RE, ReturnLocation))
        return false;
      this->emitCleanup();
      return this->emitRetVoid(RS);
    }
  } else {
    this->emitCleanup();
    if (isa<CXXConstructorDecl>(FD))
      return this->emitRetCons(RS);
    else
      return this->emitRetVoid(RS);
  }
}

static bool ConvertToBool(const Expr *E, bool &Result) {
  if (auto *CE = dyn_cast<ConstantExpr>(E)) {
    switch (CE->getResultStorageKind()) {
    case ConstantExpr::RSK_Int64: {
      Result = !CE->getResultAsAPSInt().isNullValue();
      return true;
    }
    case ConstantExpr::RSK_APValue: {
      // TODO: implement this
      return false;
    }
    case ConstantExpr::RSK_None:
      return false;
    }
  }

  return false;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitIfStmt(const IfStmt *IS) {
  BlockScope<Emitter> IfScope(this);
  if (auto *CondInit = IS->getInit())
    if (!visitStmt(IS->getInit()))
      return false;

  if (const DeclStmt *CondDecl = IS->getConditionVariableDeclStmt())
    if (!visitDeclStmt(CondDecl))
      return false;

  bool Result;
  if (ConvertToBool(IS->getCond(), Result)) {
    // As an optimisation, only lower one of the branches if the
    // condition is a constant of known value.
    if (Result)
      return visitStmt(IS->getThen());
    const Stmt *Else = IS->getElse();
    return Else ? visitStmt(Else) : true;
  }

  if (!this->visitBool(IS->getCond()))
    return false;

  if (const Stmt *Else = IS->getElse()) {
    LabelTy LabelElse = this->getLabel();
    LabelTy LabelEnd = this->getLabel();
    if (!this->jumpFalse(LabelElse))
      return false;
    if (!visitStmt(IS->getThen()))
      return false;
    if (!this->jump(LabelEnd))
      return false;
    this->emitLabel(LabelElse);
    if (!visitStmt(Else))
      return false;
    this->emitLabel(LabelEnd);
  } else {
    LabelTy LabelEnd = this->getLabel();
    if (!this->jumpFalse(LabelEnd))
      return false;
    if (!visitStmt(IS->getThen()))
      return false;
    this->emitLabel(LabelEnd);
  }

  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitBreakStmt(const BreakStmt *BS) {
  if (!BreakLabel)
    return this->bail(BS);
  return this->jump(*BreakLabel);
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitContinueStmt(const ContinueStmt *CS) {
  if (!ContinueLabel)
    return this->bail(CS);
  return this->jump(*ContinueLabel);
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitSwitchStmt(const SwitchStmt *SS) {
  BlockScope<Emitter> InnerScope(this);

  QualType Ty = SS->getCond()->getType();
  if (Optional<PrimType> T = this->classify(Ty)) {
    // The condition is stored in a local and fetched for every test.
    unsigned Off = this->allocateLocalPrimitive(SS->getCond(), Ty, *T,
                                                /*isConst=*/true);

    // Compile the condition in its own scope.
    {
      ExprScope<Emitter> CondScope(this);
      if (const Stmt *CondInit = SS->getInit())
        if (!visitStmt(SS->getInit()))
          return false;

      if (const DeclStmt *CondDecl = SS->getConditionVariableDeclStmt())
        if (!visitDeclStmt(CondDecl))
          return false;

      if (!this->visit(SS->getCond()))
        return false;

      if (!this->emitSetLocal(*T, Off, SS->getCond()))
        return false;
    }

    LabelTy LabelEnd = this->getLabel();

    // Generate code to inspect all case labels, jumping to the matched one.
    const DefaultStmt *Default = nullptr;
    CaseMap Labels;
    for (auto *SC = SS->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase()) {
      LabelTy Label = this->getLabel();
      Labels.insert({SC, Label});

      if (auto *DS = dyn_cast<DefaultStmt>(SC)) {
        Default = DS;
        continue;
      }

      if (auto *CS = dyn_cast<CaseStmt>(SC)) {
        if (!this->emitGetLocal(*T, Off, CS))
          return false;
        if (!this->visit(CS->getLHS()))
          return false;

        if (auto *RHS = CS->getRHS()) {
          if (!this->visit(CS->getRHS()))
            return false;
          if (!this->emitInRange(*T, CS))
            return false;
        } else {
          if (!this->emitEQ(*T, PT_Bool, CS))
            return false;
        }

        if (!this->jumpTrue(Label))
          return false;
        continue;
      }

      return this->bail(SS);
    }

    // If a case wasn't matched, jump to default or skip the body.
    if (!this->jump(Default ? Labels[Default] : LabelEnd))
      return false;
    OptLabelTy DefaultLabel = Default ? Labels[Default] : OptLabelTy{};

    // Compile the body, using labels defined previously.
    SwitchScope<Emitter> LabelScope(this, std::move(Labels), LabelEnd,
                                    DefaultLabel);
    if (!visitStmt(SS->getBody()))
      return false;
    this->emitLabel(LabelEnd);
    return true;
  } else {
    return this->bail(SS);
  }
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitCaseStmt(const SwitchCase *CS) {
  auto It = CaseLabels.find(CS);
  if (It == CaseLabels.end())
    return this->bail(CS);

  this->emitLabel(It->second);
  return visitStmt(CS->getSubStmt());
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitCXXForRangeStmt(const CXXForRangeStmt *FS) {
  BlockScope<Emitter> Scope(this);

  // Emit the optional init-statement.
  if (auto *Init = FS->getInit()) {
    if (!visitStmt(Init))
      return false;
  }

  // Initialise the __range variable.
  if (!visitStmt(FS->getRangeStmt()))
    return false;

  // Create the __begin and __end iterators.
  if (!visitStmt(FS->getBeginStmt()) || !visitStmt(FS->getEndStmt()))
    return false;

  LabelTy LabelStart = this->getLabel();
  LabelTy LabelEnd = this->getLabel();

  this->emitLabel(LabelStart);
  {
    // Lower the condition.
    if (!this->visitBool(FS->getCond()))
      return false;
    if (!this->jumpFalse(LabelEnd))
      return false;

    // Lower the loop var and body, marking labels for continue/break.
    {
      BlockScope<Emitter> InnerScope(this);
      if (!visitStmt(FS->getLoopVarStmt()))
        return false;

      LabelTy LabelSkip = this->getLabel();
      {
        LoopScope<Emitter> FlowScope(this, LabelEnd, LabelSkip);

        if (!visitStmt(FS->getBody()))
          return false;
      }
      this->emitLabel(LabelSkip);
    }

    // Increment: ++__begin
    if (!visitStmt(FS->getInc()))
      return false;
    if (!this->jump(LabelStart))
      return false;
  }
  this->emitLabel(LabelEnd);
  return true;
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitAttributedStmt(const AttributedStmt *AS) {
  // As a general principle, C++11 attributes can be ignored without
  // any semantic impact.
  return visitStmt(AS->getSubStmt());
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitVarDecl(const VarDecl *VD) {
  auto DT = VD->getType();
  const Expr *Init = VD->getInit();

  if (!VD->hasLocalStorage()) {
    // No code generation required.
    return true;
  }

  // Integers, pointers, primitives.
  if (Optional<PrimType> T = this->classify(DT)) {
    auto Off = this->allocateLocalPrimitive(VD, DT, *T, DT.isConstQualified());
    // Compile the initialiser in its own scope.
    {
      ExprScope<Emitter> Scope(this);
      if (!this->visit(Init))
        return false;
    }
    // Set the value.
    return this->emitSetLocal(*T, Off, VD);
  } else {
    // Composite types - allocate storage and initialize it.
    if (auto Off = this->allocateLocal(VD, Init->getType())) {
      return this->visitLocalInitializer(Init, *Off);
    } else {
      return this->bail(VD);
    }
  }
}

template <class Emitter>
bool ByteCodeStmtGen<Emitter>::visitCtorInit(Record *This,
                                             const CXXCtorInitializer *Ctor) {
  auto *Init = Ctor->getInit();
  if (auto *Base = Ctor->getBaseClass()) {
    auto *Decl = Base->getAs<RecordType>()->getDecl();
    return this->visitBaseInitializer(Init, [this, &This, Init, Ctor, Decl] {
      if (Ctor->isBaseVirtual())
        return this->emitGetPtrThisVirtBase(Decl, Init);
      else
        return this->emitGetPtrThisBase(This->getBase(Decl)->getOffset(), Init);
    });
  }

  if (const FieldDecl *FD = Ctor->getMember()) {
    ExprScope<Emitter> Scope(this);
    return this->withField(FD, [this, FD, Init, This](const Record::Field *F) {
      if (Optional<PrimType> T = this->classify(FD->getType())) {
        // Primitive type, can be computed and set.
        if (!this->visit(Init))
          return false;
        if (This->isUnion())
          return this->emitInitThisFieldActive(*T, F->getOffset(), Init);
        if (F->getDecl()->isBitField())
          return this->emitInitThisBitField(*T, F, Init);
        return this->emitInitThisField(*T, F->getOffset(), Init);
      } else {
        // Nested structures.
        return this->visitInitializer(Init, [this, F, Init] {
          return this->emitGetPtrThisField(F->getOffset(), Init);
        });
      }
    });
  }

  if (auto *FD = Ctor->getIndirectMember()) {
    auto *Init = Ctor->getInit();
    if (Optional<PrimType> T = this->classify(Init->getType())) {
      ArrayRef<NamedDecl *> Chain = FD->chain();
      Record *R = This;
      for (unsigned I = 0, N = Chain.size(); I < N; ++I) {
        const bool IsUnion = R->isUnion();
        auto *Member = cast<FieldDecl>(Chain[I]);
        auto MemberTy = Member->getType();

        auto *FD = R->getField(Member);
        if (!FD)
          return this->bail(Member);

        const unsigned Off = FD->getOffset();
        if (I + 1 == N) {
          // Last member - set the field.
          if (!this->visit(Init))
            return false;
          if (IsUnion) {
            return this->emitInitFieldActive(*T, Off, Init);
          } else {
            return this->emitInitFieldPop(*T, Off, Init);
          }
        } else {
          // Next field must be a record - fetch it.
          R = this->getRecord(cast<RecordType>(MemberTy)->getDecl());
          if (!R)
            return this->bail(Member);

          if (IsUnion) {
            if (I == 0) {
              // Base member - activate this pointer.
              if (!this->emitGetPtrActiveThisField(Off, Member))
                return false;
            } else {
              // Intermediate - active subfield.
              if (!this->emitGetPtrActiveField(Off, Member))
                return false;
            }
          } else {
            if (I == 0) {
              // Base member - get this pointer.
              if (!this->emitGetPtrThisField(Off, Member))
                return false;
            } else {
              // Intermediate - get pointer.
              if (!this->emitGetPtrField(Off, Member))
                return false;
            }
          }
        }
      }
    } else {
      return this->bail(FD);
    }

    return true;
  }

  llvm_unreachable("unknown base initializer kind");
}

namespace clang {
namespace interp {

template class ByteCodeStmtGen<ByteCodeEmitter>;

} // namespace interp
} // namespace clang
