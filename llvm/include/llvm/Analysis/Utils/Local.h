//===- Local.h - Functions to perform local transformations -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This family of functions perform various local transformations to the
// program.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_UTILS_LOCAL_H
#define LLVM_ANALYSIS_UTILS_LOCAL_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"

namespace llvm {

class DataLayout;
class IRBuilderBase;
class User;
class Value;

/// Given a getelementptr instruction/constantexpr, emit the code necessary to
/// compute the offset from the base pointer (without adding in the base
/// pointer). Return the result as a signed integer of intptr size.
/// When NoAssumptions is true, no assumptions about index computation not
/// overflowing is made.

template <typename IRBuilderTy, class OpItTy>
std::pair<Value *, Type *>
EmitGEPOffset(IRBuilderTy *Builder, const DataLayout &DL,
              generic_gep_type_iterator<OpItTy> GTI, OpItTy IdxBegin,
              OpItTy IdxEnd, Type *GEPType, StringRef GEPName, bool IsInBounds,
              bool NoAssumptions = false) {
  Type *IntIdxTy = DL.getIndexType(GEPType);
  Value *Result = nullptr;

  // If the GEP is inbounds, we know that none of the addressing operations will
  // overflow in a signed sense.
  bool isInBounds = IsInBounds && !NoAssumptions;

  // Build a mask for high order bits.
  unsigned IntPtrWidth = IntIdxTy->getScalarType()->getIntegerBitWidth();
  uint64_t PtrSizeMask =
      std::numeric_limits<uint64_t>::max() >> (64 - IntPtrWidth);
  Type *IndexedType = nullptr;

  for (OpItTy i = IdxBegin, e = IdxEnd; i != e; ++i, ++GTI) {
    Value *Op = *i;
    IndexedType = GTI.getIndexedType();
    uint64_t Size = DL.getTypeAllocSize(IndexedType) & PtrSizeMask;
    Value *Offset;
    if (Constant *OpC = dyn_cast<Constant>(Op)) {
      if (OpC->isZeroValue())
        continue;

      // Handle a struct index, which adds its field offset to the pointer.
      if (StructType *STy = GTI.getStructTypeOrNull()) {
        uint64_t OpValue = OpC->getUniqueInteger().getZExtValue();
        IndexedType = STy->getElementType(OpValue);
        Size = DL.getStructLayout(STy)->getElementOffset(OpValue);
        if (!Size)
          continue;

        Offset = ConstantInt::get(IntIdxTy, Size);
      } else {
        // Splat the constant if needed.
        if (IntIdxTy->isVectorTy() && !OpC->getType()->isVectorTy())
          OpC = ConstantVector::getSplat(
              cast<VectorType>(IntIdxTy)->getElementCount(), OpC);

        Constant *Scale = ConstantInt::get(IntIdxTy, Size);
        Constant *OC = ConstantFoldIntegerCast(OpC, IntIdxTy, true, DL);
        Offset =
            ConstantExpr::getMul(OC, Scale, false /*NUW*/, isInBounds /*NSW*/);
      }
    } else {
      // Splat the index if needed.
      if (IntIdxTy->isVectorTy() && !Op->getType()->isVectorTy())
        Op = Builder->CreateVectorSplat(
            cast<FixedVectorType>(IntIdxTy)->getNumElements(), Op);

      // Convert to correct type.
      if (Op->getType() != IntIdxTy)
        Op = Builder->CreateIntCast(Op, IntIdxTy, true, Op->getName().str()+".c");
      if (Size != 1) {
        // We'll let instcombine(mul) convert this to a shl if possible.
        Op = Builder->CreateMul(Op, ConstantInt::get(IntIdxTy, Size),
                                GEPName.str() + ".idx", false /*NUW*/,
                                isInBounds /*NSW*/);
      }
      Offset = Op;
    }

    if (Result)
      Result = Builder->CreateAdd(Result, Offset, GEPName.str()+".offs",
                                  false /*NUW*/, isInBounds /*NSW*/);
    else
      Result = Offset;
  }
  return std::make_pair(Result ? Result : Constant::getNullValue(IntIdxTy),
                        IndexedType);
}

Value *emitGEPOffset(IRBuilderBase *Builder, const DataLayout &DL, User *GEP,
                     bool NoAssumptions = false);

} // namespace llvm

#endif // LLVM_ANALYSIS_UTILS_LOCAL_H
