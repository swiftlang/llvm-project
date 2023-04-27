//===- InjectPointerSigningFixups.cpp - Inject arm64e ptr auth fixup code -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file injects a function to authenticate pointers for ARM64 JIT'd code.
//
//===----------------------------------------------------------------------===//

#include "InjectPointerSigningFixups.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalPtrAuthInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;

namespace {

struct PerModuleUtils {

  PerModuleUtils(Module &M) {
    // First grab some useful values from the context.
    Int32Ty = Type::getInt32Ty(M.getContext());
    IntPtrTy = Type::getInt64Ty(M.getContext());
    BlendIntrinsic = Intrinsic::getDeclaration(&M, Intrinsic::ptrauth_blend);

    SignIntrinsic = Intrinsic::getDeclaration(&M, Intrinsic::ptrauth_sign);

    // Then open the fixup function.
    FixupFunction = Function::Create(
        FunctionType::get(Type::getVoidTy(M.getContext()), false),
        GlobalValue::InternalLinkage, "lldb.arm64.sign_pointers", &M);
    FixupFunction->getBasicBlockList().push_back(
        BasicBlock::Create(M.getContext()));
    B = std::make_unique<IRBuilder<>>(&FixupFunction->back());
  };

  Type *Int32Ty = nullptr;
  Type *IntPtrTy = nullptr;
  Function *BlendIntrinsic = nullptr;
  Function *SignIntrinsic = nullptr;
  Function *FixupFunction = nullptr;
  std::unique_ptr<IRBuilder<>> B;
};

struct PerGlobalUtils {

  PerGlobalUtils(GlobalPtrAuthInfo PtrAuthInfo)
      : PtrAuthInfo(std::move(PtrAuthInfo)) {
    GlobalVariable *AuthGlobal =
        const_cast<GlobalVariable *>(this->PtrAuthInfo.getGV());
    Pointee = const_cast<Constant *>(
        this->PtrAuthInfo.getPointer()->stripPointerCasts());
    PtrForInsts = new GlobalVariable(
        *AuthGlobal->getParent(), Pointee->getType(), false,
        GlobalValue::PrivateLinkage,
        ConstantExpr::getPointerCast(AuthGlobal, Pointee->getType()));
  }

  Instruction *getPtrLoadFor(Function &F, Type &T) {
    auto Key = std::make_pair(&F, &T);
    auto LI = LoadsForCaller.find(Key);

    if (LI == LoadsForCaller.end()) {
      BasicBlock &Entry = F.getEntryBlock();
      if (&T == PtrForInsts->getType()) {
        for (auto &I : Entry)
          if (!isa<AllocaInst>(I)) {
            auto *Load =
                new LoadInst(cast<PointerType>(&T)->getPointerElementType(),
                             PtrForInsts, Twine(), &I);
            LI = LoadsForCaller.insert(std::make_pair(Key, Load)).first;
            break;
          }
      } else {
        auto *OriginalLoad = getPtrLoadFor(F, *PtrForInsts->getType());
        auto *Cast = new BitCastInst(OriginalLoad, &T, "",
                                     &*std::next(OriginalLoad->getIterator()));
        LI = LoadsForCaller.insert(std::make_pair(Key, Cast)).first;
      }
    }

    assert(LI != LoadsForCaller.end() && "No load available/added");
    return LI->second;
  }

