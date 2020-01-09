//===--- Builtin.cpp - Builtins for the constexpr VM ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Builtin.h"
#include "InterpFrame.h"
#include "InterpState.h"
#include "Pointer.h"
#include "PrimType.h"
#include "Program.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/Builtins.h"
#include "llvm/ADT/APFloat.h"

using namespace clang;
using namespace clang::interp;

//===----------------------------------------------------------------------===//
// Helper methods
//===----------------------------------------------------------------------===//

static void ReportInvalidExpr(InterpState &S, CodePtr OpPC, unsigned Op) {
  const char *Name = S.getASTCtx().BuiltinInfo.getName(Op);

  if (S.getLangOpts().CPlusPlus11)
    S.CCEDiag(S.getSource(OpPC), diag::note_constexpr_invalid_function)
        << /*isConstexpr*/ 0 << /*isConstructor*/ 0
        << (std::string("'") + Name + "'");
  else
    S.CCEDiag(S.getSource(OpPC), diag::note_invalid_subexpr_in_const_expr);
}

static void PushUnsignedLong(InterpState &S, unsigned long I) {
  INT_TYPE_SWITCH(S.getUnsignedLongType(),
                  S.Stk.push<T>(T::from(I, S.getUnsignedLongWidth())));
}

static void PushInt(InterpState &S, int I) {
  INT_TYPE_SWITCH(S.getIntType(), S.Stk.push<T>(T::from(I, S.getIntWidth())));
}

//===----------------------------------------------------------------------===//
// strlen
//===----------------------------------------------------------------------===//

static bool BuiltinStrLen(InterpState &S, CodePtr OpPC) {
  // Get the pointer to the string and ensure it's an array of primitives.
  const auto &Ptr = S.Stk.pop<Pointer>();
  if (const BlockPointer *BlockPtr = S.CheckLoad(OpPC, Ptr, AK_Read)) {
    assert(BlockPtr->inArray() && "not an array");
    // Find the null terminator, starting at the pointed element.
    const unsigned Size = BlockPtr->getSize() - BlockPtr->getOffset();
    char *Data = &BlockPtr->deref<char>();
    if (llvm::Optional<PrimType> T = BlockPtr->elementType()) {
      switch (*T) {
      case PT_Uint8:
      case PT_Sint8:
        for (unsigned Off = 0, I = 0; Off < Size; Off += 1, ++I) {
          if (*reinterpret_cast<int8_t *>(Data + Off) == 0) {
            PushUnsignedLong(S, I);
            return true;
          }
        }
        break;
      case PT_Uint16:
      case PT_Sint16:
        for (unsigned Off = 0, I = 0; Off < Size; Off += 2, ++I) {
          if (*reinterpret_cast<uint16_t *>(Data + Off) == 0) {
            PushUnsignedLong(S, I);
            return true;
          }
        }
        break;
      case PT_Uint32:
      case PT_Sint32:
        for (unsigned Off = 0, I = 0; Off < Size; Off += 4, ++I) {
          if (*reinterpret_cast<uint32_t *>(Data + Off) == 0) {
            PushUnsignedLong(S, I);
            return true;
          }
        }
        break;
      default:
        llvm_unreachable("invalid element type");
        break;
      }
    } else {
      llvm_unreachable("invalid element type");
    }
  }

  S.FFDiag(S.getSource(OpPC), diag::note_constexpr_access_past_end) << AK_Read;
  return false;
}

//===----------------------------------------------------------------------===//
// Strcmp
//===----------------------------------------------------------------------===//

template <typename T>
bool CompareElement(const BlockPointer *A, const BlockPointer *B,
                    uint64_t I, bool IsWide, int &Result) {
  const T &LHS = A->atIndex(I).deref<T>();
  const T &RHS = B->atIndex(I).deref<T>();
  if (LHS == RHS) {
    Result = 0;
  } else {
    if (IsWide)
      Result = LHS < RHS ? -1 : 1;
    else
      Result = LHS.toUnsigned() < RHS.toUnsigned() ? -1 : 1;
  }
  return LHS.isZero();
}

