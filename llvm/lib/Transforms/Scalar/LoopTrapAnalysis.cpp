//===-- LoopTrapAnalysis.cpp - Loop Trap Count pass -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/LoopTrapAnalysis.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Remarks/BoundsSafetyOptRemarks.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;
using namespace llvm::ore;
#define DEBUG_TYPE "loop-trap-analysis"
#define REMARK_PASS DEBUG_TYPE

enum class CheckLoopHoistType { MAYBE_CAN_HOIST, CANNOT_HOIST, SKIP };
static cl::opt<bool> BoundsSafetyTrapsOnly(
    "use-bounds-safety-traps-only", cl::init(false),
    cl::desc(
        "We only check for -fbounds-safety traps if the flag is false we can check "
        "for any hoistable traps."));

/// Max dominator depth from the entry block at which a trap-edge source BB is
/// still "entry-proximate" (plausible parameter validation). Populates the
/// `IsEntryProximate` field on each LoopTrapEdge record.
static cl::opt<unsigned> EntryProximityDepth(
    "loop-trap-entry-proximity-depth", cl::init(3),
    cl::desc("Maximum dominator depth from function entry at which a "
             "trap edge is classified IsEntryProximate (default 3)."));

/// Gate the explanatory trap analysis. When false (default), the pass emits
/// bit-identical YAML to a pre-framework snapshot. When true, it emits the
/// full explanation: the refined TrapClass classification (recognizes all
/// trap-like calls, splits stride variants, does not mask blocked edges) plus
/// the extra per-loop and per-edge fields (DominatesLatch / IV-update /
/// operand-class). Lets the framework be A/B compared and reverted via one
/// flag.
static cl::opt<bool> LTAEmitExplain(
    "loop-trap-analysis-explain", cl::init(false),
    cl::desc("Emit the explanatory trap analysis: the refined TrapClass "
             "classification plus per-loop and per-edge explanatory fields "
             "(DominatesLatch / IV-update / operand-class). When false "
             "(default), only the pre-explanation fields and compact TrapClass "
             "are emitted, so the framework can be A/B compared and reverted "
             "by toggling this flag alone."));

static cl::opt<bool> LTAEmitLoadAlias(
    "loop-load-alias", cl::init(false),
    cl::desc("Opt-in: for each load in an INNERMOST loop that is may-clobbered "
             "by an in-loop writer (store / memcpy / call), emit a "
             "LoopLoadAlias record naming the first such writer. Clobbered "
             "loads only (hoistable loads are omitted). Off by default; "
             "enable selectively as it emits one record per clobbered load."));

/// Legacy (pre-unification) trap-block predicate variants, reproduced
/// byte-for-byte when `-loop-trap-analysis-explain` is off so existing output /
/// lit tests are unchanged:
///   AnyUnreachableBoundsSafety - any `unreachable`, filtered by the
///       -fbounds-safety annotation when BoundsSafetyOnly.
///   AnyUnreachable - any `unreachable`, no annotation filter.
///   TrapIntrinsic  - `unreachable` preceded by `call @llvm.trap()` only.
enum class LegacyTrapMatch {
  AnyUnreachableBoundsSafety,
  AnyUnreachable,
  TrapIntrinsic
};

/// Single source of truth for "is BB a trap block". With
/// `-loop-trap-analysis-explain` (opt-in), all callers agree: BB ends in
/// `unreachable` immediately preceded by `@llvm.trap()` or `@llvm.ubsantrap()`
/// (Swift -fbounds-safety lowers to ubsantrap), with a consistent bounds-safety
/// annotation filter. When the flag is off (default), each caller's original
/// predicate (\p Legacy) is reproduced exactly, so emitted records do not
/// change.
static bool isTrapBlock(BasicBlock *BB, bool BoundsSafetyOnly,
                        LegacyTrapMatch Legacy) {
  if (!BB || BB->empty())
    return false;
  Instruction *Term = BB->getTerminator();
  if (!isa<UnreachableInst>(Term))
    return false;
  if (LTAEmitExplain) {
    // Unified: bounds-safety filter + a trap-like terminating call, identified
    // by its semantic property rather than an intrinsic allowlist: a
    // `noreturn` call touching only inaccessible memory (the shared property of
    // @llvm.trap / @llvm.ubsantrap / @llvm.looptrap and any future trap
    // intrinsic).
    if (BoundsSafetyOnly && !isBoundsSafetyAnnotated(Term))
      return false;
    if (Term == &BB->front())
      return false;
    if (auto *CI = dyn_cast<CallInst>(Term->getPrevNode()))
      return CI->doesNotReturn() && CI->onlyAccessesInaccessibleMemory();
    return false;
  }
  // Legacy: reproduce each caller's original predicate byte-for-byte.
  switch (Legacy) {
  case LegacyTrapMatch::AnyUnreachableBoundsSafety:
    return !BoundsSafetyOnly || isBoundsSafetyAnnotated(Term);
  case LegacyTrapMatch::AnyUnreachable:
    return true;
  case LegacyTrapMatch::TrapIntrinsic:
    if (Term == &BB->front())
      return false;
    if (auto *CI = dyn_cast<CallInst>(Term->getPrevNode()))
      if (Function *Callee = CI->getCalledFunction())
        return Callee->getIntrinsicID() == Intrinsic::trap;
    return false;
  }
  return false;
}

/// Check for an unreachable instruction that has an edge to any of \p L basic
/// blocks. if `--use-bounds-safety-traps-only` is used make sure that the trap and
/// branch instructions have -fbounds-safety annotation.
static bool hasUnreachableInst(Loop *L) {
  SmallVector<BasicBlock *, 4> LoopExitBlocks;
  L->getExitBlocks(LoopExitBlocks);
  for (auto *BB : LoopExitBlocks) {
    // Trap exit block (shared predicate, honoring
    // --use-bounds-safety-traps-only).
    if (!isTrapBlock(BB, BoundsSafetyTrapsOnly,
                     LegacyTrapMatch::AnyUnreachableBoundsSafety))
      continue;
    if (any_of(predecessors(BB), [L](BasicBlock *PredB) {
          auto *TerminatorInst = PredB->getTerminator();
          return L->contains(PredB) &&
                 (isa<CondBrInst>(TerminatorInst) ||
                  isa<UncondBrInst>(TerminatorInst) ||
                  isa<SwitchInst>(TerminatorInst)) &&
                 (!BoundsSafetyTrapsOnly ||
                  isBoundsSafetyAnnotated(TerminatorInst));
        }))
      return true;
  }
  return false;
}

static unsigned countTrapExits(Loop *L, LoopInfo &LI) {
  unsigned Count = 0;
  SmallVector<BasicBlock *, 4> LoopExitBlocks;
  L->getExitBlocks(LoopExitBlocks);
  for (auto *BB : LoopExitBlocks) {
    if (!isTrapBlock(BB, BoundsSafetyTrapsOnly,
                     LegacyTrapMatch::AnyUnreachableBoundsSafety))
      continue;
    // Strict attribution: count this trap exit only if it has a predecessor
    // whose immediate containing loop is L (not a nested sub-loop). Otherwise
    // an inner trap's unreachable target -- in every enclosing loop's exit set
    // -- would be counted at every nest level.
    if (any_of(predecessors(BB), [L, &LI](BasicBlock *PredB) {
          if (LI.getLoopFor(PredB) != L)
            return false;
          auto *TerminatorInst = PredB->getTerminator();
          return L->contains(PredB) &&
                 (isa<CondBrInst>(TerminatorInst) ||
                  isa<UncondBrInst>(TerminatorInst) ||
                  isa<SwitchInst>(TerminatorInst)) &&
                 (!BoundsSafetyTrapsOnly ||
                  isBoundsSafetyAnnotated(TerminatorInst));
        }))
      ++Count;
  }
  return Count;
}

/// Count conditional branches inside the loop body whose target is a trap
/// block (BB ending in `unreachable`, optionally preceded by
/// `tail call @llvm.trap()`). Captures per-iteration trap-direction branches.
///
/// Strict attribution: only edges originating in this loop's own body (not in
/// any sub-loop). Since `L->blocks()` transitively includes sub-loop blocks,
/// without this filter an inner-loop trap would be double-counted by every
/// enclosing loop's record.
static unsigned countCondTrapEdges(Loop *L, LoopInfo &LI) {
  unsigned Count = 0;
  // Trap block: an unreachable BB whose only side effect is llvm.trap (or bare
  // unreachable).
  auto IsTrapBlock = [](BasicBlock *BB) {
    return isTrapBlock(BB, BoundsSafetyTrapsOnly,
                       LegacyTrapMatch::AnyUnreachableBoundsSafety);
  };
  for (auto *BB : L->blocks()) {
    // Strict attribution: only edges whose source BB's immediate containing
    // loop is L. Edges in nested sub-loops belong to those inner loops.
    if (LI.getLoopFor(BB) != L)
      continue;
    auto *Term = BB->getTerminator();
    auto *BI = dyn_cast<CondBrInst>(Term);
    if (!BI)
      continue;
    for (BasicBlock *Succ : BI->successors()) {
      if (L->contains(Succ))
        continue;
      if (IsTrapBlock(Succ)) {
        if (BoundsSafetyTrapsOnly && !isBoundsSafetyAnnotated(Term))
          continue;
        ++Count;
      }
    }
  }
  return Count;
}

/// Count cond-trap edges whose condition is loop-invariant — i.e. LICM could
/// hoist them out but hasn't. Estimates the residual hoisting opportunity.
static unsigned countHoistableCondTrapEdges(Loop *L, LoopInfo &LI) {
  unsigned Count = 0;
  auto IsTrapBlock = [](BasicBlock *BB) {
    return isTrapBlock(BB, BoundsSafetyTrapsOnly,
                       LegacyTrapMatch::AnyUnreachable);
  };
  for (auto *BB : L->blocks()) {
    // Strict attribution: only edges originating in L's own body. Same rule
    // as countCondTrapEdges.
    if (LI.getLoopFor(BB) != L)
      continue;
    auto *Term = BB->getTerminator();
    auto *BI = dyn_cast<CondBrInst>(Term);
    if (!BI)
      continue;
    bool HasTrapSucc = false;
    for (BasicBlock *Succ : BI->successors()) {
      if (L->contains(Succ))
        continue;
      if (IsTrapBlock(Succ)) {
        HasTrapSucc = true;
        break;
      }
    }
    if (!HasTrapSucc)
      continue;
    if (L->isLoopInvariant(BI->getCondition()))
      ++Count;
  }
  return Count;
}

/// Classify cond-trap edges by the shape of their controlling condition.
/// Returns (invariant, iv_derived, non_iv) counts:
///   invariant:  whole condition is loop-invariant (LICM/Unswitch territory).
///   iv_derived: at least one icmp operand has an AddRec SCEV on this loop
///               (IndVars PredicatedExit / partial-BTC could help).
///   non_iv:     condition varies per-iter but no operand is an AddRec on L —
///               can't hoist (varies), can't predicate (no recurrence).
struct TrapCondShape {
  unsigned Invariant = 0;
  unsigned IVDerived = 0;
  unsigned NonIV = 0;
  // Cross-product of {DominatesLatch, NotDominatingLatch} ×
  // {Invariant, IVDerived, NonIV}, partitioning all cond-trap edges:
  //   NotDominatingLatch_*      : cond is in a conditional arm that doesn't
  //                               post-dominate the body; SCEV can't compute
  //                               trip count downstream, can't hoist here.
  //   DominatesLatch_Invariant  : fires every iteration and is invariant. LICM.
  //   DominatesLatch_IVDerived  : every iteration, has an L-AddRec operand.
  //                               SCEV / IndVars attackable.
  //   DominatesLatch_NonIV      : every iteration but no invariant/affine
  //                               operand. Varying yet uncomputable -- needs
  //                               AA/TBAA refinement or source change.
  unsigned DominatesLatch = 0;
  unsigned NotDominatingLatch = 0;
  unsigned DLInvariant = 0, DLIVDerived = 0, DLNonIV = 0;
  unsigned NDLInvariant = 0, NDLIVDerived = 0, NDLNonIV = 0;

