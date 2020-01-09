//===--- BitCast.h - Implementation of bit casts ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Bit cast between unrelated types
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_OPCODES_BITCAST_H
#define LLVM_CLANG_AST_INTERP_OPCODES_BITCAST_H

template <PrimType TFrom, PrimType TTo>
bool BitCast(InterpState &S, CodePtr OpPC) {

  // Bitcasts to cv void* are static_casts, not reinterpret_casts, so are
  // permitted in constant expressions in C++11. Bitcasts from cv void* are
  // also static_casts, but we disallow them as a resolution to DR1312.
  if (TTo == PT_VoidPtr) {
    // Casts to void are allowed - preserve pointer.
    switch (TFrom) {
    default:
      llvm_unreachable("cannot cast to pointer");
    INTEGRAL_CASES({
      auto *CE = dyn_cast<CastExpr>(S.getSource(OpPC).asExpr());
      S.CCEDiag(CE, diag::note_constexpr_invalid_cast) << 2;
      auto Offset = CharUnits::fromQuantity((uint64_t)(S.Stk.pop<T>()));
      S.Stk.push<VoidPointer>(Offset);
      return true;
    })
    case PT_Ptr:
      S.Stk.push<VoidPointer>(S.Stk.pop<Pointer>());
      break;
    case PT_FnPtr:
      S.Stk.push<VoidPointer>(S.Stk.pop<FnPointer>());
      break;
    case PT_MemPtr:
      S.Stk.push<VoidPointer>(S.Stk.pop<MemberPointer>());
      break;
    case PT_ObjCBlockPtr:
      S.Stk.push<VoidPointer>(S.Stk.pop<ObjCBlockPointer>());
      break;
    case PT_VoidPtr:
      break;
    }
  } else {
    // Emit a warning since cast is not allowed.
    auto *CE = dyn_cast<CastExpr>(S.getSource(OpPC).asExpr());
    auto *SE = CE->getSubExpr();
    if (TFrom == PT_VoidPtr)
      S.CCEDiag(CE, diag::note_constexpr_invalid_cast) << 3 << SE->getType();
    else
      S.CCEDiag(CE, diag::note_constexpr_invalid_cast) << 2;

    // Returns a descriptor for the target type.
    auto GetDescriptor = [CE, &S] {
      auto *PtrTy = CE->getType()->getAs<PointerType>();
      assert(PtrTy && "not a pointer type");
      auto *Desc = S.P.createDescriptor(nullptr, PtrTy->getPointeeType());
      assert(Desc && "cannot create descriptor");
      return Desc;
    };

    // Try to create a correct or a dummy value.
    switch (TTo) {
    default:
      return false;

    // Convert some pointers to an integer offset.
    INTEGRAL_CASES({
      switch (TFrom) {
      default:
        return false;

      case PT_Ptr: {
        const auto &Ptr = S.Stk.pop<Pointer>();
        if (const TargetPointer *TargetPtr = Ptr.asTarget()) {
          S.Stk.push<T>(T::from(TargetPtr->getTargetOffset().getQuantity()));
          return true;
        }
        return false;
      }
      case PT_VoidPtr: {
        const auto &VoidPtr = S.Stk.pop<VoidPointer>();
        if (auto *Ptr = VoidPtr.asPointer()) {
          if (auto *TargetPtr = Ptr->asTarget()) {
            S.Stk.push<T>(T::from(TargetPtr->getTargetOffset().getQuantity()));
            return true;
          }
        }
        return false;
      }
      }
    })

    // Convert offsets to a pointer.
    case PT_Ptr: {
      switch (TFrom) {
      INTEGRAL_CASES({
        auto Offset = CharUnits::fromQuantity((uint64_t)S.Stk.pop<T>());
        S.Stk.push<Pointer>(GetDescriptor(), Offset);
        return true;
      })

      case PT_UintFP:
      case PT_SintFP:
      case PT_RealFP:
        return false;

      case PT_Ptr:
        return true;

      case PT_FnPtr:
        S.Stk.discard<FnPointer>();
        break;

      case PT_MemPtr:
        S.Stk.discard<MemberPointer>();
        break;

      case PT_VoidPtr: {
        const auto &VoidPtr = S.Stk.pop<VoidPointer>();
        if (auto *Ptr = VoidPtr.asPointer()) {
          const auto &Ctx = S.getASTCtx();
          if (auto *TargetPtr = Ptr->asTarget()) {
            // Target pointer retains the offset.
            S.Stk.push<Pointer>(GetDescriptor(), TargetPtr->getTargetOffset());
            return true;
          } else if (auto *BlockPtr = Ptr->asBlock()) {
            // Block pointer can be converted if types are compatible.
            if (!Ctx.hasSameType(BlockPtr->getFieldType(), CE->getType()))
              return false;
            S.Stk.push<Pointer>(*Ptr);
            return true;
          } else if (auto *ExternPtr = Ptr->asExtern()) {
            // Extern pointer changes to the new descriptor.
            if (!Ctx.hasSameType(ExternPtr->getFieldType(), CE->getType()))
              return false;
            S.Stk.push<Pointer>(*Ptr);
            return true;
          } else {
            // The invalid pointer is propagated.
            S.Stk.push<Pointer>(*Ptr);
            return true;
          }
        }
        return false;
      }

      case PT_ObjCBlockPtr:
        S.Stk.discard<ObjCBlockPointer>();
        break;
      }
      S.Stk.push<Pointer>(Pointer::Invalid{});
      break;
    }

    // Convert or create dummy pointer.
    case PT_FnPtr: {
      // Invalid cast - create an invalid pointer.
      if (TFrom != TTo) {
        TYPE_SWITCH(TFrom, S.Stk.discard<T>());
        S.Stk.push<FnPointer>(FnPointer::Invalid{});
        return true;
      }

      // If the types are compatible, pass the pointer through.
      // Otherwise, an invalid pointer is created.
      const auto &Ctx = S.getASTCtx();
      const ValueDecl *VD = S.Stk.peek<FnPointer>().asValueDecl();
      if (VD) {
        if (auto *PtrTy = dyn_cast<PointerType>(CE->getType())) {
          if (Ctx.hasSameType(VD->getType(), PtrTy->getPointeeType()))
            return true;
        } else if (auto *RefTy = dyn_cast<ReferenceType>(CE->getType())) {
          if (Ctx.hasSameType(VD->getType(), PtrTy->getPointeeType()))
            return true;
        }
      }
      S.Stk.discard<FnPointer>();
      S.Stk.push<FnPointer>(VD, FnPointer::Invalid{});
      return true;
    }

    case PT_MemPtr:
      if (TFrom != TTo) {
        TYPE_SWITCH(TFrom, S.Stk.discard<T>());
        S.Stk.push<MemberPointer>();
      }
      return true;

    case PT_VoidPtr:
      if (TFrom != TTo) {
        TYPE_SWITCH(TFrom, S.Stk.discard<T>());
        S.Stk.push<VoidPointer>();
      }
      return true;

    case PT_ObjCBlockPtr:
      if (TFrom != TTo) {
        TYPE_SWITCH(TFrom, S.Stk.discard<T>());
        S.Stk.push<ObjCBlockPointer>();
      }
      return true;
    }
  }

  return true;
}

#endif