static bool BuiltinStrCmp(InterpState &S, CodePtr OpPC, unsigned Op) {
  const Pointer &ArgA = S.Stk.pop<Pointer>();
  const Pointer &ArgB = S.Stk.pop<Pointer>();

  // First two args are the pointers to the blocks to be compared.
  const BlockPointer *B = S.CheckLoad(OpPC, ArgA, AK_Read);
  if (!B)
    return false;
  const BlockPointer *A = S.CheckLoad(OpPC, ArgB, AK_Read);
  if (!A)
    return false;
  assert(A->inArray() && A->inArray() && "not an array");

  // The types of the arrays must match and must be valid primitives
  // since these methods perform element-wise comparisons.
  assert(A->elementType() && "not a primitive array");
  assert(A->elementType() == B->elementType() && "mismatched arrays");
  PrimType T = *A->elementType();

  // Determine the operation parameters.
  bool StopAtNull;
  bool IsWide;
  bool HasSize;

  switch (Op) {
  case Builtin::BIstrcmp:
  case Builtin::BI__builtin_strcmp:
    StopAtNull = true;
    IsWide = false;
    HasSize = false;
    break;
  case Builtin::BIstrncmp:
  case Builtin::BI__builtin_strncmp:
    StopAtNull = true;
    IsWide = false;
    HasSize = true;
    break;
  case Builtin::BIwcscmp:
  case Builtin::BI__builtin_wcscmp:
    StopAtNull = true;
    IsWide = true;
    HasSize = false;
    break;
  case Builtin::BIwcsncmp:
  case Builtin::BI__builtin_wcsncmp:
    StopAtNull = true;
    IsWide = true;
    HasSize = true;
    break;
  case Builtin::BIwmemcmp:
  case Builtin::BI__builtin_wmemcmp:
    StopAtNull = false;
    IsWide = true;
    HasSize = true;
    break;
  default:
    llvm_unreachable("invalid builtin");
  }

  // Determine the number of bytes to compare.
  uint64_t Limit;
  if (HasSize) {
    INT_TYPE_SWITCH(S.getUnsignedLongType(), Limit = uint64_t(S.Stk.pop<T>()));
  } else {
    Limit = uint64_t(-1);
  }

  // Compare individual elements.
  const uint64_t LenA = A->getNumElems();
  const uint64_t LenB = B->getNumElems();
  int Result = 0;

  for (uint64_t I = 0; I < Limit; ++I) {
    // Ensure pointers are in the bounds of their respective arrays.
    if (I >= LenA || I >= LenB) {
      S.FFDiag(S.getSource(OpPC), diag::note_constexpr_access_past_end)
          << AK_Read;
      return false;
    }
    bool IsZero;
    INT_TYPE_SWITCH(
        T, IsZero = CompareElement<T>(A, B, I, IsWide, Result));
    if (Result != 0)
      break;
    if (StopAtNull && IsZero) {
      Result = 0;
      break;
    }
  }

  PushInt(S, Result);
  return true;
}

//===----------------------------------------------------------------------===//
// bcmp
//===----------------------------------------------------------------------===//

static bool BuiltinByteCmp(InterpState &S, CodePtr OpPC) {
  return false;
}

//===----------------------------------------------------------------------===//
// bcmp
//===----------------------------------------------------------------------===//

template <PrimType Name, class T = typename PrimConv<Name>::T>
bool MemCpy(const BlockPointer *DstBlock, const BlockPointer *SrcBlock, unsigned N) {
  T *Dst = &DstBlock->decay().deref<T>();
  T *Src = &SrcBlock->decay().deref<T>();
  for (unsigned I = 0; I < N; ++I) {
    *Dst++ = *Src++;
  }
  return true;
}

static bool BuiltinMemCpy(InterpState &S, CodePtr OpPC) {
  // Fetch the number of bytes to compare.
  uint64_t Limit;
  INT_TYPE_SWITCH(S.getUnsignedLongType(), Limit = uint64_t(S.Stk.pop<T>()));

  // Fetch the pointer to load from.
  const VoidPointer &SrcVoidPtr = S.Stk.pop<VoidPointer>();
  const Pointer *SrcPtr = SrcVoidPtr.asPointer();
  if (!SrcPtr)
    return false;
  const BlockPointer *SrcBlock = S.CheckLoad(OpPC, *SrcPtr, AK_Read);
  if (!SrcBlock)
    return false;

  // Fetch the pointer to write to.
  const VoidPointer &DstVoidPtr = S.Stk.peek<VoidPointer>();
  const Pointer *DstPtr = DstVoidPtr.asPointer();
  if (!DstPtr)
    return false;
  const BlockPointer *DstBlock = S.CheckStore(OpPC, *DstPtr, AK_Assign);
  if (!DstBlock)
    return false;

  if (DstBlock->inArray() && SrcBlock->inArray()) {
    if (auto DT = DstBlock->elementType()) {
      if (auto ST = SrcBlock->elementType()) {
        if (DT == ST) {
          switch (*DT) {
          case PT_Uint64: {
            if (Limit % 8 == 0)
              return MemCpy<PT_Uint64>(DstBlock, SrcBlock, Limit / 8);
            break;
          }
          default:
            break;
          }
        }
      }
    }
  }

  // TODO: implement conversions
  return false;
}
//===----------------------------------------------------------------------===//
// inf
//===----------------------------------------------------------------------===//