  // Sub-classification of NonIV -- *why* the operands aren't affine. Flags are
  // non-exclusive and bounded by NonIV. Explains SCEV trap-elimination failure
  // (SCEV needs every operand invariant or affine).
  //   NonIVLoadOp   : operand chain reaches an in-loop LoadInst. Lever: TBAA /
  //                   AA refinement so LICM can hoist the load.
  //   NonIVPhiOp    : operand chain reaches a non-AddRec in-loop PHI (header
  //                   phi with conditional update, or merge phi). Lever:
  //                   source restructure.
  //   NonIVSelectOp : operand chain reaches an in-loop SelectInst / FreezeInst
  //                   (often from control-flow flattening). Same lever as Phi.
  //   NonIVCallOp   : an operand is a call result. SCEV treats as Unknown.
  unsigned NonIVLoadOp = 0;
  unsigned NonIVPhiOp = 0;
  unsigned NonIVSelectOp = 0;
  unsigned NonIVCallOp = 0;
};
static TrapCondShape classifyCondTrapEdges(Loop *L, ScalarEvolution &SE,
                                           const DominatorTree &DT) {
  TrapCondShape S;
  BasicBlock *Latch = L->getLoopLatch();
  auto IsTrapBlock = [](BasicBlock *BB) {
    return isTrapBlock(BB, BoundsSafetyTrapsOnly,
                       LegacyTrapMatch::AnyUnreachable);
  };
  for (auto *BB : L->blocks()) {
    auto *Term = BB->getTerminator();
    auto *BI = dyn_cast<CondBrInst>(Term);
    if (!BI)
      continue;
    bool HasTrapSucc = false;
    for (BasicBlock *Succ : BI->successors()) {
      if (L->contains(Succ))
        continue;
      if (IsTrapBlock(Succ)) {
        HasTrapSucc = true;
        break;
      }
    }
    if (!HasTrapSucc)
      continue;

    // Dominance dimension: if the trap branch's parent BB dominates the latch,
    // the cond evaluates every iteration; otherwise it's in a conditional arm
    // and SCEV cannot compute trip counts past it.
    bool DominatesLatch = Latch && DT.dominates(BB, Latch);
    if (DominatesLatch)
      ++S.DominatesLatch;
    else
      ++S.NotDominatingLatch;

    Value *Cond = BI->getCondition();
    if (L->isLoopInvariant(Cond)) {
      ++S.Invariant;
      if (DominatesLatch)
        ++S.DLInvariant;
      else
        ++S.NDLInvariant;
      continue;
    }
    auto *Cmp = dyn_cast<ICmpInst>(Cond);
    bool FoundAddRec = false;
    auto Inspect = [&](Value *V) {
      if (L->isLoopInvariant(V))
        return;
      if (!SE.isSCEVable(V->getType()))
        return;
      const SCEV *SC = SE.getSCEV(V);
      if (auto *AR = dyn_cast<SCEVAddRecExpr>(SC))
        if (AR->getLoop() == L)
          FoundAddRec = true;
    };
    if (Cmp) {
      Inspect(Cmp->getOperand(0));
      Inspect(Cmp->getOperand(1));
    } else {
      Inspect(Cond);
    }
    if (FoundAddRec) {
      ++S.IVDerived;
      if (DominatesLatch)
        ++S.DLIVDerived;
      else
        ++S.NDLIVDerived;
    } else {
      ++S.NonIV;
      if (DominatesLatch)
        ++S.DLNonIV;
      else
        ++S.NDLNonIV;

      // Sub-classify NonIV: walk the cond's operand chain (in-loop,
      // depth-bounded) and tag which operand classes appear. Flags are
      // non-exclusive. The walk is shallow (no header-phi recurrence) so it
      // stays O(loop body) without chain explosion.
      SmallPtrSet<const Value *, 32> Seen;
      SmallVector<const Value *, 16> Stack;
      auto Push = [&](Value *V) { Stack.push_back(V); };
      if (Cmp) {
        Push(Cmp->getOperand(0));
        Push(Cmp->getOperand(1));
      } else {
        Push(Cond);
      }
      bool HasLoad = false, HasPhi = false, HasSelect = false, HasCall = false;
      unsigned Steps = 0;
      const unsigned StepLimit = 64;
      while (!Stack.empty() && Steps++ < StepLimit) {
        const Value *V = Stack.pop_back_val();
        if (!Seen.insert(V).second)
          continue;
        const auto *I = dyn_cast<Instruction>(V);
        if (!I || !L->contains(I->getParent()))
          continue;
        if (isa<LoadInst>(I))
          HasLoad = true;
        else if (isa<PHINode>(I))
          HasPhi = true;
        else if (isa<SelectInst>(I) || isa<FreezeInst>(I))
          HasSelect = true;
        else if (isa<CallInst>(I))
          HasCall = true;
        // Don't recurse through phi operands (avoids cycle chain explosion);
        // 'phi' is already tagged. Recurse through anything else.
        if (!isa<PHINode>(I))
          for (const Use &U : I->operands())
            Stack.push_back(U.get());
      }
      if (HasLoad)
        ++S.NonIVLoadOp;
      if (HasPhi)
        ++S.NonIVPhiOp;
      if (HasSelect)
        ++S.NonIVSelectOp;
      if (HasCall)
        ++S.NonIVCallOp;
    }
  }
  return S;
}

// Emit one or more side-effect tags identifying the *class* of instruction
// that blocks LICM. Tags use the structured format
//
//   <Category>.<cause>[: <callee>]
//
// where <Category> is the instruction class (Store / AtomicStore /
// VolatileStore / AtomicLoad / VolatileLoad / AtomicRMW / AtomicCmpXchg /
// Fence / Other / Call / MemIntrinsic / VolatileCall) and <cause> is one of
// {may-write-to-memory, may-read-from-memory, may-access-memory, may-throw,
// may-not-return}. Optional ": <callee>" disambiguates calls. Splitting by
// class (vs the prior two opaque buckets) shows which lever -- TBAA,
// function-attr annotations, per-intrinsic AA -- would unblock a given loop.
static std::string getSideEffectReasons(const Instruction &I) {
  std::string Buf;
  raw_string_ostream OS(Buf);

  // CallInst class: distinguish intrinsics from user calls, split by memory
  // effect. AA models memcpy/memset ModRefInfo well; opaque user calls not.
  if (const auto *CI = dyn_cast<CallInst>(&I)) {
    StringRef CalleeName;
    if (const Function *Callee = CI->getCalledFunction())
      CalleeName = Callee->getName();
    bool IsIntrinsic = CI->getIntrinsicID() != Intrinsic::not_intrinsic;
    StringRef CategoryPrefix = IsIntrinsic ? "MemIntrinsic" : "Call";

    // argmemonly memintrinsics (memcpy/memset/memmove) only touch their
    // pointer arguments, which AA already models precisely, so a generic
    // memory-effect tag would mis-suggest they are blockers. Skip the
    // may-{write,read}-memory tags for them; throw / no-return still emit
    // (orthogonal to memory).
    bool SkipMemoryTags =
        IsIntrinsic && CI->getMemoryEffects().onlyAccessesArgPointees();

    if (CI->isVolatile())
      OS << "VolatileCall.may-access-memory\n";
    if (!SkipMemoryTags) {
      if (I.mayWriteToMemory()) {
        OS << CategoryPrefix << ".may-write-to-memory";
        if (!CalleeName.empty())
          OS << ": " << CalleeName;
        OS << "\n";
      } else if (I.mayReadFromMemory()) {
        OS << CategoryPrefix << ".may-read-from-memory";
        if (!CalleeName.empty())
          OS << ": " << CalleeName;
        OS << "\n";
      }
    }
    if (I.mayThrow())
      OS << CategoryPrefix << ".may-throw\n";
    if (!I.willReturn())
      OS << CategoryPrefix << ".may-not-return\n";
    return Buf;
  }

  // Non-call instructions: split by opcode so the metric distinguishes
  // a real store from atomic RMW / cmpxchg / fence / volatile load.
  if (auto *SI = dyn_cast<StoreInst>(&I)) {
    if (SI->isVolatile())
      OS << "VolatileStore.may-write-to-memory\n";
    else if (SI->isAtomic())
      OS << "AtomicStore.may-write-to-memory\n";
    else
      OS << "Store.may-write-to-memory\n";
  } else if (auto *LI = dyn_cast<LoadInst>(&I)) {
    if (LI->isVolatile())
      OS << "VolatileLoad.may-write-to-memory\n";
    else if (LI->isAtomic())
      OS << "AtomicLoad.may-write-to-memory\n";
    // Plain load: mayWriteToMemory() is false — nothing to emit.
  } else if (isa<AtomicRMWInst>(&I)) {
    OS << "AtomicRMW.may-write-to-memory\n";
  } else if (isa<AtomicCmpXchgInst>(&I)) {
    OS << "AtomicCmpXchg.may-write-to-memory\n";
  } else if (isa<FenceInst>(&I)) {
    OS << "Fence.may-write-to-memory\n";
  } else if (I.mayWriteToMemory()) {
    // Catch-all for any other non-call write (rare).
    OS << "Other.may-write-to-memory\n";
  }

  if (I.mayThrow())
    OS << "Instruction.may-throw\n";
  if (!I.willReturn())
    OS << "Instruction.may-not-return\n";
  return Buf;
}

/// Check if the loop preheader has ".hoisted" instructions, indicating
/// successful hoisting of trap checks out of the loop.
static bool hasHoistedInPreheader(Loop *L,
                                  SmallVectorImpl<std::string> &HoistedInsts) {
  BasicBlock *Preheader = L->getLoopPreheader();
  if (!Preheader)
    return false;
  for (auto &I : *Preheader) {
    if (I.hasName() && I.getName().contains(".hoisted")) {
      std::string Buf;
      raw_string_ostream OS(Buf);
      I.print(OS);
      HoistedInsts.push_back(Buf);
    }
  }
  return !HoistedInsts.empty();
}

/// Build a remark Name suffixed with the optional Tag (so the same pass can
/// be invoked multiple times in a pipeline with distinguishable output).
static std::string taggedName(StringRef Base, StringRef Tag) {
  if (Tag.empty())
    return Base.str();
  return (Base + Tag).str();
}

/// Print a stable, non-empty label for \p BB, so remark args that identify
/// BasicBlocks stay useful when the BB has no source-level name (numeric IR,
/// stripped names, or non-C frontends such as swiftc's IRGen).
///
/// Preferred: `BB->getName()`. Fallback: `printAsOperand` slot-tracker form
/// (`%5` etc.), which is parseable and unique within the function. Never
/// returns empty.
static std::string bbLabel(const BasicBlock *BB) {
  if (!BB)
    return "<null>";
  if (BB->hasName())
    return BB->getName().str();
  std::string S;
  raw_string_ostream OS(S);
  BB->printAsOperand(OS, /*PrintType=*/false);
  return S.empty() ? std::string("<unnamed>") : S;
}

/// Recursively unfold a boolean expression chain (select-OR, select-AND,
/// or/and) into the leaf comparison operands. Used to surface every
/// concrete operand of a trap predicate so we can classify each one.
static void collectBoolLeafOperands(Value *V,
                                    SmallVectorImpl<Value *> &Operands,
                                    SmallPtrSetImpl<Value *> &Visited,
                                    int Depth = 0) {
  if (!V || !Visited.insert(V).second || Depth > 8)
    return;
  if (auto *SI = dyn_cast<SelectInst>(V)) {
    Value *T = SI->getTrueValue(), *FV = SI->getFalseValue();
    if (auto *TC = dyn_cast<ConstantInt>(T))
      if (TC->isOne()) {
        collectBoolLeafOperands(SI->getCondition(), Operands, Visited,
                                Depth + 1);
        collectBoolLeafOperands(FV, Operands, Visited, Depth + 1);
        return;
      }
    if (auto *FC = dyn_cast<ConstantInt>(FV))
      if (FC->isZero()) {
        collectBoolLeafOperands(SI->getCondition(), Operands, Visited,
                                Depth + 1);
        collectBoolLeafOperands(T, Operands, Visited, Depth + 1);
        return;
      }
  }
  if (auto *BO = dyn_cast<BinaryOperator>(V)) {
    if (BO->getOpcode() == Instruction::Or ||
        BO->getOpcode() == Instruction::And) {
      collectBoolLeafOperands(BO->getOperand(0), Operands, Visited, Depth + 1);
      collectBoolLeafOperands(BO->getOperand(1), Operands, Visited, Depth + 1);
      return;
    }
  }
  if (auto *Cmp = dyn_cast<CmpInst>(V)) {
    Operands.push_back(Cmp->getOperand(0));
    Operands.push_back(Cmp->getOperand(1));
    return;
  }
  // Opaque — record as itself.
  Operands.push_back(V);
}

/// SCEV traversal helper: collects every SCEVUnknown and SCEVAddRecExpr
/// node reachable from a SCEV expression. We use the canonical
/// `SCEVTraversal<>` machinery — no string parsing, no regex.
namespace {
struct SCEVNodeCollector {
  SmallPtrSet<const SCEVUnknown *, 8> Unknowns;
  SmallPtrSet<const SCEVAddRecExpr *, 4> AddRecs;
  bool follow(const SCEV *S) {
    if (auto *U = dyn_cast<SCEVUnknown>(S))
      Unknowns.insert(U);
    if (auto *AR = dyn_cast<SCEVAddRecExpr>(S))
      AddRecs.insert(AR);
    return true; // keep walking
  }
  bool isDone() const { return false; }
};
} // anonymous namespace

/// Structural shape of a trap branch's i1 predicate. Sizes the reach of
/// compiler-side fix opportunities (bounds-check OR/AND forms with a constant
/// or variable bound, and the generic OR-of-AddRec exit recognition).
/// Independent of whether the source BB sits inside a loop.
namespace {
enum class TrapPredicateShape {
  Unknown,                  ///< Could not classify (null cond, etc.).
  SingleICmp,               ///< Single icmp (NumLeafOps <= 2).
  OrBoundsCheckConstBound,  ///< or(uge(X, B), ult(sub(B, X), K_const))
  AndBoundsCheckConstBound, ///< and(ult(X, B), uge(sub(B, X), K_const))
  OrBoundsCheckVarBound,    ///< or(uge(X, B), ult(sub(B, X), K_addrec))
  AndBoundsCheckVarBound,   ///< and(ult(X, B), uge(sub(B, X), K_addrec))
  OrTwoAddRecICmp,          ///< or(icmp(AR_a), icmp(AR_b)) — no sub-arith
  AndTwoAddRecICmp,         ///< and(icmp(AR_a), icmp(AR_b)) — no sub-arith
  OtherMulti,               ///< NumLeafOps >= 4, no recognized structure
};
} // anonymous namespace

static StringRef trapPredicateShapeName(TrapPredicateShape S) {
  switch (S) {
  case TrapPredicateShape::Unknown:
    return "Unknown";
  case TrapPredicateShape::SingleICmp:
    return "SingleICmp";
  case TrapPredicateShape::OrBoundsCheckConstBound:
    return "OrBoundsCheck-ConstBound";
  case TrapPredicateShape::AndBoundsCheckConstBound:
    return "AndBoundsCheck-ConstBound";
  case TrapPredicateShape::OrBoundsCheckVarBound:
    return "OrBoundsCheck-VarBound";
  case TrapPredicateShape::AndBoundsCheckVarBound:
    return "AndBoundsCheck-VarBound";
  case TrapPredicateShape::OrTwoAddRecICmp:
    return "OrTwoAddRecICmp";
  case TrapPredicateShape::AndTwoAddRecICmp:
    return "AndTwoAddRecICmp";
  case TrapPredicateShape::OtherMulti:
    return "OtherMulti";
  }
  llvm_unreachable("unhandled TrapPredicateShape");
}

