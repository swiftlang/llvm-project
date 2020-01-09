//===--- InterpLoop.cpp - Interpreter for the constexpr VM ------*- C++ -*-===//
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

#include "InterpLoop.h"
#include "Function.h"
#include "Interp.h"
#include "InterpFrame.h"
#include "InterpHelper.h"
#include "InterpStack.h"
#include "Opcode.h"
#include "PrimType.h"
#include "Program.h"
#include "State.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "llvm/ADT/APSInt.h"
#include <limits>
#include <vector>

using namespace clang;
using namespace clang::interp;

//===----------------------------------------------------------------------===//
// Ret
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
static bool Ret(InterpState &S, CodePtr &PC, APValue &Result) {
  S.CallStackDepth--;
  const T &Ret = S.Stk.pop<T>();

  assert(S.Current->getFrameOffset() == S.Stk.size() && "Invalid frame");
  if (!S.checkingPotentialConstantExpression())
    S.Current->popArgs();

  if (InterpFrame *Caller = S.Current->Caller) {
    PC = S.Current->getRetPC();
    delete S.Current;
    S.Current = Caller;
    S.Stk.push<T>(Ret);
  } else {
    delete S.Current;
    S.Current = nullptr;
    return Ret.toAPValue(S.getASTCtx(), Result);
  }
  return true;
}

static bool RetVoid(InterpState &S, CodePtr &PC, APValue &Result) {
  S.CallStackDepth--;

  assert(S.Current->getFrameOffset() == S.Stk.size() && "Invalid frame");
  if (!S.checkingPotentialConstantExpression())
    S.Current->popArgs();

  if (InterpFrame *Caller = S.Current->Caller) {
    PC = S.Current->getRetPC();
    delete S.Current;
    S.Current = Caller;
  } else {
    delete S.Current;
    S.Current = nullptr;
  }
  return true;
}

static bool RetCons(InterpState &S, CodePtr &PC, APValue &Result) {
  S.CallStackDepth--;

  assert(S.Current->getFrameOffset() == S.Stk.size() && "Invalid frame");
  if (!S.checkingPotentialConstantExpression())
    S.Current->popArgs();

  S.Current->construct();

  if (InterpFrame *Caller = S.Current->Caller) {
    PC = S.Current->getRetPC();
    delete S.Current;
    S.Current = Caller;
  } else {
    delete S.Current;
    S.Current = nullptr;
  }
  return true;
}

static bool RetValue(InterpState &S, CodePtr &Pt, APValue &Result) {
  llvm::report_fatal_error("Interpreter cannot return values");
}

//===----------------------------------------------------------------------===//
// Jmp, Jt, Jf
//===----------------------------------------------------------------------===//

static bool Jmp(InterpState &S, CodePtr &PC, int32_t Offset) {
  PC += Offset;
  return true;
}

static bool Jt(InterpState &S, CodePtr &PC, int32_t Offset) {
  if (S.Stk.pop<Boolean>().isTrue())
    PC += Offset;
  return true;
}

static bool Jf(InterpState &S, CodePtr &PC, int32_t Offset) {
  if (S.Stk.pop<Boolean>().isFalse())
    PC += Offset;
  return true;
}

//===----------------------------------------------------------------------===//
// Call, NoCall, Invoke, NoInvoke, IndirectInvoke
//===----------------------------------------------------------------------===//

static bool HandleCall(InterpState &S, CodePtr &PC, CodePtr OpPC, Function *F,
                       Pointer &&This) {
  if (!S.CheckCallable(OpPC, F->getDecl()))
    return false;
  if (S.checkingPotentialConstantExpression())
    return false;

  // Adjust the state.
  S.CallStackDepth++;

  // Construct a new frame.
  S.Current = new InterpFrame(S, F, S.Current, PC, std::move(This));

  // Transfer control to the callee.
  PC = F->getCodeBegin();
  return true;
}

static bool HandleNoCall(InterpState &S, CodePtr PC, const FunctionDecl *FD) {
  CodePtr OpPC = PC - sizeof(FD);
  if (!S.CheckCallable(OpPC, FD))
    return false;
  if (S.checkingPotentialConstantExpression())
    return false;
  const SourceInfo &CallLoc = S.getSource(OpPC);
  if (S.getLangOpts().CPlusPlus11) {
    S.FFDiag(CallLoc, diag::note_constexpr_invalid_function, 1)
        << FD->isConstexpr() << isa<CXXConstructorDecl>(FD) << FD;
    S.Note(FD->getLocation(), diag::note_declared_at);
  } else {
    S.FFDiag(CallLoc, diag::note_invalid_subexpr_in_const_expr);
  }
  return false;
}

static bool Call(InterpState &S, CodePtr &PC, Function *F) {
  return HandleCall(S, PC, PC - sizeof(F), F, {});
}