static bool BuiltinInf(const llvm::fltSemantics &Sem, InterpState &S,
                       CodePtr OpPC) {
  S.Stk.push<Real>(Real::from(llvm::APFloat::getInf(Sem), Sem));
  return true;
}

//===----------------------------------------------------------------------===//
// fabs
//===----------------------------------------------------------------------===//

static bool BuiltinFabs(InterpState &S, CodePtr OpPC) {
  const Real &V = S.Stk.pop<Real>();
  if (V.isNegative())
    S.Stk.push<Real>(Real::negate(V));
  else
    S.Stk.push<Real>(V);
  return true;
}

//===----------------------------------------------------------------------===//
// nan
//===----------------------------------------------------------------------===//

static bool BuiltinNaN(const llvm::fltSemantics &Sem, bool SNaN,
                       InterpState &S, CodePtr OpPC) {
  // Get the pointer and ensure it is readable.
  const auto &Ptr = S.Stk.pop<Pointer>();
  const BlockPointer *BPtr = S.CheckLoad(OpPC, Ptr, AK_Read);
  if (BPtr == nullptr) {
    S.FFDiag(S.getSource(OpPC));
    return false;
  }

  // Ensure elements are primitive characters.
  assert(BPtr->inArray() && "not an array");
  llvm::Optional<PrimType> T = BPtr->elementType();
  if (!T) {
    S.FFDiag(S.getSource(OpPC));
    return false;
  }

  llvm::APInt f;

  // Decode the string.
  StringRef Str(&BPtr->deref<char>(), BPtr->getSize() - BPtr->getOffset() - 1);
  switch (*T) {
    case PT_Sint8:
    case PT_Uint8: {
      // Treat empty strings as if they were zero.
      if (Str.empty()) {
        f = llvm::APInt(32, 0);
      } else if (Str.getAsInteger(0, f)) {
        S.FFDiag(S.getSource(OpPC));
        return false;
      }
      break;
    }

    default:
      llvm_unreachable("invalid type");
  }

  if (S.getASTCtx().isNan2008()) {
    if (SNaN)
      S.Stk.push<Real>(Real::from(llvm::APFloat::getSNaN(Sem, false, &f), Sem));
    else
      S.Stk.push<Real>(Real::from(llvm::APFloat::getQNaN(Sem, false, &f), Sem));
  } else {
    // Prior to IEEE 754-2008, architectures were allowed to choose whether
    // the first bit of their significand was set for qNaN or sNaN. MIPS chose
    // a different encoding to what became a standard in 2008, and for pre-
    // 2008 revisions, MIPS interpreted sNaN-2008 as qNan and qNaN-2008 as
    // sNaN. This is now known as "legacy NaN" encoding.
    if (SNaN)
      S.Stk.push<Real>(Real::from(llvm::APFloat::getQNaN(Sem, false, &f), Sem));
    else
      S.Stk.push<Real>(Real::from(llvm::APFloat::getSNaN(Sem, false, &f), Sem));
  }

  return true;
}

//===----------------------------------------------------------------------===//
// clz
//===----------------------------------------------------------------------===//

bool CountLeadingZeros(InterpState &S, CodePtr OpPC, PrimType Ty) {
  int Zeros;
  INT_TYPE_SWITCH(Ty, Zeros = S.Stk.pop<T>().countLeadingZeros());
  PushInt(S, Zeros);
  return true;
}

//===----------------------------------------------------------------------===//
// __builtin_is_constant_evaluated
//===----------------------------------------------------------------------===//

bool BuiltinIsConstantEvaluated(InterpState &S, CodePtr OpPC) {
  if (S.inConstantContext() && !S.checkingPotentialConstantExpression()) {
    // FIXME: Find a better way to avoid duplicated diagnostics.
    if (S.hasDiagnostics()) {
      bool IsBuiltin = false;
      if (S.Current && S.Current->getFunction()) {
        const FunctionDecl *F = S.Current->getFunction()->getDecl();
        if (F->isInStdNamespace() && F->getIdentifier() &&
            F->getIdentifier()->isStr("is_constant_evaluated")) {
          IsBuiltin = true;
        }
      }

      if (IsBuiltin) {
        S.report(S.Current->getCallLocation(),
                 diag::warn_is_constant_evaluated_always_true_constexpr)
            << "std::is_constant_evaluated";
      } else {
        S.report(S.getSource(OpPC).getLoc(),
                 diag::warn_is_constant_evaluated_always_true_constexpr)
            << "__builtin_is_constant_evaluated";
      }
    }
  }

  using BoolT = typename PrimConv<PT_Bool>::T;
  S.Stk.push<BoolT>(BoolT::from(S.inConstantContext()));
  return true;
}