/// Compute the count-ordered, descriptive trap-class name for one trap edge
/// from its per-edge fields. Single source of truth for a classification
/// consumers previously re-derived in Python (gen_ir_view.priority_class);
/// emitted as the `TrapClass` field, and its precedence must stay in lock-step
/// with that fallback. Returned strings are static literals (safe as
/// StringRef).
static StringRef computeTrapClass(
    bool InLoop, TrapPredicateShape Shape, bool IsEntryProx, bool DomByEquiv,
    bool IsLoopExit, bool EdgeBTCComputable, bool LoopOtherUnk,
    bool StoreReload, bool MemIReload, bool CallReload, bool InLoopPhi,
    bool UnaliasLoad, bool OtherUnk, bool OpaqueNoUnk, bool InLoopFreeze,
    bool InLoopSelect, unsigned NumLeafOps, bool NonUnitStride, bool NegStride,
    bool NonConstStride, bool NotProvenMonotonicOnly) {
  const bool ConstK = Shape == TrapPredicateShape::OrBoundsCheckConstBound ||
                      Shape == TrapPredicateShape::AndBoundsCheckConstBound;
  const bool VarK = Shape == TrapPredicateShape::OrBoundsCheckVarBound ||
                    Shape == TrapPredicateShape::AndBoundsCheckVarBound;
  const bool TwoAR = Shape == TrapPredicateShape::OrTwoAddRecICmp ||
                     Shape == TrapPredicateShape::AndTwoAddRecICmp;
  // The leading token is the index-shape axis -- how well the compiler can
  // model the trapping index: `Invariant-` = not inside any loop (one-shot /
  // hoistable check); `Affine-` = the index is an affine AddRec over the loop
  // IV (understood; eliminability turns on trip count / no-wrap); `Opaque-` =
  // the index depends on an in-loop load / phi / call / opaque value SCEV can't
  // model. `InLoop-NonExit` carries no index-shape axis. The rest of each name
  // is unchanged.
  // Out-of-loop: classify by structural redundancy / predicate shape.
  if (!InLoop) {
    if (IsEntryProx)
      return "Invariant-OutsideLoop-EntryProximate";
    if (DomByEquiv)
      return "Invariant-OutsideLoop-RedundantWithDominatingCheck";
    if (ConstK)
      return "Invariant-OutsideLoop-MultiComparison-ConstBound";
    if (VarK)
      return "Invariant-OutsideLoop-MultiComparison-VarBound";
    if (Shape == TrapPredicateShape::SingleICmp)
      return "Invariant-OutsideLoop-SingleComparison";
    if (Shape == TrapPredicateShape::OtherMulti)
      return "Invariant-OutsideLoop-MultiComparison-Other";
    if (TwoAR)
      return "Invariant-OutsideLoop-MultiComparison-Other";
    return "Invariant-OutsideLoop-Unclassifiable";
  }
  // In-loop, not a loop exit.
  if (!IsLoopExit)
    return "InLoop-NonExit";
  // In-loop loop-exit.
  // A BTC-computable edge whose predicate also has a reload/opaque blocker
  // isn't cleanly eliminable via trip count, so under LTAEmitExplain it is NOT
  // masked as a trip-count-known class -- it falls through to the blocker
  // classes below. Legacy (flag off) keeps the trip-count-known classes for any
  // BTC-computable edge (byte-identical).
  const bool HasBlocker = StoreReload || MemIReload || CallReload ||
                          InLoopPhi || UnaliasLoad || OtherUnk || OpaqueNoUnk;
  if (EdgeBTCComputable && (!LTAEmitExplain || !HasBlocker))
    return LoopOtherUnk
               ? "Affine-InLoopExit-TripCountKnown-LoopBlockedOtherTrapExit"
               : "Affine-InLoopExit-TripCountKnown";
  const bool S = StoreReload || MemIReload;
  if (S && CallReload)
    return "Opaque-InLoopExit-TripCountUnknown-StoreAndCallReload";
  if (S)
    return "Opaque-InLoopExit-TripCountUnknown-StoreReload";
  if (CallReload)
    return "Opaque-InLoopExit-TripCountUnknown-CallReload";
  if (InLoopPhi)
    return "Opaque-InLoopExit-TripCountUnknown-InLoopPhiOperand";
  if (UnaliasLoad)
    return "Opaque-InLoopExit-TripCountUnknown-InLoopLoadOperand";
  // The opaque-operand class split by opacity shape (freeze / select / other
  // in-loop unknown). InLoopFreeze / InLoopSelect are subsets of OtherUnk, so
  // test them first.
  if (OtherUnk) {
    if (InLoopFreeze)
      return "Opaque-InLoopExit-TripCountUnknown-OpaqueOperand-Freeze";
    if (InLoopSelect)
      return "Opaque-InLoopExit-TripCountUnknown-OpaqueOperand-Select";
    return "Opaque-InLoopExit-TripCountUnknown-OpaqueOperand-Other";
  }
  if (OpaqueNoUnk)
    return "Opaque-InLoopExit-TripCountUnknown-OpaqueOperand-NoInLoopUnknown";
  // The multi-comparison suffix is driven by the predicate SHAPE (one source of
  // truth), so the multi-comparison class can't disagree with the emitted
  // PredicateShape. Legacy (flag off) keeps the NumLeafOps>=4 gate for
  // byte-identical output.
  bool MultiShape = LTAEmitExplain ? (ConstK || VarK || TwoAR ||
                                      Shape == TrapPredicateShape::OtherMulti)
                                   : (NumLeafOps >= 4);
  if (MultiShape) {
    if (ConstK)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "MultiComparison-ConstBound";
    if (VarK)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "MultiComparison-VarBound";
    if (TwoAR)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "MultiComparison-TwoAddRec";
    if (Shape == TrapPredicateShape::OtherMulti)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "MultiComparison-Other";
  }
  // Surface stride-fragility (the Has*StrideForLAddRec flags previously never
  // influenced the class) instead of lumping it into the bare weak-no-wrap
  // class. Legacy (flag off) returns the bare weak-no-wrap class
  // (byte-identical).
  if (LTAEmitExplain) {
    if (NonConstStride)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "NonConstantStride";
    if (NonUnitStride)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "NonUnitStride";
    if (NegStride)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "NegativeStride";
    if (NotProvenMonotonicOnly)
      return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-"
             "NotProvenMonotonicOnly";
  }
  return "Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic";
}

/// Match the bounded-iterator OR / AND shape:
///   OR  form: or (uge X, B)  (ult (sub B, X), K)
///   AND form: and(ult X, B)  (uge (sub B, X), K)
/// Returns true iff one arm ordering matches the requested (predicate-pair,
/// K-kind) tuple. K-kind is constant for the *ConstK shapes and an L-affine
/// SCEVAddRecExpr for the *VarK shapes.
///
/// icmp operand-swap is not attempted: clang's post-SROA/SimplifyCFG lowering
/// produces canonical (X, B) and (sub, K) orderings. Swapped forms fall through
/// to OtherMulti, the correct conservative classification.
static bool matchBoundsCheckArmsImpl(Value *Arm1, Value *Arm2,
                                     ICmpInst::Predicate ArmAPred,
                                     ICmpInst::Predicate ArmBPred, bool VarK,
                                     ScalarEvolution &SE, Loop *L) {
  using namespace PatternMatch;
  for (int Try = 0; Try < 2; ++Try) {
    Value *A = Try == 0 ? Arm1 : Arm2;
    Value *B = Try == 0 ? Arm2 : Arm1;
    auto *CmpA = dyn_cast<ICmpInst>(A);
    auto *CmpB = dyn_cast<ICmpInst>(B);
    if (!CmpA || !CmpB)
      continue;
    if (CmpA->getPredicate() != ArmAPred)
      continue;
    if (CmpB->getPredicate() != ArmBPred)
      continue;
    Value *X = CmpA->getOperand(0);
    Value *Bound = CmpA->getOperand(1);
    Value *SubV = CmpB->getOperand(0);
    Value *K = CmpB->getOperand(1);
    Value *BoundOfSub = nullptr, *XOfSub = nullptr;
    if (!match(SubV, m_Sub(m_Value(BoundOfSub), m_Value(XOfSub))))
      continue;
    if (Bound != BoundOfSub || X != XOfSub)
      continue;
    if (VarK) {
      if (!L)
        continue;
      if (!SE.isSCEVable(K->getType()))
        continue;
      const SCEV *KS = SE.getSCEV(K);
      if (auto *AR = dyn_cast<SCEVAddRecExpr>(KS))
        if (AR->getLoop() == L && AR->isAffine())
          return true;
      continue;
    }
    if (isa<ConstantInt>(K))
      return true;
  }
  return false;
}

/// True when \p V is an icmp with an operand whose SCEV is an AddRec for \p L.
static bool hasLAddRec(Value *V, ScalarEvolution &SE, Loop *L) {
  auto *Cmp = dyn_cast<ICmpInst>(V);
  if (!Cmp)
    return false;
  for (Value *Op : {Cmp->getOperand(0), Cmp->getOperand(1)}) {
    if (!SE.isSCEVable(Op->getType()))
      continue;
    const SCEV *S = SE.getSCEV(Op);
    if (auto *AR = dyn_cast<SCEVAddRecExpr>(S))
      if (AR->getLoop() == L)
        return true;
  }
  return false;
}

/// Match `or/and(icmp(X), icmp(Y))` where each arm has an operand whose SCEV is
/// an L-AddRec -- the "two unrelated AddRec exits combined by one OR/AND" shape
/// that SCEV's `computeExitLimit` does not fold to `min(BTC_a, BTC_b)`.
/// Returns false when L is null (out-of-loop edges can't have L-AddRec
/// operands).
static bool matchTwoAddRecICmpImpl(Value *Arm1, Value *Arm2,
                                   ScalarEvolution &SE, Loop *L) {
  if (!L)
    return false;
  return hasLAddRec(Arm1, SE, L) && hasLAddRec(Arm2, SE, L);
}

/// Classify a trap branch's i1 predicate by structural shape.
/// Trial order matters: the more-specific bounds-check sub-arithmetic OR/AND
/// shapes (incl. var-K) are attempted before the generic two-AddRec shape, so a
/// sub-arithmetic OR is never reported as `OrTwoAddRecICmp`.
///
/// Uses `m_LogicalOr` / `m_LogicalAnd` to catch both the explicit `or/and i1`
/// form and clang's `select i1` lowering.
///
/// 3+-arm cascades (`or(or(a, b), c)`) report as `OtherMulti`; the
/// simplification fixpoint peels inner forms into recognized shapes on later
/// pipeline iterations.
static TrapPredicateShape
classifyTrapPredicateShape(Value *Cond, ScalarEvolution &SE, Loop *L) {
  using namespace PatternMatch;
  if (!Cond)
    return TrapPredicateShape::Unknown;

  if (isa<ICmpInst>(Cond))
    return TrapPredicateShape::SingleICmp;

  Value *L1 = nullptr, *L2 = nullptr;

  if (match(Cond, m_LogicalOr(m_Value(L1), m_Value(L2)))) {
    if (matchBoundsCheckArmsImpl(L1, L2, ICmpInst::ICMP_UGE, ICmpInst::ICMP_ULT,
                                 /*VarK=*/false, SE, L))
      return TrapPredicateShape::OrBoundsCheckConstBound;
    if (matchBoundsCheckArmsImpl(L1, L2, ICmpInst::ICMP_UGE, ICmpInst::ICMP_ULT,
                                 /*VarK=*/true, SE, L))
      return TrapPredicateShape::OrBoundsCheckVarBound;
    if (matchTwoAddRecICmpImpl(L1, L2, SE, L))
      return TrapPredicateShape::OrTwoAddRecICmp;
    return TrapPredicateShape::OtherMulti;
  }

  if (match(Cond, m_LogicalAnd(m_Value(L1), m_Value(L2)))) {
    if (matchBoundsCheckArmsImpl(L1, L2, ICmpInst::ICMP_ULT, ICmpInst::ICMP_UGE,
                                 /*VarK=*/false, SE, L))
      return TrapPredicateShape::AndBoundsCheckConstBound;
    if (matchBoundsCheckArmsImpl(L1, L2, ICmpInst::ICMP_ULT, ICmpInst::ICMP_UGE,
                                 /*VarK=*/true, SE, L))
      return TrapPredicateShape::AndBoundsCheckVarBound;
    if (matchTwoAddRecICmpImpl(L1, L2, SE, L))
      return TrapPredicateShape::AndTwoAddRecICmp;
    return TrapPredicateShape::OtherMulti;
  }

  return TrapPredicateShape::OtherMulti;
}

/// HEURISTIC that estimates which trap checks are likely necessary validation
/// traps for incoming (function-argument) values, as opposed to in-loop
/// elimination candidates. Returns true iff \p BB sits within
/// `EntryProximityDepth` dominator steps of the function's entry block OR any
/// loop preheader. A trap at/above a preheader is structurally like
/// entry-level parameter validation -- on the pre-loop boundary, never paid for
/// by the loop body.
static bool isEntryProximate(const Function &F, BasicBlock *BB,
                             const DominatorTree &DT, const LoopInfo &LI) {
  if (!BB)
    return false;
  auto *N = DT.getNode(BB);
  if (!N)
    return false;
  unsigned Depth = 0;
  for (auto *Cur = N; Cur; Cur = Cur->getIDom()) {
    BasicBlock *CurBB = Cur->getBlock();
    // Function entry: classical entry-proximity boundary.
    if (CurBB == &F.getEntryBlock())
      return true;
    // Loop preheader: also a validation boundary. It sits outside the loop,
    // and trap edges at/above it run once per outer entry to the nest,
    // regardless of inner-loop iteration counts.
    if (BasicBlock *Succ = CurBB->getSingleSuccessor()) {
      if (Loop *SuccL = LI.getLoopFor(Succ))
        if (SuccL->getHeader() == Succ && SuccL->getLoopPreheader() == CurBB)
          return true;
    }
    if (Depth >= EntryProximityDepth.getValue())
      return false;
    ++Depth;
  }
  return false;
}