  GlobalPtrAuthInfo PtrAuthInfo;
  Constant *Pointee = nullptr;
  GlobalVariable *PtrForInsts = nullptr;
  std::map<std::pair<Function *, Type *>, Instruction *> LoadsForCaller;
};

Error makeStringError(const char *Msg, Value *V) {
  std::string ErrString;
  raw_string_ostream ErrStringStream(ErrString);
  ErrStringStream << Msg << " '" << *V << "'";
  return make_error<StringError>(ErrStringStream.str(),
                                 inconvertibleErrorCode());
}

Value *createStructGEP(PerModuleUtils &MUtils, Value &Base,
                       std::vector<uint32_t> Idxs) {
  Idxs.push_back(0);
  std::vector<Value *> GEPArgs;
  GEPArgs.reserve(Idxs.size());
  while (!Idxs.empty()) {
    GEPArgs.push_back(ConstantInt::get(MUtils.Int32Ty, Idxs.back()));
    Idxs.pop_back();
  }
  return MUtils.B->CreateGEP(
      Base.getType()->getScalarType()->getPointerElementType(), &Base, GEPArgs);
}

void processGlobalVariable(PerModuleUtils &MUtils, PerGlobalUtils &GUtils,
                           GlobalVariable &V, std::vector<uint32_t> Idxs) {
  auto &B = *MUtils.B;
  Value *Discriminator =
      const_cast<ConstantInt *>(GUtils.PtrAuthInfo.getDiscriminator());
  Value *PtrLoc =
      Idxs.empty() ? &V : createStructGEP(MUtils, V, std::move(Idxs));

  // If this pointer has address diversity, then blend the discriminator
  // and the address.
  if (GUtils.PtrAuthInfo.hasAddressDiversity())
    Discriminator = B.CreateCall(
        MUtils.BlendIntrinsic,
        {B.CreatePointerCast(PtrLoc, MUtils.IntPtrTy), Discriminator});

  Value *RawPtr =
      B.CreateLoad(PtrLoc->getType()->getPointerElementType(), PtrLoc);
  Value *SignedPtr = B.CreateCall(
      MUtils.SignIntrinsic,
      {B.CreatePointerCast(RawPtr, MUtils.IntPtrTy),
       const_cast<ConstantInt *>(GUtils.PtrAuthInfo.getKey()), Discriminator});

  B.CreateStore(B.CreateBitOrPointerCast(
                    SignedPtr, PtrLoc->getType()->getPointerElementType()),
                PtrLoc);
}

void processInstruction(PerModuleUtils &MUtils, PerGlobalUtils &GUtils, Use &U,
                        std::vector<uint32_t> Idxs) {
  assert(Idxs.empty() &&
         "Accessing aggregate in instruction. Need a GEPExpr for this.");
  Instruction *V = cast<Instruction>(U.getUser());
  Function &F = *V->getParent()->getParent();
  Type *UseType = U.get()->getType();

  U.set(GUtils.getPtrLoadFor(F, *UseType));
}

Error processConstantExpr(PerModuleUtils &MUtils, PerGlobalUtils &GUtils,
                          Use &U, std::vector<uint32_t> Idxs) {
  // We can only handle constantexpr comparisons against null for now. This is
  // safe, since null will be signed to null, and non-null to non-null.
  auto *VExpr = cast<ConstantExpr>(U.getUser());
  auto OtherOpIdx = 1 - U.getOperandNo();
  if (!VExpr->isCompare() ||
      (VExpr->getPredicate() != CmpInst::ICMP_EQ &&
       VExpr->getPredicate() != CmpInst::ICMP_NE) ||
      VExpr->getNumOperands() != 2 ||
      !isa<Constant>(VExpr->getOperand(OtherOpIdx)) ||
      !cast<Constant>(VExpr->getOperand(OtherOpIdx))->isZeroValue())
    return makeStringError("Unsupported ptrauth constexpr: ", VExpr);

  // If this is a conforming constexpr then there's nothing to do. We can just
  // let the RAUW at the top level replace the ptrauth global with the
  // pointee.
  return Error::success();
}

Error processPtrAuthUsers(PerModuleUtils &MUtils, PerGlobalUtils &GUtils,
                          Use &U, std::vector<uint32_t> Idxs = {}) {
  Value *V = U.getUser();
  assert(V != nullptr);

  // Recurse through any casts.
  if (isa<ConstantExpr>(V) && cast<ConstantExpr>(V)->isCast()) {
    for (auto &U2 : V->uses())
      if (auto Err = processPtrAuthUsers(MUtils, GUtils, U2, Idxs))
        return Err;
  } else if (isa<ConstantAggregate>(V)) {
    Idxs.push_back(U.getOperandNo());
    for (auto &U2 : V->uses())
      if (auto Err = processPtrAuthUsers(MUtils, GUtils, U2, Idxs))
        return Err;
  } else if (isa<GlobalVariable>(V))
    processGlobalVariable(MUtils, GUtils, cast<GlobalVariable>(*V), Idxs);
  else if (isa<Instruction>(V))
    processInstruction(MUtils, GUtils, U, Idxs);
  else if (isa<ConstantExpr>(V)) {
    auto *VExpr = cast<ConstantExpr>(V);
    if (isa<GEPOperator>(VExpr)) {
      // We only support constant GEPs introduced when folding pointer casts.
      Type *VExprType = VExpr->getType();

      // Check that the types line up for a pointer cast.
      if (VExprType !=
          PointerType::get(GUtils.PtrAuthInfo.getPointer()->getType(), 0))
        return makeStringError("Type mismatch while rewriting ptrauth use", V);

      // Check that all indexes are constant zero.
      for (auto &Op :
           make_range(std::next(VExpr->op_begin()), VExpr->op_end())) {
        if (!isa<ConstantInt>(Op))
          return makeStringError("Cannot rewrite ptrauth use with non-constant "
                                 "indexes",
                                 Op);

        if (!cast<ConstantInt>(Op)->isZero())
          return makeStringError("Cannot rewrite ptrauth use with non-zero "
                                 "indexes",
                                 Op);
      }

      // Ok -- this is a supported constant GEP. Rewrite it.
      for (auto &U2 : VExpr->uses())
        if (auto Err = processPtrAuthUsers(MUtils, GUtils, U2, Idxs))
          return Err;
    } else
      return processConstantExpr(MUtils, GUtils, U, Idxs);
  } else
    return makeStringError("Unable to rewrite ptrauth use", V);

  return Error::success();
}

} // namespace