namespace clang {
namespace interp {

bool InterpBuiltin(InterpState &S, CodePtr OpPC, unsigned Op) {
  switch (Op) {
  default:
    S.FFDiag(S.getSource(OpPC).getLoc(),
             diag::err_experimental_clang_interp_failed);
    return false;

  // strlen
  case Builtin::BIstrlen:
  case Builtin::BIwcslen:
    ReportInvalidExpr(S, OpPC, Op);
    LLVM_FALLTHROUGH;
  case Builtin::BI__builtin_strlen:
  case Builtin::BI__builtin_wcslen:
    return BuiltinStrLen(S, OpPC);

  // strcmp
  case Builtin::BIstrcmp:
  case Builtin::BIstrncmp:
  case Builtin::BIwcscmp:
  case Builtin::BIwcsncmp:
  case Builtin::BIwmemcmp:
    ReportInvalidExpr(S, OpPC, Op);
    LLVM_FALLTHROUGH;
  case Builtin::BI__builtin_strcmp:
  case Builtin::BI__builtin_strncmp:
  case Builtin::BI__builtin_wcscmp:
  case Builtin::BI__builtin_wcsncmp:
  case Builtin::BI__builtin_wmemcmp:
    return BuiltinStrCmp(S, OpPC, Op);

  // memcmp
  case Builtin::BImemcmp:
  case Builtin::BIbcmp:
    ReportInvalidExpr(S, OpPC, Op);
    LLVM_FALLTHROUGH;
  case Builtin::BI__builtin_memcmp:
  case Builtin::BI__builtin_bcmp:
    return BuiltinByteCmp(S, OpPC);

  // huge, inf
  case Builtin::BI__builtin_inf:
  case Builtin::BI__builtin_huge_val:
    return BuiltinInf(llvm::APFloat::IEEEdouble(), S, OpPC);
  case Builtin::BI__builtin_inff:
  case Builtin::BI__builtin_huge_valf:
    return BuiltinInf(llvm::APFloat::IEEEsingle(), S, OpPC);
  case Builtin::BI__builtin_inff128:
  case Builtin::BI__builtin_huge_valf128:
    return BuiltinInf(llvm::APFloat::IEEEquad(), S, OpPC);
  case Builtin::BI__builtin_infl:
  case Builtin::BI__builtin_huge_vall:
    return BuiltinInf(S.getASTCtx().getLongDoubleSemantics(), S, OpPC);

  // nan
  case Builtin::BI__builtin_nans:
    return BuiltinNaN(llvm::APFloat::IEEEdouble(), true, S, OpPC);
  case Builtin::BI__builtin_nan:
    return BuiltinNaN(llvm::APFloat::IEEEdouble(), false, S, OpPC);
  case Builtin::BI__builtin_nansf:
    return BuiltinNaN(llvm::APFloat::IEEEsingle(), true, S, OpPC);
  case Builtin::BI__builtin_nanf:
    return BuiltinNaN(llvm::APFloat::IEEEsingle(), false, S, OpPC);
  case Builtin::BI__builtin_nansf128:
    return BuiltinNaN(llvm::APFloat::IEEEquad(), true, S, OpPC);
  case Builtin::BI__builtin_nanf128:
    return BuiltinNaN(llvm::APFloat::IEEEquad(), false, S, OpPC);
  case Builtin::BI__builtin_nansl:
    return BuiltinNaN(S.getASTCtx().getLongDoubleSemantics(), true, S, OpPC);
  case Builtin::BI__builtin_nanl:
    return BuiltinNaN(S.getASTCtx().getLongDoubleSemantics(), false, S, OpPC);

  case Builtin::BI__builtin_fabs:
  case Builtin::BI__builtin_fabsf:
  case Builtin::BI__builtin_fabsl:
  case Builtin::BI__builtin_fabsf128:
    return BuiltinFabs(S, OpPC);

  case Builtin::BI__builtin_clz:
    return CountLeadingZeros(S, OpPC, S.getIntType());
  case Builtin::BI__builtin_clzl:
    return CountLeadingZeros(S, OpPC, S.getLongType());
  case Builtin::BI__builtin_clzll:
    return CountLeadingZeros(S, OpPC, S.getLongLongType());
  case Builtin::BI__builtin_clzs:
    return CountLeadingZeros(S, OpPC, S.getShortType());


  case Builtin::BI__builtin_memcpy:
    return BuiltinMemCpy(S, OpPC);

  case Builtin::BI__builtin_is_constant_evaluated:
    return BuiltinIsConstantEvaluated(S, OpPC);
  }
}

} // namespace interp
} // namespace clang