/// Decide whether an equivalent check on a dominating path already guarantees
/// this trap cannot fire. Returns true iff some dominator of \p BB ends in a
/// conditional branch whose condition is the same as \p MyCond -- either the
/// identical SSA value, or a structurally equivalent icmp (same predicate and
/// operand pointers) -- so the trap predicate has already been decided before
/// control reaches \p BB.
static bool isDominatedByEquivalentCheck(BasicBlock *BB, Value *MyCond,
                                         const DominatorTree &DT) {
  if (!BB || !MyCond)
    return false;
  auto *MyCmp = dyn_cast<ICmpInst>(MyCond);
  auto *N = DT.getNode(BB);
  if (!N)
    return false;
  // Walk strict dominators (skip BB itself), bounding cost on functions with
  // very deep dominator chains.
  const unsigned MaxDomChains = 16;
  unsigned Steps = 0;
  for (auto *Cur = N->getIDom(); Cur && Steps < MaxDomChains;
       Cur = Cur->getIDom(), ++Steps) {
    BasicBlock *Dom = Cur->getBlock();
    auto *DomBI = dyn_cast<CondBrInst>(Dom->getTerminator());
    if (!DomBI)
      continue;
    Value *DomCond = DomBI->getCondition();
    bool Equivalent = (DomCond == MyCond);
    if (!Equivalent && MyCmp)
      if (auto *DomCmp = dyn_cast<ICmpInst>(DomCond))
        Equivalent = DomCmp->getPredicate() == MyCmp->getPredicate() &&
                     DomCmp->getOperand(0) == MyCmp->getOperand(0) &&
                     DomCmp->getOperand(1) == MyCmp->getOperand(1);
    if (!Equivalent)
      continue;
    if (!LTAEmitExplain)
      return true;
    // Require the dominating branch to actually DETERMINE the condition on the
    // path to BB (one of its edges dominates BB). A same-condition dominator
    // whose neither edge dominates BB proves nothing, so calling the trap
    // "redundant" would be unsound.
    BasicBlockEdge TrueEdge(Dom, DomBI->getSuccessor(0));
    BasicBlockEdge FalseEdge(Dom, DomBI->getSuccessor(1));
    if (DT.dominates(TrueEdge, BB) || DT.dominates(FalseEdge, BB))
      return true;
  }
  return false;
}

/// For each conditional branch whose target is a trap block, emit one
/// `LoopTrapEdge<Tag>` remark with per-edge classification fields.
///
/// SCEV / AA blockers (in-loop trap edges):
///   SCEVComputed       — every leaf-operand has a non-CouldNotCompute SCEV
///   SCEVLoopInvariant  — every leaf-operand SCEV is loop-invariant in the
///                        *innermost* containing loop (standard
///                        `SE.isLoopInvariant(SCEV, L)`; trivially true when
///                        the source BB is outside any loop).
///   HasAddRec          — at least one operand SCEV contains a SCEVAddRecExpr
///   HasInLoopUnknown   — at least one operand SCEV contains a SCEVUnknown
///                        whose Instruction is defined inside any containing
///                        loop (true even when SCEVLoopInvariant w.r.t.
///                        innermost holds — i.e. the load lives in an outer
///                        loop).
///   HasStoreReload     — at least one operand SCEV references a SCEVUnknown
///                        that is a LoadInst in the innermost loop,
///                        AA-may-aliased by some StoreInst / mem-intrinsic in
///                        that loop.
///   HasCallReload      — as HasStoreReload but for a may-modifying CallBase.
///                        (Not exclusive: an edge can have both; downstream
///                        treats that as a "both" bucket.)
///   IsLoopExit         — the trap successor is outside the innermost loop
///                        (loop-exit, not loop-internal). Required for the BTC
///                        fields to be meaningful.
///   EdgeBTCComputable  — the per-edge SCEV exit count for THIS trap edge's
///                        source BB is known: `SE.getExitCount(Innermost,
///                        sourceBB) != SCEVCouldNotCompute` (false if
///                        !IsLoopExit). This is the exit count for this
///                        specific exiting block, NOT the loop's overall
///                        latch/backedge-taken count.
///   LoopHasOtherUnknownBTCTrap — the innermost loop has another trap-exit
///                        (excluding this edge) with SCEVCouldNotCompute exit
///                        count ("this edge computable but loop's others
///                        aren't, so we can't hoist any").
///
/// Predicate-tree shape (all trap edges):
///   PredicateShape     — TrapPredicateShape enum name for the trap branch's
///                        i1 predicate. Sizes the reach of shape-specific
///                        levers (NUW propagation on bounds-check OR/AND forms,
///                        their variable-bound extensions, and generic
///                        OR-of-AddRec exit recognition).
///
/// Out-of-loop / structural-redundancy fields:
///   IsEntryProximate   — source BB within `EntryProximityDepth` dominator
///                        steps of the entry block OR a loop preheader; genuine
///                        validation trap, NOT an elimination candidate.
///   DominatedByEquivalentCheck — some dominator of source BB ends in a
///                        cond-branch with the same predicate; already proven
///                        earlier — a dominator-aware InstCombine / CVP fold
///                        candidate.
///
/// Plus loop-context fields (LoopHeader / Depth / IsInnermost) so downstream
/// tooling can join against `LoopPrimitives<Tag>` records.
/// Walk the recurrence chain reachable from a header phi's loop-carried
/// incoming value, returning false if any reached in-loop Instruction sits in
/// a BB that doesn't dominate the latch. Follows operands transitively through
/// non-header (merge) phis and arithmetic, so the chain reaches the underlying
/// add/sub even behind an inserted merge phi.
static bool allUpdatesDominate(Value *Start, Loop *L, BasicBlock *Header,
                               BasicBlock *Latch, const DominatorTree &DT) {
  SmallPtrSet<const Value *, 32> Seen;
  SmallVector<const Value *, 16> Stack;
  Stack.push_back(Start);
  unsigned Steps = 0;
  const unsigned StepLimit = 64;
  while (!Stack.empty() && Steps++ < StepLimit) {
    const Value *V = Stack.pop_back_val();
    if (!Seen.insert(V).second)
      continue;
    const auto *I = dyn_cast<Instruction>(V);
    if (!I)
      continue;
    if (!L->contains(I->getParent()))
      continue;
    // The header phi's parent IS the header, which dominates the latch;
    // don't recurse through it (recurrence already inspected at the call
    // site).
    if (isa<PHINode>(I) && I->getParent() == Header)
      continue;
    // Any other chain instruction must live in a BB dominating the latch;
    // otherwise the update is control-dependent.
    if (!DT.dominates(I->getParent(), Latch))
      return false;
    // Walk operands. For non-header (merge) phis this recurses into both
    // arms — catching the `m += cond ? 1 : 0` shape where one arm's add sits
    // in a non-dominating BB.
    for (const Use &U : I->operands())
      Stack.push_back(U.get());
  }
  return true;
}

/// Walk the def chain of \p Cond (bounded to instructions inside \p L) and
/// answer Q2 of the trap-explanation tree: for every IV-shaped operand reached,
/// does its update dominate the loop latch?
///
/// For each header phi reached, walk its loop-carried recurrence (the
/// latch-incoming value, through non-header phis) and check every visited
/// Instruction's parent BB. If any does not dominate the latch, the IV's update
/// is control-dependent (e.g. `m = c ? m + 1 : m` puts `m+1` in a
/// non-dominating arm) and SCEV refuses an AddRec -> returns false.
///
/// Returns true vacuously when no header-phi is reached.
static bool computeIVUpdateDominatesLatch(Value *Cond, Loop *L,
                                          const DominatorTree &DT) {
  if (!L)
    return true;
  BasicBlock *Header = L->getHeader();
  BasicBlock *Latch = L->getLoopLatch();
  if (!Latch)
    return true;
  SmallPtrSet<const Value *, 32> Seen;
  SmallVector<const Value *, 16> Stack;
  Stack.push_back(Cond);
  unsigned Steps = 0;
  const unsigned StepLimit = 64;
  while (!Stack.empty() && Steps++ < StepLimit) {
    const Value *V = Stack.pop_back_val();
    if (!Seen.insert(V).second)
      continue;
    const auto *I = dyn_cast<Instruction>(V);
    if (!I)
      continue;
    if (!L->contains(I->getParent()))
      continue;
    if (const auto *Phi = dyn_cast<PHINode>(I)) {
      if (Phi->getParent() == Header) {
        if (Value *In = Phi->getIncomingValueForBlock(Latch))
          if (!allUpdatesDominate(In, L, Header, Latch, DT))
            return false;
        // Don't recurse through phi operands — recurrence just inspected via
        // the latch incoming.
        continue;
      }
    }
    for (const Use &U : I->operands())
      Stack.push_back(U.get());
  }
  return true;
}

// Per-load "do any in-loop instructions may-alias this load?" query, cached in
// \p ReloadCache. Key: (Loop*, LoadInst*). Returns (StoreAlias,
// MemIntrinsicAlias, CallAlias) tracked SEPARATELY so the consumer can bucket
// scalar-store vs struct-copy (mem-intrinsic) aliasing (different levers).
static std::tuple<bool, bool, bool> loadAliasFlags(
    LoadInst *Load, Loop *L, AAResults &AA,
    DenseMap<std::pair<Loop *, LoadInst *>, std::tuple<bool, bool, bool>>
        &ReloadCache) {
  auto Key = std::make_pair(L, Load);
  auto It = ReloadCache.find(Key);
  if (It != ReloadCache.end())
    return It->second;
  bool Store = false, MemI = false, Call = false;
  MemoryLocation LoadLoc = MemoryLocation::get(Load);
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (&I == Load)
        continue;
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (LTAEmitExplain ? isModSet(AA.getModRefInfo(SI, LoadLoc))
                           : !AA.isNoAlias(MemoryLocation::get(SI), LoadLoc))
          Store = true;
      } else if (auto *MI = dyn_cast<MemIntrinsic>(&I)) {
        if (isModSet(AA.getModRefInfo(MI, LoadLoc)))
          MemI = true;
      } else if (auto *CB = dyn_cast<CallBase>(&I)) {
        if (CB->onlyReadsMemory() || CB->doesNotAccessMemory())
          continue;
        if (isModSet(AA.getModRefInfo(CB, LoadLoc)))
          Call = true;
      }
      if (Store && MemI && Call)
        break;
    }
    if (Store && MemI && Call)
      break;
  }
  auto Out = std::make_tuple(Store, MemI, Call);
  ReloadCache[Key] = Out;
  return Out;
}

// For a load flagged store-may-cause-reload, return the FIRST in-loop
// store / mem-intrinsic that may-alias it (per AA, which already folds in
// TBAA). Lets the consumer point at the specific writer that blocks LICM.
static Instruction *firstAliasingStore(LoadInst *Load, Loop *L, AAResults &AA) {
  MemoryLocation LoadLoc = MemoryLocation::get(Load);
  for (BasicBlock *BB : L->blocks())
    for (Instruction &I : *BB) {
      if (&I == Load)
        continue;
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (LTAEmitExplain ? isModSet(AA.getModRefInfo(SI, LoadLoc))
                           : !AA.isNoAlias(MemoryLocation::get(SI), LoadLoc))
          return &I;
      } else if (auto *MI = dyn_cast<MemIntrinsic>(&I)) {
        if (isModSet(AA.getModRefInfo(MI, LoadLoc)))
          return &I;
      }
    }
  return nullptr;
}

// Extract a human-readable TBAA access-type name from an instruction's TBAA
// metadata (struct-path tag: operand 1 is the access-type descriptor; scalar
// tag: operand 0 is the type-name string). A memcpy / struct copy carries
// !tbaa.struct instead — report "struct-copy". Empty if no TBAA.
static StringRef tbaaTypeName(const Instruction *I) {
  AAMDNodes AAMD = I->getAAMetadata();
  if (const MDNode *TBAA = AAMD.TBAA) {
    const MDNode *AccessType = TBAA;
    if (TBAA->getNumOperands() >= 2)
      if (auto *AT = dyn_cast<MDNode>(TBAA->getOperand(1)))
        AccessType = AT;
    if (AccessType->getNumOperands() >= 1)
      if (auto *Name = dyn_cast<MDString>(AccessType->getOperand(0)))
        return Name->getString();
  }
  if (AAMD.TBAAStruct)
    return "struct-copy";
  return "";
}

// Short kind tag for a reload-blocking writer.
static StringRef writerKind(const Instruction *I) {
  if (isa<MemCpyInst>(I))
    return "memcpy";
  if (isa<MemMoveInst>(I))
    return "memmove";
  if (isa<MemSetInst>(I))
    return "memset";
  if (isa<MemIntrinsic>(I))
    return "mem-intrinsic";
  if (isa<StoreInst>(I))
    return "store";
  if (isa<CallBase>(I))
    return "call";
  return "writer";
}

// Like firstAliasingStore but also considers side-effecting calls — used
// by the flag-gated per-load alias annotation (LoopLoadAlias).
static Instruction *firstAliasingWriter(LoadInst *Load, Loop *L,
                                        AAResults &AA) {
  MemoryLocation LoadLoc = MemoryLocation::get(Load);
  for (BasicBlock *BB : L->blocks())
    for (Instruction &I : *BB) {
      if (&I == Load)
        continue;
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (LTAEmitExplain ? isModSet(AA.getModRefInfo(SI, LoadLoc))
                           : !AA.isNoAlias(MemoryLocation::get(SI), LoadLoc))
          return &I;
      } else if (auto *MI = dyn_cast<MemIntrinsic>(&I)) {
        if (isModSet(AA.getModRefInfo(MI, LoadLoc)))
          return &I;
      } else if (auto *CB = dyn_cast<CallBase>(&I)) {
        if (CB->onlyReadsMemory() || CB->doesNotAccessMemory())
          continue;
        if (isModSet(AA.getModRefInfo(CB, LoadLoc)))
          return &I;
      }
    }
  return nullptr;
}

