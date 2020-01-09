//===--- InterpState.h - Interpreter state for the constexpr VM -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Definition of the interpreter state and entry point.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_INTERPSTATE_H
#define LLVM_CLANG_AST_INTERP_INTERPSTATE_H

#include "Context.h"
#include "Function.h"
#include "InterpStack.h"
#include "State.h"
#include "clang/AST/APValue.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/Expr.h"
#include "clang/AST/OptionalDiagnostic.h"

namespace clang {
namespace interp {
class Context;
class Function;
class InterpStack;
class InterpFrame;
class SourceMapper;

/// Interpreter context.
class InterpState final : public State {
public:
  InterpState(State &Parent, Program &P, InterpStack &Stk, Context &Ctx,
              SourceMapper *M = nullptr);

  ~InterpState();

  // Stack frame accessors.
  Frame *getSplitFrame() { return Parent.getCurrentFrame(); }
  Frame *getCurrentFrame() override;
  unsigned getCallStackDepth() override { return CallStackDepth; }
  const Frame *getBottomFrame() const override {
    return Parent.getBottomFrame();
  }

  // Acces objects from the walker context.
  Expr::EvalStatus &getEvalStatus() const override {
    return Parent.getEvalStatus();
  }
  Context &getCtx() const { return Ctx; }
  ASTContext &getASTCtx() const override { return Parent.getASTCtx(); }

  /// Returns the type implementing int.
  PrimType getShortType() { return Ctx.getShortType(); }
  /// Returns the type implementing int.
  PrimType getIntType() { return Ctx.getIntType(); }
  /// Returns the type implementing int.
  PrimType getLongType() { return Ctx.getLongType(); }
  /// Returns the type implementing unsigned long.
  PrimType getUnsignedLongType() { return Ctx.getUnsignedLongType(); }
  /// Returns the type implementing int.
  PrimType getLongLongType() { return Ctx.getLongLongType(); }

  /// Returns the bit width of integers.
  uint64_t getIntWidth() { return Ctx.getIntWidth(); }
  /// returns the bit width of unsigned longs.
  uint64_t getUnsignedLongWidth() { return Ctx.getUnsignedLongWidth(); }

  // Forward status checks and updates to the walker.
  bool checkingForUndefinedBehavior() const override {
    return Parent.checkingForUndefinedBehavior();
  }
  bool keepEvaluatingAfterFailure() const override {
    return Parent.keepEvaluatingAfterFailure();
  }
  bool checkingPotentialConstantExpression() const override {
    return Parent.checkingPotentialConstantExpression();
  }
  bool noteUndefinedBehavior() override {
    return Parent.noteUndefinedBehavior();
  }
  bool noteFailure() override {
    return Parent.noteFailure();
  }
  bool hasActiveDiagnostic() override { return Parent.hasActiveDiagnostic(); }
  void setActiveDiagnostic(bool Flag) override {
    Parent.setActiveDiagnostic(Flag);
  }
  void setFoldFailureDiagnostic(bool Flag) override {
    Parent.setFoldFailureDiagnostic(Flag);
  }
  bool hasPriorDiagnostic() override { return Parent.hasPriorDiagnostic(); }

  bool hasDiagnostics() const override { return Parent.hasDiagnostics(); }

  bool inConstantContext() const override { return Parent.inConstantContext(); }

  /// Reports overflow and return true if evaluation should continue.
  bool reportOverflow(const Expr *E, const llvm::APSInt &Value);

  /// Deallocates a pointer.
  void deallocate(Block *B);

  /// Delegates source mapping to the mapper.
  SourceInfo getSource(Function *F, CodePtr PC) const {
    return M ? M->getSource(F, PC) : F->getSource(PC);
  }

  /// Maps the location to the innermost non-default action.
  SourceInfo getSource(CodePtr PC) const;

public:
  /// Checks if a value can be loaded from a block.
  const BlockPointer *CheckLoad(CodePtr PC, const Pointer &Ptr, AccessKinds AK);
  bool CheckLoad(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);

  /// Checks if a value can be stored in a block.
  const BlockPointer *CheckStore(CodePtr PC, const Pointer &Ptr,
                                 AccessKinds AK);
  bool CheckStore(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);

  /// Checks if a value can be mutated.
  const BlockPointer *CheckLoadStore(CodePtr PC, const Pointer &Ptr,
                                     AccessKinds AK);
  bool CheckLoadStore(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);

  /// Checks if a value can be initialised in a block.
  const BlockPointer *CheckInit(CodePtr PC, const Pointer &Ptr);
  /// Checks if a method can be invoked on an object.
  const BlockPointer *CheckObject(CodePtr PC, const Pointer &Ptr);
  /// Checks if a structure can be inspected by typeid.
  const BlockPointer *CheckTypeid(CodePtr PC, const Pointer &Ptr);
  /// Checks if a method can be invoked on an object.
  bool CheckInvoke(CodePtr PC, const Pointer &Ptr);

  /// Checks if a record's field can be addressed.
  bool CheckSubobject(CodePtr PC, const Pointer &Ptr, CheckSubobjectKind AK);
  /// Checks if a field can be derived from the pointer.
  bool CheckField(CodePtr PC, const Pointer &Ptr);
  /// Checks if a block pointer is in range.
  bool CheckRange(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);
  /// Checks if a extern pointer is in range.
  bool CheckRange(CodePtr PC, const ExternPointer &Ptr, AccessKinds AK);
  /// Checks if a pointer is in range.
  bool CheckRange(CodePtr PC, const Pointer &Ptr, AccessKinds AK);
  /// Checks if a method can be called.
  bool CheckCallable(CodePtr PC, const FunctionDecl *FD);
  /// Checks the 'this' pointer.
  bool CheckThis(CodePtr PC, const Pointer &This);
  /// Checks if a method is pure virtual.
  bool CheckPure(CodePtr PC, const CXXMethodDecl *MD);

private:
  /// Checks if a pointer is live and accesible.
  const BlockPointer *CheckLive(CodePtr PC, const Pointer &Ptr, AccessKinds AK,
                                bool IsPolymorphic = false);
  /// Checks if storage was initialized.
  bool CheckInitialized(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);
  /// Checks if a field is active if in a union.
  bool CheckActive(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);
  /// Checks if a termpoary is accessed.
  bool CheckTemporary(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);
  /// Checks if a global is illegally accessed.
  bool CheckGlobal(CodePtr PC, const BlockPointer &Ptr);
  /// Checks if a mutable field is accessed.
  bool CheckMutable(CodePtr PC, const BlockPointer &Ptr, AccessKinds AK);
  /// Checks if a pointer points to const storage.
  bool CheckConst(CodePtr PC, const BlockPointer &Ptr);

private:
  /// AST Walker state.
  State &Parent;
  /// Dead block chain.
  DeadBlock *DeadBlocks = nullptr;
  /// Reference to the offset-source mapping.
  SourceMapper *M;

public:
  /// Reference to the module containing all bytecode.
  Program &P;
  /// Temporary stack.
  InterpStack &Stk;
  /// Interpreter Context.
  Context &Ctx;
  /// The current frame.
  InterpFrame *Current = nullptr;
  /// Call stack depth.
  unsigned CallStackDepth;
};

} // namespace interp
} // namespace clang

#endif