namespace lldb_private {

Error InjectPointerSigningFixupCode(llvm::Module &M,
                                    ExecutionPolicy execution_policy) {
  // If we cannot execute fixups, don't insert them.
  if (execution_policy == eExecutionPolicyNever)
    return Error::success();

  llvm::Triple T(M.getTargetTriple());

  // Bail out if we don't need pointer signing fixups.
  if (T.getSubArch() != Triple::AArch64SubArch_arm64e)
    return Error::success();

  PerModuleUtils MUtils(M);

  std::vector<GlobalVariable *> PtrAuthVarsToDelete;

  for (auto &G : M.globals()) {
    // We are only interested in globals in the ptrauth section.
    if (G.getSection() != "llvm.ptrauth")
      continue;

    // Add this ptrauth global to the list to be deleted.
    PtrAuthVarsToDelete.push_back(&G);

    // If this ptrauth global is unused then skip it, otherwise the fixup pass
    // could end up introducing a real use of it. Introducing a real use of an
    // unused external could result in errors if the unused external is
    // undefined whereas an unused, undefined external is benign.
    G.removeDeadConstantUsers();
    if (G.getNumUses() == 0)
      continue;

    // Analyze the ptr auth info and skip with a warning if it's invalid.
    auto PtrAuthInfo = GlobalPtrAuthInfo::tryAnalyze(&G);
    if (!PtrAuthInfo)
      return PtrAuthInfo.takeError();

    // Process all uses of this ptrauth global.
    PerGlobalUtils GUtils(std::move(*PtrAuthInfo));
    for (auto &U : G.uses())
      if (auto Err = processPtrAuthUsers(MUtils, GUtils, U))
        return Err;

    // Replace all uses of the ptrauth global with uses of the vanilla
    // (non-auth) global.
    G.replaceAllUsesWith(
        ConstantExpr::getPointerCast(GUtils.Pointee, G.getType()));
  }

  // Delete all the ptrauth globals.
  for (auto *G : PtrAuthVarsToDelete) {
    // Delete this ptrauth global. It should no longer be referenced.
    assert(G && G->user_empty() &&
           "All references to G should have bene dropped");
    G->eraseFromParent();
  }

  // If we never wrote any fixup code then just erase the fixup function and
  // bail out.
  if (MUtils.FixupFunction->getEntryBlock().empty()) {
    MUtils.FixupFunction->eraseFromParent();
    return Error::success();
  }

  // Close off the fixup function.
  MUtils.B->CreateRetVoid();

  // Update the global ctors list to call the poniter fixup function first.
  auto *UInt8PtrTy =
      PointerType::getUnqual(llvm::Type::getInt8Ty(M.getContext()));
  StructType *CtorType = StructType::get(
      M.getContext(),
      {MUtils.Int32Ty, MUtils.FixupFunction->getType(), UInt8PtrTy});
  Constant *PtrFixupCtor = ConstantStruct::get(
      CtorType, {ConstantInt::get(MUtils.Int32Ty, 0), MUtils.FixupFunction,
                 Constant::getNullValue(UInt8PtrTy)});

  const char *LLVMGlobalCtorsName = "llvm.global_ctors";
  GlobalVariable *OldCtorList = M.getNamedGlobal(LLVMGlobalCtorsName);
  std::vector<Constant *> CtorListArgs;
  CtorListArgs.push_back(PtrFixupCtor);

  if (OldCtorList) {

    // If the old ctors list has any uses then bail out: we do not know how to
    // rewrite them.
    if (OldCtorList->getNumUses() != 0)
      return makeStringError("Global ctors variable has users, so can not be "
                             "rewritten to include pointer fixups: ",
                             OldCtorList);

    for (auto &Op : OldCtorList->getInitializer()->operands())
      CtorListArgs.push_back(cast<Constant>(Op.get()));
  }

  ArrayType *CtorListType = ArrayType::get(CtorType, CtorListArgs.size());
  Constant *CtorListInit = ConstantArray::get(CtorListType, CtorListArgs);

  GlobalVariable *NewCtorList = new GlobalVariable(
      M, CtorListType, false, GlobalValue::AppendingLinkage, CtorListInit);

  if (OldCtorList) {
    NewCtorList->takeName(OldCtorList);
    OldCtorList->eraseFromParent();
  } else
    NewCtorList->setName(LLVMGlobalCtorsName);

  return Error::success();
}

} // namespace lldb_private