static void emitPerTrapEdgeSCEV(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                                AAResults &AA, DominatorTree &DT,
                                OptimizationRemarkEmitter &ORE, StringRef Tag,
                                unsigned InvocationSeq) {
  std::string Name = taggedName("LoopTrapEdge", Tag);
  // Same trap-block predicate as the rest of the pass: require `call
  // @llvm.trap()` immediately before `unreachable`. Bare `unreachable` (after a
  // noreturn call, or `__builtin_unreachable`) is NOT a trap.
  auto IsTrapBlock = [](BasicBlock *BB) {
    return isTrapBlock(BB, BoundsSafetyTrapsOnly,
                       LegacyTrapMatch::TrapIntrinsic);
  };

  // Pre-pass: per innermost loop, count how many of its trap-exits have
  // SCEVCouldNotCompute exit-counts. Used downstream to classify the
  // "edge is computable but loop's other traps aren't" case.
  DenseMap<Loop *, unsigned> UncomputableTrapExits;
  for (Loop *L : LI.getLoopsInPreorder()) {
    SmallVector<BasicBlock *, 4> Exiting;
    L->getExitingBlocks(Exiting);
    unsigned N = 0;
    for (BasicBlock *EB : Exiting) {
      auto *BI = dyn_cast<CondBrInst>(EB->getTerminator());
      if (!BI)
        continue;
      BasicBlock *TrapSucc = nullptr;
      for (BasicBlock *Succ : BI->successors())
        if (!L->contains(Succ) && IsTrapBlock(Succ)) {
          TrapSucc = Succ;
          break;
        }
      if (!TrapSucc)
        continue;
      const SCEV *EC = SE.getExitCount(L, EB);
      if (isa<SCEVCouldNotCompute>(EC))
        ++N;
    }
    UncomputableTrapExits[L] = N;
  }

  // Per-loop cache of "do any in-loop instructions may-alias this load?",
  // populated lazily. Key: (Loop*, LoadInst*). Returns (StoreAlias,
  // MemIntrinsicAlias, CallAlias) tracked SEPARATELY so the consumer can bucket
  // scalar-store vs struct-copy (mem-intrinsic) aliasing (different levers).
  DenseMap<std::pair<Loop *, LoadInst *>, std::tuple<bool, bool, bool>>
      ReloadCache;

  for (BasicBlock &BB : F) {
    auto *BI = dyn_cast<CondBrInst>(BB.getTerminator());
    if (!BI)
      continue;
    BasicBlock *TrapSucc = nullptr;
    for (BasicBlock *Succ : BI->successors())
      if (IsTrapBlock(Succ)) {
        TrapSucc = Succ;
        break;
      }
    if (!TrapSucc)
      continue;

    // Compute the chain of containing loops for the source BB.
    Loop *Innermost = LI.getLoopFor(&BB);
    SmallVector<Loop *, 4> ContainingLoops;
    for (Loop *L = Innermost; L; L = L->getParentLoop())
      ContainingLoops.push_back(L);

    bool IsLoopExit = Innermost && !Innermost->contains(TrapSucc);

    // Unfold the predicate into leaf operands.
    SmallVector<Value *, 8> LeafOperands;
    SmallPtrSet<Value *, 16> Visited;
    collectBoolLeafOperands(BI->getCondition(), LeafOperands, Visited);

    bool AllComputed = true;
    bool LoopInvariantInInnermost = true; // trivially true if no loop
    bool HasAddRec = false;
    bool HasInLoopUnknown = false;
    bool HasStoreReload = false;
    bool HasMemIntrinsicReload = false;
    bool HasCallReload = false;
    // Trip-count-unknown sub-flavor flags (set when the operand SCEV is
    // computable but non-AddRec / non-invariant for L AND the AA reload check
    // didn't fire). Split the opaque-operand cases by what makes the operand
    // opaque:
    //   HasUnaliasedLoadOperand  — LoadInst in L with no may-aliased writer
    //                              (data-dependent/indirect load).
    //   HasInLoopPhiOperand      — non-IV PHINode in L (loop-carried
    //                              state / conditional increments). IV phis
    //                              fold to AddRecs and don't appear here.
    //   HasOtherInLoopUnknownOperand — some other in-loop def: select / freeze
    //                              / intrinsic-call result / etc.
    //   HasOpaqueOperandNoInLoopUnknown — non-AddRec/non-invariant for L but no
    //                              in-loop SCEVUnknown leaf (outer-loop
    //                              AddRec, or non-unit modular stride).
    bool HasUnaliasedLoadOperand = false;
    bool HasInLoopPhiOperand = false;
    bool HasInLoopFreezeOperand = false;
    bool HasInLoopSelectOperand = false;
    bool HasOtherInLoopUnknownOperand = false;
    // The in-loop store / mem-intrinsic that blocks a reload, and the load it
    // clobbers; null unless HasStoreReload fires.
    Instruction *ReloadStore = nullptr;
    LoadInst *ReloadLoad = nullptr;
    bool HasOpaqueOperandNoInLoopUnknown = false;
    // True when any operand SCEV contains an AddRec on a loop that is NOT the
    // innermost. Splits the opaque / no-in-loop-unknown class into "via
    // outer-loop AddRec" (attackable by teaching SCEV that an outer-loop AddRec
    // is innermost-invariant) vs "via something else" (modular stride, etc.).
    bool HasOuterLoopAddRecOperand = false;
    // Weak-no-wrap-class sub-classifier flags. The weak-no-wrap class is
    // "operands clean for L but getExitCount still CouldNotCompute"; these
    // break it down:
    //   HasNonUnitStrideForLAddRec  — an L-AddRec operand with |step| > 1.
    //                                 SCEV's exit-count machinery has limited
    //                                 non-unit-stride support (needs a
    //                                 divisibility proof against the bound).
    //   HasOnlyNotProvenMonotonicForLAddRec — an L-AddRec with FlagNW but
    //   neither NUW
    //                                 nor NSW; howMany{Less,Greater}Thans
    //                                 typically need the stronger flag.
    //   HasNegativeStrideForLAddRec — an L-AddRec with a negative step (hits
    //                                 the independently fragile
    //                                 howManyGreaterThans path).
    // Note: no HasMultipleLeafOperands field — NumLeafOperands already exposes
    // it; the Python aggregator treats NumLeafOperands>1 as the multi-leaf
    // OR/AND-of-icmps case (the dominant weak-no-wrap-class shape).
    bool HasNonUnitStrideForLAddRec = false;
    bool HasOnlyNotProvenMonotonicForLAddRec = false;
    bool HasNegativeStrideForLAddRec = false;
    // True when some L-AddRec has a NON-CONSTANT (runtime) step. Distinct from
    // constant-but-non-unit: closed-form trip count is essentially never
    // computable for a non-constant step. Surfaced separately so consumers can
    // split "stride!=±1 (gcd alignment maybe works)" from "stride is runtime".
    bool HasNonConstantStrideForLAddRec = false;
    unsigned NumLeafOps = 0;

    for (Value *V : LeafOperands) {
      ++NumLeafOps;
      if (!V || isa<Constant>(V))
        continue;
      if (!SE.isSCEVable(V->getType())) {
        AllComputed = false;
        continue;
      }
      const SCEV *SC = SE.getSCEV(V);
      if (isa<SCEVCouldNotCompute>(SC)) {
        AllComputed = false;
        continue;
      }
      // Walk the SCEV tree.
      SCEVNodeCollector Coll;
      SCEVTraversal<SCEVNodeCollector>(Coll).visitAll(SC);
      if (!Coll.AddRecs.empty())
        HasAddRec = true;
      // Weak-no-wrap-class sub-classifier: for each AddRec on the innermost
      // loop, inspect step
      // constness, magnitude, and nowrap flags.
      if (Innermost) {
        for (const SCEVAddRecExpr *AR : Coll.AddRecs) {
          if (AR->getLoop() != Innermost)
            continue;
          if (!AR->isAffine())
            continue;
          // FlagNW only (no NUW / NSW) — typical for AddRecs from
          // `add invariant, mul(IV, const)` where the outer add lacks IR-level
          // nowrap.
          if (!AR->hasNoUnsignedWrap() && !AR->hasNoSignedWrap())
            HasOnlyNotProvenMonotonicForLAddRec = true;
          if (auto *StepC = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE))) {
            const APInt &Step = StepC->getAPInt();
            if (Step.isNegative())
              HasNegativeStrideForLAddRec = true;
            // |step| > 1 — non-unit stride, fragile for getExitCount.
            APInt AbsStep = Step.isNegative() ? -Step : Step;
            if (AbsStep.ugt(1))
              HasNonUnitStrideForLAddRec = true;
          } else {
            // Non-constant (runtime) step: closed-form trip count essentially
            // never computable. Tag both the umbrella non-unit flag and the
            // specific non-constant-stride flag.
            HasNonUnitStrideForLAddRec = true;
            HasNonConstantStrideForLAddRec = true;
          }
        }
      }
      // Per-loop invariance: standard SE.isLoopInvariant against the
      // *innermost* containing loop only (single canonical bool; caller can
      // re-query outer loops via SE).
      if (Innermost && !SE.isLoopInvariant(SC, Innermost))
        LoopInvariantInInnermost = false;

      // Is this operand "opaque w.r.t. L" — the kind that pushes the edge into
      // a trip-count-unknown bucket because its SCEV isn't pinned to L's IV and
      // isn't loop-invariant?
      bool OpIsOpaqueForL = false;
      if (Innermost) {
        OpIsOpaqueForL = !SE.isLoopInvariant(SC, Innermost);
        if (auto *AR = dyn_cast<SCEVAddRecExpr>(SC))
          if (AR->getLoop() == Innermost)
            OpIsOpaqueForL = false;
      }
      // Did we find any in-loop SCEVUnknown for THIS operand? Fires the
      // opaque-operand / no-in-loop-unknown flavor when the SCEV is
      // opaque-for-L but its leaves are all outside L.
      bool OpHasInLoopUnknown = false;

      // SCEVUnknowns defined in any containing loop — diagnostic for the "load
      // lives in an outer containing loop" case (invariant in the innermost,
      // but a load in some outer loop changes per-outer-iteration).
      for (const SCEVUnknown *U : Coll.Unknowns) {
        Value *UV = U->getValue();
        auto *I = dyn_cast_or_null<Instruction>(UV);
        if (!I)
          continue;
        Loop *DefLoop = LI.getLoopFor(I->getParent());
        for (Loop *DL = DefLoop; DL; DL = DL->getParentLoop()) {
          for (Loop *CL : ContainingLoops)
            if (DL == CL) {
              HasInLoopUnknown = true;
              break;
            }
          if (HasInLoopUnknown)
            break;
        }
        // AA-based reload classification and operand-flavor bucketing: only
        // meaningful when the SCEVUnknown's Inst is in the *innermost* loop
        // (the one SCEV reasons about and partial-BTC predication would
        // target).
        if (Innermost && Innermost->contains(I->getParent())) {
          OpHasInLoopUnknown = true;
          if (auto *Load = dyn_cast<LoadInst>(I)) {
            auto [SAlias, MAlias, CAlias] =
                loadAliasFlags(Load, Innermost, AA, ReloadCache);
            if (SAlias)
              HasStoreReload = true;
            if (MAlias)
              HasMemIntrinsicReload = true;
            if (CAlias)
              HasCallReload = true;
            // Point at the specific aliasing writer (first store / mem-
            // intrinsic wins) so the consumer sees WHERE the reload comes from.
            if ((SAlias || MAlias) && !ReloadStore) {
              ReloadStore = firstAliasingStore(Load, Innermost, AA);
              ReloadLoad = Load;
            }
            // Load with no AA-detected writer (indirect / data-dependent;
            // classifier today buckets under the generic trip-count-unknown
            // opaque-operand case).
            if (!SAlias && !MAlias && !CAlias)
              HasUnaliasedLoadOperand = true;
          } else if (isa<PHINode>(I)) {
            // In-loop phi operand — phi SCEV couldn't recognize as an IV (IVs
            // fold into an AddRec instead of appearing here as a SCEVUnknown).
            HasInLoopPhiOperand = true;
          } else {
            // Any other in-loop def (freeze / select / intrinsic / call /
            // ...). HasOtherInLoopUnknownOperand is the umbrella flag (superset
            // for existing consumers); the specific opacity shape is also
            // recorded so newer consumers can split freeze vs select vs other.
            // freeze is often a redundant freeze on a value proven non-poison
            // by a dominating guard; select is a control-flow-merge choice.
            HasOtherInLoopUnknownOperand = true;
            if (isa<FreezeInst>(I))
              HasInLoopFreezeOperand = true;
            else if (isa<SelectInst>(I))
              HasInLoopSelectOperand = true;
          }
        }
      }

      // Detect AddRecs on a loop OTHER than the innermost. If the opaque /
      // no-in-loop-unknown class fires for this operand AND such an AddRec is
      // present, the opacity is only because SCEV doesn't fold outer-loop
      // AddRecs into innermost-invariance — a SCEV-teaching opportunity, not a
      // fundamental "modular stride" wall.
      if (Innermost) {
        for (const SCEVAddRecExpr *AR : Coll.AddRecs) {
          if (AR->getLoop() != Innermost) {
            HasOuterLoopAddRecOperand = true;
            break;
          }
        }
      }

      // Opaque-operand / no-in-loop-unknown class — operand SCEV is non-AddRec
      // / non-invariant for L but none of its SCEVUnknown leaves is defined
      // inside L. Captures outer-loop AddRecs and other constructs blocking
      // partial-BTC predication for reasons unrelated to in-loop memory.
      if (OpIsOpaqueForL && !OpHasInLoopUnknown)
        HasOpaqueOperandNoInLoopUnknown = true;
    }

    // Per-edge exit count + per-loop "other-uncomputable" flag.
    // EdgeBTCComputable reflects the per-edge SCEV exit count for THIS exiting
    // block (SE.getExitCount for &BB), not the loop's overall
    // latch/backedge-taken count.
    bool EdgeBTCComputable = false;
    bool EdgeBTCSymbolic = false;
    bool LoopHasOtherUnknownBTCTrap = false;
    if (Innermost && IsLoopExit) {
      const SCEV *EC = SE.getExitCount(Innermost, &BB);
      EdgeBTCComputable = !isa<SCEVCouldNotCompute>(EC);
      if (!EdgeBTCComputable) {
        // Not exactly-known; check the SymbolicMaximum bucket so edge-level
        // attribution matches the per-loop SCEV bucket.
        const SCEV *SymEC =
            SE.getExitCount(Innermost, &BB, ScalarEvolution::SymbolicMaximum);
        EdgeBTCSymbolic = !isa<SCEVCouldNotCompute>(SymEC);
      }
      unsigned NUnc = UncomputableTrapExits.lookup(Innermost);
      LoopHasOtherUnknownBTCTrap = EdgeBTCComputable ? (NUnc > 0) : (NUnc > 1);
    }

    // Predicate-tree shape (in-loop and out-of-loop edges alike). Sizes the
    // reach of shape-specific levers: NUW propagation on a bounds-check OR
    // targets OrBoundsCheckConstBound; the AND-form lever targets
    // AndBoundsCheckConstBound; the variable-bound extension targets the
    // *BoundsCheckVarBound shapes; a generic SCEV exit-count extension targets
    // {Or,And}TwoAddRecICmp.
    TrapPredicateShape PredShape =
        classifyTrapPredicateShape(BI->getCondition(), SE, Innermost);

    // Out-of-loop / structural-redundancy fields. Also computed for in-loop
    // edges (cheap, and an in-loop trap dominated by an equivalent earlier
    // check is still a CVP / dominator-fold candidate).
    bool IsEntryProx = isEntryProximate(F, &BB, DT, LI);
    bool DomByEquiv = isDominatedByEquivalentCheck(&BB, BI->getCondition(), DT);

    // Q1 (dominance): does the trap branch's BB dominate the latch? If yes the
    // cond fires every iteration; if no it's in a conditional arm and LICM
    // can't hoist here. Q2 (IV update): for any IV operand, does its update
    // dominate the latch? If no, the IV is conditionally updated and SCEV
    // refuses an AddRec → trap not eliminable via trip-count.
    bool DominatesLatch = false;
    bool IVUpdateDominatesLatch = true;
    if (LTAEmitExplain) {
      BasicBlock *Latch = Innermost ? Innermost->getLoopLatch() : nullptr;
      DominatesLatch = Latch && DT.dominates(&BB, Latch);
      IVUpdateDominatesLatch =
          computeIVUpdateDominatesLatch(BI->getCondition(), Innermost, DT);
    }

    // Resolve the reload-blocking writer: source line, kind (store / memcpy /
    // …), TBAA type; plus WHICH load is reloaded (pointer name + source line)
    // and its TBAA type. Empty when no store-reload.
    unsigned ReloadStoreLine = 0, ReloadLoadLine = 0;
    StringRef ReloadStoreTBAA, ReloadStoreKind, ReloadLoadTBAA, ReloadLoadName;
    if (ReloadStore) {
      if (const DebugLoc &DL = ReloadStore->getDebugLoc())
        ReloadStoreLine = DL.getLine();
      ReloadStoreTBAA = tbaaTypeName(ReloadStore);
      ReloadStoreKind = writerKind(ReloadStore);
    }
    if (ReloadLoad) {
      ReloadLoadTBAA = tbaaTypeName(ReloadLoad);
      if (const DebugLoc &DL = ReloadLoad->getDebugLoc())
        ReloadLoadLine = DL.getLine();
      if (Value *Ptr = ReloadLoad->getPointerOperand())
        ReloadLoadName = Ptr->getName();
    }

    // Count-ordered trap-class code, computed once here (single source of
    // truth; consumers read `TrapClass` instead of re-deriving it).
    StringRef TrapClass = computeTrapClass(
        /*InLoop=*/Innermost != nullptr, PredShape, IsEntryProx, DomByEquiv,
        IsLoopExit, EdgeBTCComputable, LoopHasOtherUnknownBTCTrap,
        HasStoreReload, HasMemIntrinsicReload, HasCallReload,
        HasInLoopPhiOperand, HasUnaliasedLoadOperand,
        HasOtherInLoopUnknownOperand, HasOpaqueOperandNoInLoopUnknown,
        HasInLoopFreezeOperand, HasInLoopSelectOperand, NumLeafOps,
        HasNonUnitStrideForLAddRec, HasNegativeStrideForLAddRec,
        HasNonConstantStrideForLAddRec, HasOnlyNotProvenMonotonicForLAddRec);

    OptimizationRemarkAnalysis Rem(REMARK_PASS, Name, BI);
    Rem << "Function " << NV("Function", F.getName())
        << " src_bb=" << NV("SourceBB", bbLabel(&BB))
        << " trap_bb=" << NV("TrapBB", bbLabel(TrapSucc)) << " loop_depth="
        << NV("LoopDepth", Innermost ? Innermost->getLoopDepth() : 0u)
        << " loop_header="
        << NV("LoopHeader",
              Innermost ? bbLabel(Innermost->getHeader()) : std::string(""))
        << " is_innermost="
        << NV("IsInnermost", (bool)(Innermost && Innermost->isInnermost()))
        << " is_loop_exit=" << NV("IsLoopExit", IsLoopExit)
        << " trap_class=" << NV("TrapClass", TrapClass)
        << " num_leaf_operands=" << NV("NumLeafOperands", NumLeafOps)
        << " scev_computed=" << NV("SCEVComputed", AllComputed)
        << " scev_loop_invariant="
        << NV("SCEVLoopInvariant", LoopInvariantInInnermost)
        << " has_addrec=" << NV("HasAddRec", HasAddRec)
        << " has_in_loop_unknown=" << NV("HasInLoopUnknown", HasInLoopUnknown)
        << " has_store_reload=" << NV("HasStoreReload", HasStoreReload)
        << " has_mem_intrinsic_reload="
        << NV("HasMemIntrinsicReload", HasMemIntrinsicReload)
        << " reload_store_line=" << NV("ReloadStoreLine", ReloadStoreLine)
        << " reload_store_kind=" << NV("ReloadStoreKind", ReloadStoreKind)
        << " reload_store_tbaa=" << NV("ReloadStoreTBAA", ReloadStoreTBAA)
        << " reload_load_tbaa=" << NV("ReloadLoadTBAA", ReloadLoadTBAA)
        << " reload_load_name=" << NV("ReloadLoadName", ReloadLoadName)
        << " reload_load_line=" << NV("ReloadLoadLine", ReloadLoadLine)
        << " has_call_reload=" << NV("HasCallReload", HasCallReload)
        << " has_unaliased_load_operand="
        << NV("HasUnaliasedLoadOperand", HasUnaliasedLoadOperand)
        << " has_in_loop_phi_operand="
        << NV("HasInLoopPhiOperand", HasInLoopPhiOperand)
        << " has_in_loop_freeze_operand="
        << NV("HasInLoopFreezeOperand", HasInLoopFreezeOperand)
        << " has_in_loop_select_operand="
        << NV("HasInLoopSelectOperand", HasInLoopSelectOperand)
        << " has_other_in_loop_unknown_operand="
        << NV("HasOtherInLoopUnknownOperand", HasOtherInLoopUnknownOperand)
        << " has_opaque_operand_no_in_loop_unknown="
        << NV("HasOpaqueOperandNoInLoopUnknown",
              HasOpaqueOperandNoInLoopUnknown)
        << " has_outer_loop_addrec_operand="
        << NV("HasOuterLoopAddRecOperand", HasOuterLoopAddRecOperand)
        << " has_non_unit_stride_for_l_addrec="
        << NV("HasNonUnitStrideForLAddRec", HasNonUnitStrideForLAddRec)
        << " has_non_constant_stride_for_l_addrec="
        << NV("HasNonConstantStrideForLAddRec", HasNonConstantStrideForLAddRec)
        << " has_only_weak_no_wrap_for_l_addrec="
        << NV("HasOnlyNotProvenMonotonicForLAddRec",
              HasOnlyNotProvenMonotonicForLAddRec)
        << " has_negative_stride_for_l_addrec="
        << NV("HasNegativeStrideForLAddRec", HasNegativeStrideForLAddRec)
        << " edge_btc_computable=" << NV("EdgeBTCComputable", EdgeBTCComputable)
        << " edge_btc_symbolic=" << NV("EdgeBTCSymbolic", EdgeBTCSymbolic)
        << " loop_has_other_unknown_btc_trap="
        << NV("LoopHasOtherUnknownBTCTrap", LoopHasOtherUnknownBTCTrap)
        << " predicate_shape="
        << NV("PredicateShape", trapPredicateShapeName(PredShape).str())
        << " is_entry_proximate=" << NV("IsEntryProximate", IsEntryProx)
        << " dominated_by_equivalent_check="
        << NV("DominatedByEquivalentCheck", DomByEquiv);
    if (LTAEmitExplain) {
      Rem << " dominates_latch=" << NV("DominatesLatch", DominatesLatch)
          << " iv_update_dominates_latch="
          << NV("IVUpdateDominatesLatch", IVUpdateDominatesLatch)
          << " invocation_seq=" << NV("InvocationSeq", InvocationSeq);
    }
    ORE.emit(Rem);
  }

  // Flag-gated per-load alias annotation: for each load in an INNERMOST loop
  // may-clobbered by an in-loop writer (store / memcpy / call), emit a
  // LoopLoadAlias record naming the first such writer. Clobbered loads only
  // (hoistable loads omitted).
  if (LTAEmitLoadAlias) {
    std::string LName = taggedName("LoopLoadAlias", Tag);
    for (Loop *L : LI.getLoopsInPreorder()) {
      if (!L->isInnermost())
        continue;
      for (BasicBlock *BB : L->blocks())
        for (Instruction &I : *BB) {
          auto *Load = dyn_cast<LoadInst>(&I);
          if (!Load)
            continue;
          Instruction *W = firstAliasingWriter(Load, L, AA);
          if (!W)
            continue; // hoistable — no in-loop writer may-aliases it
          unsigned LoadLine = 0, WLine = 0;
          if (const DebugLoc &DL = Load->getDebugLoc())
            LoadLine = DL.getLine();
          if (const DebugLoc &DL = W->getDebugLoc())
            WLine = DL.getLine();
          StringRef LoadName;
          if (Value *P = Load->getPointerOperand())
            LoadName = P->getName();
          OptimizationRemarkAnalysis Rem(REMARK_PASS, LName, Load);
          Rem << "Function " << NV("Function", F.getName())
              << " load_name=" << NV("LoadName", LoadName)
              << " load_line=" << NV("LoadLine", LoadLine)
              << " load_tbaa=" << NV("LoadTBAA", tbaaTypeName(Load))
              << " loop_header=" << NV("LoopHeader", bbLabel(L->getHeader()))
              << " writer_kind=" << NV("WriterKind", writerKind(W))
              << " writer_line=" << NV("WriterLine", WLine)
              << " writer_tbaa=" << NV("WriterTBAA", tbaaTypeName(W));
          ORE.emit(Rem);
        }
    }
  }
}

