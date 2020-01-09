//===--- Setter.h - Memory setter instructions for the VM -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the opcodes which write values to pointers.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_SETTER_H
#define LLVM_CLANG_AST_INTERP_OPCODES_SETTER_H

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SetLocal(InterpState &S, CodePtr OpPC, uint32_t I) {
  S.Current->setLocal<T>(I, S.Stk.pop<T>());
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SetParam(InterpState &S, CodePtr OpPC, uint32_t I) {
  S.Current->setParam<T>(I, S.Stk.pop<T>());
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SetGlobal(InterpState &S, CodePtr OpPC, GlobalLocation G) {
  // TODO: emit warning.
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitGlobal(InterpState &S, CodePtr OpPC, GlobalLocation G) {
  S.P.getGlobal(G)->deref<T>() = S.Stk.pop<T>();
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool Store(InterpState &S, CodePtr OpPC) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.peek<Pointer>();
  if (auto *BlockPtr = S.CheckStore(OpPC, Ptr, AK_Assign)) {
    BlockPtr->deref<T>() = Val;
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool StorePop(InterpState &S, CodePtr OpPC) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (auto *BlockPtr = S.CheckStore(OpPC, Ptr, AK_Assign)) {
    BlockPtr->deref<T>() = Val;
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SetField(InterpState &S, CodePtr OpPC, uint32_t I) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Obj = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Obj, CSK_Field))
    return false;
  const Pointer &Field = Obj.atField(I);
  if (auto *BlockPtr = S.CheckStore(OpPC, Field, AK_Assign)) {
    BlockPtr->deref<T>() = Val;
    S.Stk.push<Pointer>(Field);
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SetFieldPop(InterpState &S, CodePtr OpPC, uint32_t I) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Obj = S.Stk.pop<Pointer>();
  if (!S.CheckSubobject(OpPC, Obj, CSK_Field))
    return false;
  const Pointer &Field = Obj.atField(I);
  if (auto *BlockPtr = S.CheckStore(OpPC, Field, AK_Assign)) {
    BlockPtr->deref<T>() = Val;
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool SetThisField(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const T &Val = S.Stk.pop<T>();
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  const Pointer &Field = This.atField(I);
  if (auto *BlockPtr = S.CheckStore(OpPC, Field, AK_Assign)) {
    BlockPtr->deref<T>() = Val;
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool StoreBitField(InterpState &S, CodePtr OpPC) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.peek<Pointer>();
  if (auto *BlockPtr = S.CheckStore(OpPC, Ptr, AK_Assign)) {
    if (auto *FD = Ptr.getField()) {
      BlockPtr->deref<T>() = Val.truncate(FD->getBitWidthValue(S.getASTCtx()));
    } else {
      BlockPtr->deref<T>() = Val;
    }
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool StoreBitFieldPop(InterpState &S, CodePtr OpPC) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (auto *BlockPtr = S.CheckStore(OpPC, Ptr, AK_Assign)) {
    if (auto *FD = Ptr.getField()) {
      BlockPtr->deref<T>() = Val.truncate(FD->getBitWidthValue(S.getASTCtx()));
    } else {
      BlockPtr->deref<T>() = Val;
    }
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitPop(InterpState &S, CodePtr OpPC) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  if (auto *BlockPtr = S.CheckInit(OpPC, Ptr)) {
    new (&BlockPtr->deref<T>()) T(Val);
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitElem(InterpState &S, CodePtr OpPC, uint32_t Idx) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.peek<Pointer>().atIndex(Idx);
  if (auto *BlockPtr = S.CheckInit(OpPC, Ptr)) {
    new (&BlockPtr->deref<T>()) T(Val);
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitElemPop(InterpState &S, CodePtr OpPC, uint32_t Idx) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.pop<Pointer>().atIndex(Idx);
  if (auto *BlockPtr = S.CheckInit(OpPC, Ptr)) {
    new (&BlockPtr->deref<T>()) T(Val);
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitBitField(InterpState &S, CodePtr OpPC, const Record::Field *F) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Field = S.Stk.pop<Pointer>().atField(F->getOffset());
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    unsigned BitFieldWidth = F->getDecl()->getBitWidthValue(S.getASTCtx());
    const auto &Trunc = Val.truncate(BitFieldWidth);
    new (&BlockPtr->deref<T>()) T(Trunc);
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitThisField(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  const Pointer &Field = This.atField(I);
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = S.Stk.pop<T>();
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitThisFieldActive(InterpState &S, CodePtr OpPC, uint32_t I) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  const Pointer &Field = This.atField(I);
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = S.Stk.pop<T>();
    BlockPtr->activate();
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitField(InterpState &S, CodePtr OpPC, uint32_t I) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Field = S.Stk.peek<Pointer>().atField(I);
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = Val;
    BlockPtr->activate();
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitFieldPop(InterpState &S, CodePtr OpPC, uint32_t I) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Field = S.Stk.pop<Pointer>().atField(I);
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = Val;
    BlockPtr->activate();
    BlockPtr->initialize();
    return true;
  }
  return true;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitFieldPeek(InterpState &S, CodePtr OpPC, uint32_t I) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Field = S.Stk.peek<Pointer>().atField(I);
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = Val;
    BlockPtr->activate();
    BlockPtr->initialize();
    S.Stk.push<T>(Val);
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitFieldActive(InterpState &S, CodePtr OpPC, uint32_t I) {
  const T &Val = S.Stk.pop<T>();
  const Pointer &Ptr = S.Stk.pop<Pointer>();
  const Pointer &Field = Ptr.atField(I);

  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = Val;
    BlockPtr->activate();
    BlockPtr->initialize();
    return true;
  }
  return false;
}

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool InitThisBitField(InterpState &S, CodePtr OpPC, const Record::Field *F) {
  if (S.checkingPotentialConstantExpression())
    return false;
  const Pointer &This = S.Current->getThis();
  if (!S.CheckThis(OpPC, This))
    return false;
  unsigned BitFieldWidth = F->getDecl()->getBitWidthValue(S.getASTCtx());
  const Pointer &Field = This.atField(F->getOffset());
  const auto &Val = S.Stk.pop<T>();
  const auto &Trunc = Val.truncate(BitFieldWidth);
  if (auto *BlockPtr = S.CheckInit(OpPC, Field)) {
    BlockPtr->deref<T>() = Trunc;
    BlockPtr->initialize();
    return true;
  }
  return false;
}

inline bool Initialise(InterpState &S, CodePtr OpPC) {
  S.Stk.pop<Pointer>().asBlock()->initialize();
  return true;
}

inline bool Activate(InterpState &S, CodePtr OpPC) {
  S.Stk.pop<Pointer>().asBlock()->activate();
  return true;
}

#endif