static bool Invoke(InterpState &S, CodePtr &PC, Function *F) {
  CodePtr OpPC = PC - sizeof(F);

  // Get the this pointer, ensure it's live.
  Pointer This = S.Stk.pop<Pointer>();
  if (!S.CheckInvoke(OpPC, This))
    return false;

  // Dispatch to the method.
  return HandleCall(S, PC, PC - sizeof(F), F, std::move(This));
}

static bool VirtualInvoke(InterpState &S, CodePtr &PC, const CXXMethodDecl *M) {
  CodePtr OpPC = PC - sizeof(M);

  // Get the this pointer, ensure it's live.
  Pointer This = S.Stk.pop<Pointer>();
  if (const BlockPointer *ThisBlock = S.CheckObject(OpPC, This)) {
    // Dispatch to the virtual method.
    BlockPointer Base(*ThisBlock);
    const CXXMethodDecl *Override = VirtualLookup(Base, M);
    if (!S.CheckPure(OpPC, Override))
      return false;

    if (auto Func = S.P.getOrCreateFunction(S, Override)) {
      if (*Func)
        return HandleCall(S, PC, OpPC, *Func, std::move(Base));
    } else {
      consumeError(Func.takeError());
    }
    return HandleNoCall(S, PC, Override);
  }
  return false;
}

static bool NoCall(InterpState &S, CodePtr &PC, const FunctionDecl *FD) {
  CodePtr OpPC = PC - sizeof(FD);

  // Function was not defined - emit a diagnostic.
  if (auto *F = S.P.getFunction(FD)) {
    // Function was defined in the meantime - rewrite opcode.
    S.Current->getFunction()->relocateCall(OpPC, F);
    return HandleCall(S, PC, OpPC, F, {});
  }
  // Function was not defined - emit a diagnostic.
  return HandleNoCall(S, PC, FD);
}

static bool NoInvoke(InterpState &S, CodePtr &PC, const CXXMethodDecl *MD) {
  CodePtr OpPC = PC - sizeof(MD);

  // Access checks.
  Pointer This = S.Stk.pop<Pointer>();
  if (!S.CheckInvoke(OpPC, This))
    return false;

  if (auto *F = S.P.getFunction(MD)) {
    // Function was defined in the meantime - rewrite opcode.
    S.Current->getFunction()->relocateInvoke(OpPC, F);
    return HandleCall(S, PC, OpPC, F, std::move(This));
  }
  // Function was not defined - emit a diagnostic.
  return HandleNoCall(S, PC, MD);
}

static bool IndirectCall(InterpState &S, CodePtr &PC) {
  const auto &FnPtr = S.Stk.pop<FnPointer>();
  if (!FnPtr.isValid())
    return false;
  if (FnPtr.isZero() || FnPtr.isWeak()) {
    S.FFDiag(S.getSource(PC));
    return false;
  }

  if (Function *F = FnPtr.asFunction())
    return HandleCall(S, PC, PC, F, {});

  auto *Decl = FnPtr.asFunctionDecl();
  if (auto Func = S.P.getOrCreateFunction(S, Decl)) {
    if (*Func)
      return HandleCall(S, PC, PC, *Func, {});
  } else {
    consumeError(Func.takeError());
  }
  return HandleNoCall(S, PC, Decl);
}

static bool IndirectInvoke(InterpState &S, CodePtr &PC) {
  // Fetch and validate the field pointer.
  const MemberPointer &Field = S.Stk.pop<MemberPointer>();
  if (Field.isZero()) {
    S.FFDiag(S.getSource(PC));
    return false;
  }

  // Validate the 'this' pointer.
  Pointer This = S.Stk.pop<Pointer>();
  if (const BlockPointer *ThisBlock = S.CheckObject(PC, This)) {
    // Fetch a pointer to the member function.
    BlockPointer Base(*ThisBlock);
    const CXXMethodDecl *Method = IndirectLookup(Base, Field);
    if (!Method)
      return false;
    if (!S.CheckPure(PC, Method))
      return false;

    if (Expected<Function *> Func = S.P.getOrCreateFunction(S, Method)) {
      if (*Func)
        return HandleCall(S, PC, PC, *Func, std::move(Base));
    } else {
      consumeError(Func.takeError());
    }
    return HandleNoCall(S, PC, Method);
  }
  return false;
}

namespace clang {
namespace interp {

bool Interpret(InterpState &S, APValue &Result) {
  CodePtr PC = S.Current->getPC();

  for (;;) {
    auto Op = PC.read<Opcode>();
    CodePtr OpPC = PC;

    switch (Op) {
#define GET_INTERP
#include "Opcodes.inc"
#undef GET_INTERP
    }
  }
}

} // namespace interp
} // namespace clang