// 3-way reason for an unknown-BTC exit. Priority StoreReload > CallReload
// > Other so buckets are exclusive and total to the unknown-BTC count.
enum class ReloadReason { StoreReload, CallReload, Other };

// Classify why an in-loop load blocks SCEV, caching the result per-load in
// \p LoadCache: several leaf cmp operands (within and across exits in L) can
// fan in from the same load; don't re-walk L each time.
static ReloadReason
loadReloadCause(LoadInst *Load, Loop *L, AAResults &AA,
                DenseMap<LoadInst *, ReloadReason> &LoadCache) {
  auto It = LoadCache.find(Load);
  if (It != LoadCache.end())
    return It->second;
  MemoryLocation LoadLoc = MemoryLocation::get(Load);
  bool SawCallMod = false;
  ReloadReason Result = ReloadReason::Other;
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (&I == Load)
        continue;
      if (auto *SI = dyn_cast<StoreInst>(&I)) {
        if (LTAEmitExplain ? isModSet(AA.getModRefInfo(SI, LoadLoc))
                           : !AA.isNoAlias(MemoryLocation::get(SI), LoadLoc)) {
          Result = ReloadReason::StoreReload;
          goto done;
        }
        continue;
      }
      if (auto *MI = dyn_cast<MemIntrinsic>(&I)) {
        if (isModSet(AA.getModRefInfo(MI, LoadLoc))) {
          Result = ReloadReason::StoreReload;
          goto done;
        }
        continue;
      }
      if (auto *CB = dyn_cast<CallBase>(&I)) {
        if (CB->onlyReadsMemory() || CB->doesNotAccessMemory())
          continue;
        if (isModSet(AA.getModRefInfo(CB, LoadLoc)))
          SawCallMod = true;
      }
    }
  }
  if (SawCallMod)
    Result = ReloadReason::CallReload;
done:
  LoadCache[Load] = Result;
  return Result;
}

// Walk a branch's leaf cmp operands. For each whose SCEV references an
// in-loop SCEVUnknown, classify the blocker; the exit-level reason is the
// strongest cause across operands.
static ReloadReason
classifyExit(CondBrInst *BI, Loop *L, ScalarEvolution &SE, AAResults &AA,
             DenseMap<LoadInst *, ReloadReason> &LoadCache) {
  SmallVector<Value *, 8> Operands;
  SmallPtrSet<Value *, 16> Visited;
  collectBoolLeafOperands(BI->getCondition(), Operands, Visited);
  ReloadReason Best = ReloadReason::Other;
  for (Value *V : Operands) {
    if (!V || isa<Constant>(V))
      continue;
    if (!SE.isSCEVable(V->getType()))
      continue;
    const SCEV *SC = SE.getSCEV(V);
    if (isa<SCEVCouldNotCompute>(SC))
      continue;
    if (SE.isLoopInvariant(SC, L))
      continue;
    if (auto *AR = dyn_cast<SCEVAddRecExpr>(SC))
      if (AR->getLoop() == L)
        continue;
    SCEVNodeCollector Coll;
    SCEVTraversal<SCEVNodeCollector>(Coll).visitAll(SC);
    for (const SCEVUnknown *U : Coll.Unknowns) {
      auto *I = dyn_cast_or_null<Instruction>(U->getValue());
      if (!I || !L->contains(I->getParent()))
        continue;
      if (auto *Load = dyn_cast<LoadInst>(I)) {
        ReloadReason R = loadReloadCause(Load, L, AA, LoadCache);
        if (R == ReloadReason::StoreReload)
          return R;
        if (R == ReloadReason::CallReload && Best != ReloadReason::CallReload)
          Best = R;
      }
      // Non-load in-loop SCEVUnknown → genuine varying. Stays Other
      // unless another operand promotes us.
    }
  }
  return Best;
}

// Legacy bool: did *any* leaf operand reference an in-loop SCEVUnknown?
// (Superset of the reason buckets.) Kept for backwards-compat with
// classify_unknown_btc.py and existing dashboards.
static bool isBlockedByReload(CondBrInst *BI, Loop *L, ScalarEvolution &SE) {
  SmallVector<Value *, 8> Operands;
  SmallPtrSet<Value *, 16> Visited;
  collectBoolLeafOperands(BI->getCondition(), Operands, Visited);
  for (Value *V : Operands) {
    if (!V || isa<Constant>(V))
      continue;
    if (!SE.isSCEVable(V->getType()))
      continue;
    const SCEV *SC = SE.getSCEV(V);
    if (isa<SCEVCouldNotCompute>(SC))
      continue;
    if (SE.isLoopInvariant(SC, L))
      continue;
    if (auto *AR = dyn_cast<SCEVAddRecExpr>(SC))
      if (AR->getLoop() == L)
        continue;
    SCEVNodeCollector Coll;
    SCEVTraversal<SCEVNodeCollector>(Coll).visitAll(SC);
    for (const SCEVUnknown *U : Coll.Unknowns) {
      if (auto *I = dyn_cast_or_null<Instruction>(U->getValue()))
        if (L->contains(I->getParent()))
          return true;
    }
  }
  return false;
}

/// Emit one machine-readable LoopPrimitives remark per loop in F, plus a
/// per-function LoopPrimitivesSummary. Always emits (does not gate on
/// hasUnreachableInst) so every loop is captured, including trap-free ones —
/// lets the early/late diff find loops that disappear between pipeline points.
static void emitLoopPrimitives(Function &F, LoopInfo &LI,
                               OptimizationRemarkEmitter &ORE,
                               ScalarEvolution &SE, AAResults &AA,
                               DominatorTree &DT, StringRef Tag,
                               unsigned InvocationSeq) {
  unsigned TotalLoops = 0;
  unsigned Innermost = 0;
  unsigned LoopsWithTraps = 0;
  unsigned LoopsWithTrapsUnknownBTC = 0;
  unsigned MaxDepth = 0;
  unsigned Depth1 = 0, Depth2 = 0, Depth3Plus = 0;
  // Trap-condition shape histogram (sum across all loops in this function).
  unsigned ShapeInvariant = 0, ShapeIVDerived = 0, ShapeNonIV = 0;
  unsigned ShapeDominatesLatch = 0, ShapeNotDominatingLatch = 0;

  std::string PrimName = taggedName("LoopPrimitives", Tag);
  std::string SumName = taggedName("LoopPrimitivesSummary", Tag);

  for (auto *L : LI.getLoopsInPreorder()) {
    ++TotalLoops;
    bool IsInnermost = L->isInnermost();
    if (IsInnermost)
      ++Innermost;
    unsigned Depth = L->getLoopDepth();
    if (Depth > MaxDepth)
      MaxDepth = Depth;
    if (Depth == 1)
      ++Depth1;
    else if (Depth == 2)
      ++Depth2;
    else if (Depth >= 3)
      ++Depth3Plus;

    unsigned TrapExits = countTrapExits(L, LI);
    unsigned CondTrapEdges = countCondTrapEdges(L, LI);
    unsigned HoistableCondTrapEdges = countHoistableCondTrapEdges(L, LI);
    TrapCondShape Shape = classifyCondTrapEdges(L, SE, DT);
    ShapeInvariant += Shape.Invariant;
    ShapeIVDerived += Shape.IVDerived;
    ShapeNonIV += Shape.NonIV;
    ShapeDominatesLatch += Shape.DominatesLatch;
    ShapeNotDominatingLatch += Shape.NotDominatingLatch;
    bool BTCKnown = !isa<SCEVCouldNotCompute>(SE.getBackedgeTakenCount(L));
    if (TrapExits > 0 || CondTrapEdges > 0) {
      ++LoopsWithTraps;
      if (!BTCKnown)
        ++LoopsWithTrapsUnknownBTC;
    }

    // Per-exit SCEV-computability counts. IndVars partial-BTC needs every
    // exiting block's exit count SCEV-computable; counting unknowns split by
    // trap-vs-early shows what blocks eligibility.
    //   trap_exits_unknown_btc      — conditional exits targeting a trap block
    //                                  whose SE.getExitCount(L, BB) is
    //                                  SCEVCouldNotCompute
    //   non_trap_exits_unknown_btc  — same, for exits NOT targeting a trap
    //
    // Unknown-BTC exits further split by blocker reason:
    //   *_due_to_reload  — a cmp operand's SCEV (after select-OR/AND unfold)
    //                      refers to an in-loop SCEVUnknown: the predicate
    //                      reloads a value per iteration, so SCEV gives up.
    //   *_other_reason   — SCEV gave up otherwise (operand not AddRec for L,
    //                      not invariant, not an in-loop SCEVUnknown — rare;
    //                      AddRec for a different loop, modular non-unit
    //                      stride).
    //
    // Reload-blocked additionally subdivided by what blocked hoisting the load:
    //   *_store_reload   — a StoreInst / mem-intrinsic in L may-aliases the
    //                      load (AA). Common: char* / unsigned-char* writes AA
    //                      can't disambiguate from the bound-param storage.
    //   *_call_reload    — a side-effecting CallBase in L may-modifies the
    //                      load's location (AA), no aliasing store found. E.g.
    //                      C++ methods on `this` reachable through the same
    //                      allocation.
    //   *_other_blocker  — not a clean store/call match: non-load SCEVUnknown
    //                      (PHI/select/etc.), or a load with no aliasing
    //                      writer.
    // Sums: store_reload + call_reload + other_blocker = trap_exits_unknown_btc
    //       (same for non-trap).
    // One ordered, tag-keyed table drives the unknown-BTC breakdown so the set
    // generalizes: add a counter with a row here, not a local plus an NV line.
    // Field is the remark tag consumers read; Label is the full inline text
    // (leading space, trailing '='); the map is seeded in table order so
    // emission stays deterministic.
    static const std::pair<StringRef, StringRef> UnknownBTCCounters[] = {
        {"TrapExitsUnknownBTC", " trap_exits_unknown_btc="},
        {"TrapExitsUnknownBTCDueToReload", " trap_exits_unknown_btc_reload="},
        {"TrapExitsUnknownBTCOtherReason", " trap_exits_unknown_btc_other="},
        {"TrapExitsUnknownBTCStoreReload",
         " trap_exits_unknown_btc_store_reload="},
        {"TrapExitsUnknownBTCCallReload",
         " trap_exits_unknown_btc_call_reload="},
        {"TrapExitsUnknownBTCOtherBlocker",
         " trap_exits_unknown_btc_other_blocker="},
        {"NonTrapExitsUnknownBTC", " non_trap_exits_unknown_btc="},
        {"NonTrapExitsUnknownBTCDueToReload",
         " non_trap_exits_unknown_btc_reload="},
        {"NonTrapExitsUnknownBTCOtherReason",
         " non_trap_exits_unknown_btc_other="},
        {"NonTrapExitsUnknownBTCStoreReload",
         " non_trap_exits_unknown_btc_store_reload="},
        {"NonTrapExitsUnknownBTCCallReload",
         " non_trap_exits_unknown_btc_call_reload="},
        {"NonTrapExitsUnknownBTCOtherBlocker",
         " non_trap_exits_unknown_btc_other_blocker="},
    };
    MapVector<StringRef, unsigned> UnknownBTC;
    for (const auto &KV : UnknownBTCCounters)
      UnknownBTC[KV.first] = 0;
    // SCEV-computable counterparts: cond-trap edges whose per-exit BTC is
    // computable. These are the edges IndVars *could* fold via predication if
    // the other gates hold — they bound the achievable reduction from
    // compiler-side trap-edge work.
    //
    // Three-way SCEV bucket per exit, increasing strength:
    //   *Unknown*    — both Exact and SymbolicMax are CouldNotCompute.
    //   *Symbolic*   — Exact CouldNotCompute but SymbolicMax known (upper
    //                  bound). Folding the trap "never taken" needs comparing
    //                  the trap-exit's count to this bound.
    //   *Computable* — Exact known; cleanest case for predication.
    unsigned TrapExitsComputableBTC = 0;
    unsigned NonTrapExitsComputableBTC = 0;
    unsigned TrapExitsSymbolicBTC = 0;
    unsigned NonTrapExitsSymbolicBTC = 0;
    {
      auto IsTrapBB = [](BasicBlock *BB) {
        return isTrapBlock(BB, BoundsSafetyTrapsOnly,
                           LegacyTrapMatch::TrapIntrinsic);
      };

      // Cache per-load AA result: several leaf cmp operands (within and across
      // exits in L) can fan in from the same load; don't re-walk L each time.
      DenseMap<LoadInst *, ReloadReason> LoadCache;

      SmallVector<BasicBlock *, 4> ExitingBlocks;
      L->getExitingBlocks(ExitingBlocks);
      for (BasicBlock *EB : ExitingBlocks) {
        // Strict attribution: only count this exit if its source-BB's
        // immediate containing loop is L. Otherwise an inner-loop trap-exit --
        // whose unreachable successor exits every enclosing loop -- would be
        // counted at every nesting level's SCEV-bucket counters.
        if (LI.getLoopFor(EB) != L)
          continue;
        auto *BI = dyn_cast<CondBrInst>(EB->getTerminator());
        if (!BI)
          continue;
        BasicBlock *ExitSucc = nullptr;
        for (BasicBlock *Succ : BI->successors())
          if (!L->contains(Succ)) {
            ExitSucc = Succ;
            break;
          }
        if (!ExitSucc)
          continue;
        const SCEV *EC = SE.getExitCount(L, EB);
        bool IsTrap = IsTrapBB(ExitSucc);
        if (!isa<SCEVCouldNotCompute>(EC)) {
          if (IsTrap)
            ++TrapExitsComputableBTC;
          else
            ++NonTrapExitsComputableBTC;
          continue;
        }
        // Exact unknown; check the symbolic-max bucket before fully-unknown
        // attribution.
        const SCEV *SymEC =
            SE.getExitCount(L, EB, ScalarEvolution::SymbolicMaximum);
        if (!isa<SCEVCouldNotCompute>(SymEC)) {
          if (IsTrap)
            ++TrapExitsSymbolicBTC;
          else
            ++NonTrapExitsSymbolicBTC;
          continue;
        }
        bool ByReload = isBlockedByReload(BI, L, SE);
        ReloadReason R = classifyExit(BI, L, SE, AA, LoadCache);
        if (IsTrap) {
          ++UnknownBTC["TrapExitsUnknownBTC"];
          ++UnknownBTC[ByReload ? "TrapExitsUnknownBTCDueToReload"
                                : "TrapExitsUnknownBTCOtherReason"];
          ++UnknownBTC[R == ReloadReason::StoreReload
                           ? "TrapExitsUnknownBTCStoreReload"
                       : R == ReloadReason::CallReload
                           ? "TrapExitsUnknownBTCCallReload"
                           : "TrapExitsUnknownBTCOtherBlocker"];
        } else {
          ++UnknownBTC["NonTrapExitsUnknownBTC"];
          ++UnknownBTC[ByReload ? "NonTrapExitsUnknownBTCDueToReload"
                                : "NonTrapExitsUnknownBTCOtherReason"];
          ++UnknownBTC[R == ReloadReason::StoreReload
                           ? "NonTrapExitsUnknownBTCStoreReload"
                       : R == ReloadReason::CallReload
                           ? "NonTrapExitsUnknownBTCCallReload"
                           : "NonTrapExitsUnknownBTCOtherBlocker"];
        }
      }
    }

    std::string ParentHeader = "-";
    if (Loop *Parent = L->getParentLoop())
      ParentHeader = bbLabel(Parent->getHeader());

    OptimizationRemarkAnalysis Rem(REMARK_PASS, PrimName,
                                   &L->getHeader()->front());
    Rem << "Loop " << NV("LoopHeader", bbLabel(L->getHeader()))
        << " depth=" << NV("Depth", Depth)
        << " parent=" << NV("ParentHeader", ParentHeader)
        << " innermost=" << NV("IsInnermost", IsInnermost)
        << " blocks=" << NV("BlockCount", (unsigned)L->getNumBlocks())
        << " trap_exits=" << NV("TrapExitCount", TrapExits)
        << " cond_trap_edges=" << NV("CondTrapEdgeCount", CondTrapEdges)
        << " hoistable_cond_trap_edges="
        << NV("HoistableCondTrapEdges", HoistableCondTrapEdges)
        << " trap_cond_invariant=" << NV("TrapCondInvariant", Shape.Invariant)
        << " trap_cond_iv_derived=" << NV("TrapCondIVDerived", Shape.IVDerived)
        << " trap_cond_non_iv=" << NV("TrapCondNonIV", Shape.NonIV);
    if (LTAEmitExplain) {
      Rem << " trap_cond_dominates_latch="
          << NV("TrapCondDominatesLatch", Shape.DominatesLatch)
          << " trap_cond_not_dominating_latch="
          << NV("TrapCondNotDominatingLatch", Shape.NotDominatingLatch)
          << " trap_cond_dl_invariant="
          << NV("TrapCondDLInvariant", Shape.DLInvariant)
          << " trap_cond_dl_iv_derived="
          << NV("TrapCondDLIVDerived", Shape.DLIVDerived)
          << " trap_cond_dl_non_iv=" << NV("TrapCondDLNonIV", Shape.DLNonIV)
          << " trap_cond_ndl_invariant="
          << NV("TrapCondNDLInvariant", Shape.NDLInvariant)
          << " trap_cond_ndl_iv_derived="
          << NV("TrapCondNDLIVDerived", Shape.NDLIVDerived)
          << " trap_cond_ndl_non_iv=" << NV("TrapCondNDLNonIV", Shape.NDLNonIV)
          << " trap_cond_non_iv_load_op="
          << NV("TrapCondNonIVLoadOp", Shape.NonIVLoadOp)
          << " trap_cond_non_iv_phi_op="
          << NV("TrapCondNonIVPhiOp", Shape.NonIVPhiOp)
          << " trap_cond_non_iv_select_op="
          << NV("TrapCondNonIVSelectOp", Shape.NonIVSelectOp)
          << " trap_cond_non_iv_call_op="
          << NV("TrapCondNonIVCallOp", Shape.NonIVCallOp);
    }
    Rem << " btc_known=" << NV("BTCKnown", BTCKnown);
    for (const auto &[Field, Label] : UnknownBTCCounters)
      Rem << Label << NV(Field, UnknownBTC[Field]);
    Rem << " trap_exits_computable_btc="
        << NV("TrapExitsComputableBTC", TrapExitsComputableBTC)
        << " non_trap_exits_computable_btc="
        << NV("NonTrapExitsComputableBTC", NonTrapExitsComputableBTC)
        << " trap_exits_symbolic_btc="
        << NV("TrapExitsSymbolicBTC", TrapExitsSymbolicBTC)
        << " non_trap_exits_symbolic_btc="
        << NV("NonTrapExitsSymbolicBTC", NonTrapExitsSymbolicBTC);
    if (LTAEmitExplain)
      Rem << " invocation_seq=" << NV("InvocationSeq", InvocationSeq);
    ORE.emit(Rem);
  }

  OptimizationRemarkAnalysis Sum(REMARK_PASS, SumName, &F);
  // Per-function unique trap-block accounting: each trap BB counted once.
  // UniqueTrapBlocksInLoop = subset with at least one predecessor inside any
  // loop body (each loop-incident trap attributed once, independent of nesting
  // depth or incoming-edge count).
  auto IsTrapBlock = [](BasicBlock *BB) {
    return isTrapBlock(BB, BoundsSafetyTrapsOnly,
                       LegacyTrapMatch::TrapIntrinsic);
  };
  SmallPtrSet<BasicBlock *, 32> LoopBlocks;
  for (Loop *L : LI.getLoopsInPreorder())
    for (BasicBlock *BB : L->blocks())
      LoopBlocks.insert(BB);
  unsigned UniqueTrapBlocksTotal = 0;
  unsigned UniqueTrapBlocksReachableFromLoop = 0;
  // Edge-level counts: each cond-branch in any loop body that targets a
  // trap block counted once (no nesting overcount). Hoistable subset =
  // condition is loop-invariant for the innermost enclosing loop.
  unsigned InLoopTrapEdges = 0;
  unsigned InLoopHoistableTrapEdges = 0;
  // Symmetric counter for cond-branches OUTSIDE any loop body that target a
  // trap.
  unsigned OutOfLoopTrapEdges = 0;
  for (BasicBlock &BB : F) {
    if (!IsTrapBlock(&BB))
      continue;
    ++UniqueTrapBlocksTotal;
    for (BasicBlock *Pred : predecessors(&BB)) {
      if (LoopBlocks.count(Pred)) {
        ++UniqueTrapBlocksReachableFromLoop;
        break;
      }
    }
  }
  // Walk all BBs once; classify cond-branch-to-trap as in-loop or out-of-loop.
  for (BasicBlock &BB : F) {
    auto *BI = dyn_cast<CondBrInst>(BB.getTerminator());
    if (!BI)
      continue;
    bool BBInLoop = LoopBlocks.count(&BB) != 0;
    Loop *Inner = BBInLoop ? LI.getLoopFor(&BB) : nullptr;
    for (BasicBlock *Succ : BI->successors()) {
      if (BBInLoop && LoopBlocks.count(Succ))
        continue; // skip loop-internal succs
      if (!IsTrapBlock(Succ))
        continue;
      if (BBInLoop) {
        ++InLoopTrapEdges;
        if (Inner && Inner->isLoopInvariant(BI->getCondition()))
          ++InLoopHoistableTrapEdges;
      } else {
        ++OutOfLoopTrapEdges;
      }
    }
  }

  Sum << "Function " << NV("Function", F.getName())
      << " total_loops=" << NV("TotalLoops", TotalLoops)
      << " innermost=" << NV("Innermost", Innermost)
      << " loops_with_traps=" << NV("LoopsWithTraps", LoopsWithTraps)
      << " loops_with_traps_unknown_btc="
      << NV("LoopsWithTrapsUnknownBTC", LoopsWithTrapsUnknownBTC)
      << " max_depth=" << NV("MaxDepth", MaxDepth)
      << " depth1=" << NV("Depth1", Depth1)
      << " depth2=" << NV("Depth2", Depth2)
      << " depth3+=" << NV("Depth3Plus", Depth3Plus)
      << " unique_trap_blocks=" << NV("UniqueTrapBlocks", UniqueTrapBlocksTotal)
      << " unique_trap_blocks_reachable_from_loop="
      << NV("UniqueTrapBlocksReachableFromLoop",
            UniqueTrapBlocksReachableFromLoop)
      << " in_loop_trap_edges=" << NV("InLoopTrapEdges", InLoopTrapEdges)
      << " in_loop_hoistable_trap_edges="
      << NV("InLoopHoistableTrapEdges", InLoopHoistableTrapEdges)
      << " out_of_loop_trap_edges="
      << NV("OutOfLoopTrapEdges", OutOfLoopTrapEdges)
      << " trap_cond_invariant_total="
      << NV("TrapCondInvariantTotal", ShapeInvariant)
      << " trap_cond_iv_derived_total="
      << NV("TrapCondIVDerivedTotal", ShapeIVDerived)
      << " trap_cond_non_iv_total=" << NV("TrapCondNonIVTotal", ShapeNonIV);
  if (LTAEmitExplain) {
    Sum << " trap_cond_dominates_latch_total="
        << NV("TrapCondDominatesLatchTotal", ShapeDominatesLatch)
        << " trap_cond_not_dominating_latch_total="
        << NV("TrapCondNotDominatingLatchTotal", ShapeNotDominatingLatch)
        << " invocation_seq=" << NV("InvocationSeq", InvocationSeq);
  }
  ORE.emit(Sum);
}

/// Check if \p L can be hoisted or not and emit a detailed remark about why
/// it can't be hoisted.
static CheckLoopHoistType processLoops(Loop *L, ScalarEvolution &SE,
                                       LoopInfo &LI,
                                       OptimizationRemarkEmitter &ORE,
                                       StringRef Tag) {
  CheckLoopHoistType HoistType;
  bool SymbolicMaxBackEdgeComputable =
      !isa<SCEVCouldNotCompute>(SE.getSymbolicMaxBackedgeTakenCount(L));
  bool HasSideEffects = false;
  if (!hasUnreachableInst(L))
    return CheckLoopHoistType::SKIP;

  unsigned TrapCount = countTrapExits(L, LI);

  SmallVector<std::string, 4> InstructionsWithSideEffects;
  SmallVector<std::string, 4> SideEffectReasons;
  for (auto *BB : L->blocks()) {
    if (any_of(*BB, [&InstructionsWithSideEffects,
                     &SideEffectReasons](const Instruction &I) {
          bool InstHasSideEffects = false;
          // If a call instruction reads or writes to memory we don't know if
          // the access is non-volatile so we asssume that the call instruction
          // has side effects.
          if (isa<CallInst>(I))
            InstHasSideEffects =
                I.mayHaveSideEffects() || I.mayReadFromMemory();
          else if (LTAEmitExplain)
            InstHasSideEffects = !I.willReturn() || I.mayThrow();
          else
            InstHasSideEffects = I.mayHaveSideEffects();
          if (InstHasSideEffects) {
            std::string Buf;
            raw_string_ostream OS(Buf);
            I.print(OS);
            InstructionsWithSideEffects.push_back(Buf);
            SideEffectReasons.push_back(getSideEffectReasons(I));
          }
          return InstHasSideEffects;
        })) {

      HasSideEffects = true;
      break;
    }
  }

  HoistType = !HasSideEffects && SymbolicMaxBackEdgeComputable
                  ? CheckLoopHoistType::MAYBE_CAN_HOIST
                  : CheckLoopHoistType::CANNOT_HOIST;
  // Emit a remark for the loop.
  // NB: `OptimizationRemarkAnalysis` stores its `RemarkName` as a `StringRef`.
  // Passing `taggedName(...)` inline would create a temporary `std::string`
  // destroyed at the end of the full expression, leaving the stored `StringRef`
  // dangling and producing garbled `Name:` fields in YAML. Hold the name in a
  // local that outlives the remark.
  std::string TrapRemarkName = taggedName("LoopTrap", Tag);
  auto ORA = OptimizationRemarkAnalysis(REMARK_PASS, TrapRemarkName,
                                        &L->getHeader()->front());
  ORA << "Loop: " << L->getName() << " "
      << "TrapExits: " << NV("TrapExitCount", TrapCount) << " ";
  if (HoistType == CheckLoopHoistType::CANNOT_HOIST) {
    ORA << "cannot be hoisted: \n";
    if (HasSideEffects) {
      ORA << "\nThe following instructions have side effects:\n";
      for (unsigned Idx = 0; Idx < InstructionsWithSideEffects.size(); Idx++) {
        ORA << "\t" << InstructionsWithSideEffects[Idx] << "\n";
        ORA << "Reason:\n";
        ORA << SideEffectReasons[Idx];
      }
    }
    if (!SymbolicMaxBackEdgeComputable)
      ORA << "Backedge is not computable.\n";
  } else
    ORA << "can be hoisted\n";
  ORE.emit(ORA);
  return HoistType;
}

/// Collect info for hoistable loop checks for \p F and report remarks for
/// individual loops and report a summary for hoistable checks for the function.
static void emitRemarks(Function &F, LoopInfo &LI,
                        OptimizationRemarkEmitter &ORE, ScalarEvolution &SE,
                        StringRef Tag) {
  unsigned TotalCanHoistLoops = 0;
  unsigned TotalUnHoistableLoops = 0;
  unsigned TotalHoistedLoops = 0;
  for (auto *L : LI.getLoopsInPreorder()) {
    // Check for .hoisted instructions in preheader (successfully hoisted)
    SmallVector<std::string, 4> PreheaderHoisted;
    if (hasHoistedInPreheader(L, PreheaderHoisted)) {
      TotalHoistedLoops++;
      // RemarkName must outlive the remark — see comment at the
      // earlier LoopTrap remark for context.
      std::string HoistRemarkName = taggedName("LoopTrapHoisted", Tag);
      OptimizationRemarkAnalysis HoistRem(REMARK_PASS, HoistRemarkName,
                                          &L->getHeader()->front());
      HoistRem << "Loop: " << L->getName()
               << " has trap check hoisted to preheader\n";
      for (const auto &Inst : PreheaderHoisted)
        HoistRem << "\t" << Inst << "\n";
      ORE.emit(HoistRem);
    }

    CheckLoopHoistType Type = processLoops(L, SE, LI, ORE, Tag);
    if (Type == CheckLoopHoistType::MAYBE_CAN_HOIST)
      TotalCanHoistLoops++;
    else if (Type == CheckLoopHoistType::CANNOT_HOIST)
      TotalUnHoistableLoops++;
  }

  // RemarkName must outlive the remark — see earlier comment.
  std::string SummaryRemarkName = taggedName("LoopTrapSummary", Tag);
  OptimizationRemarkAnalysis Rem(REMARK_PASS, SummaryRemarkName, &F);
  Rem << "Trap checks results:\n";
  Rem << "Total count of loops with traps "
      << NV("TotalCount", TotalCanHoistLoops + TotalUnHoistableLoops) << "\n";
  Rem << "Loops that maybe can be hoisted: "
      << NV("CountHoist", TotalCanHoistLoops) << "\n";
  Rem << "Loops that cannot be hoisted: "
      << NV("CountCannotHoist", TotalUnHoistableLoops) << "\n";
  Rem << "Loops with trap check hoisted to preheader: "
      << NV("CountHoisted", TotalHoistedLoops) << "\n";
  ORE.emit(Rem);
}

PreservedAnalyses LoopTrapAnalysisPass::run(Function &F,
                                            FunctionAnalysisManager &AM) {
  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
  auto &ORE = AM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  auto &AA = AM.getResult<AAManager>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  // Per-function run() counter, emitted as InvocationSeq (gated by
  // -loop-trap-analysis-explain) so repeated invocations can be deduped by
  // max(seq) per (function, src_bb, trap_bb); stays 1 for a single run.
  unsigned Seq = ++InvocationCount[&F];
  emitLoopPrimitives(F, LI, ORE, SE, AA, DT, Tag, Seq);
  emitRemarks(F, LI, ORE, SE, Tag);
  emitPerTrapEdgeSCEV(F, LI, SE, AA, DT, ORE, Tag, Seq);
  return PreservedAnalyses::all();
}

void LoopTrapAnalysisPass::printPipeline(
    raw_ostream &OS, function_ref<StringRef(StringRef)> MapClassName2PassName) {
  OS << "loop-trap-analysis";
  if (!Tag.empty())
    OS << "<tag=" << Tag << ">";
}
