//===-- LoopTrapAnalysis.cpp - Loop Trap Count pass -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// LoopTrapAnalysis emits machine-readable opt-remark records describing each
// conditional branch to a trap block (a bounds/overflow check lowered to
// br+@llvm.trap). Under -loop-trap-analysis-explain, each LoopTrapEdge record
// carries the edge's position within its loop (loop header, depth, whether the
// edge exits the loop, and whether the loop is innermost) so a consumer can
// classify it from the fields alone.
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
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Remarks/BoundsSafetyOptRemarks.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace llvm::ore;
#define DEBUG_TYPE "loop-trap-analysis"
#define REMARK_PASS DEBUG_TYPE

enum class CheckLoopHoistType { MAYBE_CAN_HOIST, CANNOT_HOIST, SKIP };
static cl::opt<bool> NewTrapSemantics(
    "use-new-trap-semantics", cl::init(false),
    cl::desc("Assume that traps are using the new trap semantics "
             "logic."));
static cl::opt<bool> BoundsSafetyTrapsOnly(
    "use-bounds-safety-traps-only", cl::init(false),
    cl::desc(
        "We only check for -fbounds-safety traps if the flag is false we can check "
        "for any hoistable traps."));
static cl::opt<bool> LTAEmitExplain(
    "loop-trap-analysis-explain", cl::init(false),
    cl::desc("Emit the per-trap-edge explain analysis: one LoopTrapEdge remark "
             "per conditional branch to a trap block. Off by default, so the "
             "base remark output is unchanged."));
static cl::opt<unsigned> EntryProximityDepth(
    "loop-trap-entry-proximity-depth", cl::init(3),
    cl::desc("Maximum dominator depth from function entry at which a "
             "trap edge is classified IsEntryProximate (default 3)."));
static cl::opt<bool> LTAEmitLoadAlias(
    "loop-load-alias", cl::init(false),
    cl::desc("Opt-in: for each load in an INNERMOST loop that is may-clobbered "
             "by an in-loop writer (store / memcpy / call), emit a "
             "LoopLoadAlias record naming the first such writer. Clobbered "
             "loads only (hoistable loads are omitted). Off by default; "
             "enable selectively as it emits one record per clobbered load."));

/// Print a stable, non-empty label for \p BB, so remark args that identify
/// BasicBlocks stay useful when the BB has no source-level name .
static std::string bbLabel(const BasicBlock *BB) {
  if (BB->hasName())
    return BB->getName().str();
  std::string S;
  raw_string_ostream OS(S);
  BB->printAsOperand(OS, /*PrintType=*/false);
  return S;
}

/// Minimal trap-block predicate for the per-edge explain output.
static bool isTrapEdgeBlock(BasicBlock *BB) {
  Instruction *Term = BB->getTerminator();
  if (!isa<UnreachableInst>(Term) || Term == &BB->front())
    return false;
  auto *CI = dyn_cast<CallInst>(Term->getPrevNode());
  // Trap intrinsic: noreturn and accesses inaccessible memory.
  return isa_and_nonnull<IntrinsicInst>(CI) && CI->doesNotReturn() &&
         CI->onlyAccessesInaccessibleMemory();
}

/// Count trap loop-exit edges of Loop
static unsigned countTrapExits(Loop *L, LoopInfo &LI) {
  unsigned Count = 0;
  SmallVector<BasicBlock *, 4> LoopExitBlocks;
  L->getExitBlocks(LoopExitBlocks);
  for (auto *BB : LoopExitBlocks) {
    if (!isTrapEdgeBlock(BB))
      continue;
    if (any_of(predecessors(BB), [L, &LI](BasicBlock *PredB) {
          if (LI.getLoopFor(PredB) != L)
            return false;
          auto *TerminatorInst = PredB->getTerminator();
          return L->contains(PredB) && (isa<CondBrInst>(TerminatorInst) ||
                                        isa<UncondBrInst>(TerminatorInst) ||
                                        isa<SwitchInst>(TerminatorInst));
        }))
      ++Count;
  }
  return Count;
}

// 3-way reason for an unknown-BTC exit. Priority StoreReload > CallReload
// > Other so buckets are exclusive and total to the unknown-BTC count.
enum class ReloadReason { StoreReload, CallReload, Other };

// The unknown-BTC breakdown counters emitted on LoopPrimitives are computed by
// classifyExit / isBlockedByReload; these depend on collectIcmpBoolOperands and
// SCEVNodeCollector, which are defined below, so forward-declare them here.
static ReloadReason
loadReloadCause(LoadInst *Load, Loop *L, AAResults &AA,
                DenseMap<LoadInst *, ReloadReason> &LoadCache);
static ReloadReason classifyExit(CondBrInst *BI, Loop *L, ScalarEvolution &SE,
                                 AAResults &AA,
                                 DenseMap<LoadInst *, ReloadReason> &LoadCache);
static bool isBlockedByReload(CondBrInst *BI, Loop *L, ScalarEvolution &SE);

/// Emit one LoopPrimitives remark per loop with a trap exit (trap-free loops
/// emit none), plus a per-function LoopPrimitivesSummary with loop-depth
/// tallies over all loops. Gated by -loop-trap-analysis-explain.
static void emitLoopPrimitives(Function &F, LoopInfo &LI,
                               OptimizationRemarkEmitter &ORE,
                               ScalarEvolution &SE, AAResults &AA,
                               unsigned InvocationSeq) {
  unsigned TotalLoops = 0;
  unsigned Innermost = 0;
  unsigned MaxDepth = 0;
  unsigned Depth1 = 0, Depth2 = 0, Depth3Plus = 0;

  std::string PrimName = "LoopPrimitives";
  std::string SumName = "LoopPrimitivesSummary";

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
    // loop-trap-analysis: only loops with a trap exit get a record (trap-free
    // loops still count toward the summary below).
    if (TrapExits == 0)
      continue;
    bool BTCKnown = !isa<SCEVCouldNotCompute>(SE.getBackedgeTakenCount(L));

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
    //                      load (AA).
    //   *_call_reload    — a side-effecting CallBase in L may-modifies the
    //                      load's location (AA), no aliasing store found.
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
    //                  bound).
    //   *Computable* — Exact known; cleanest case for predication.
    unsigned TrapExitsComputableBTC = 0;
    unsigned NonTrapExitsComputableBTC = 0;
    unsigned TrapExitsSymbolicBTC = 0;
    unsigned NonTrapExitsSymbolicBTC = 0;
    {
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
        bool IsTrap = isTrapEdgeBlock(ExitSucc);
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
        << " btc_known=" << NV("BTCKnown", BTCKnown);
    for (const auto &[Field, Label] : UnknownBTCCounters)
      Rem << Label << NV(Field, UnknownBTC[Field]);
    Rem << " trap_exits_computable_btc="
        << NV("TrapExitsComputableBTC", TrapExitsComputableBTC)
        << " non_trap_exits_computable_btc="
        << NV("NonTrapExitsComputableBTC", NonTrapExitsComputableBTC)
        << " trap_exits_symbolic_btc="
        << NV("TrapExitsSymbolicBTC", TrapExitsSymbolicBTC)
        << " non_trap_exits_symbolic_btc="
        << NV("NonTrapExitsSymbolicBTC", NonTrapExitsSymbolicBTC)
        << " invocation_seq=" << NV("InvocationSeq", InvocationSeq);
    ORE.emit(Rem);
  }

  OptimizationRemarkAnalysis Sum(REMARK_PASS, SumName, &F);
  Sum << "Function " << NV("Function", F.getName())
      << " total_loops=" << NV("TotalLoops", TotalLoops)
      << " innermost=" << NV("Innermost", Innermost)
      << " max_depth=" << NV("MaxDepth", MaxDepth)
      << " depth1=" << NV("Depth1", Depth1)
      << " depth2=" << NV("Depth2", Depth2)
      << " depth3+=" << NV("Depth3Plus", Depth3Plus);
  ORE.emit(Sum);
}

/// Recursively unfold a boolean expression chain (select-OR, select-AND,
/// or/and) into the leaf comparison operands, optionally also collecting the
/// leaf ICmpInsts (for their signedness).
static void
collectIcmpBoolOperands(Value *V, SmallVectorImpl<Value *> &Operands,
                        SmallPtrSetImpl<Value *> &Visited,
                        SmallVectorImpl<ICmpInst *> *ICmps = nullptr,
                        int Depth = 0) {
  if (!V || !Visited.insert(V).second || Depth > 8)
    return;
  if (auto *SI = dyn_cast<SelectInst>(V)) {
    Value *T = SI->getTrueValue(), *FV = SI->getFalseValue();
    if (auto *TC = dyn_cast<ConstantInt>(T))
      if (TC->isOne()) {
        collectIcmpBoolOperands(SI->getCondition(), Operands, Visited, ICmps,
                                Depth + 1);
        collectIcmpBoolOperands(FV, Operands, Visited, ICmps, Depth + 1);
        return;
      }
    if (auto *FC = dyn_cast<ConstantInt>(FV))
      if (FC->isZero()) {
        collectIcmpBoolOperands(SI->getCondition(), Operands, Visited, ICmps,
                                Depth + 1);
        collectIcmpBoolOperands(T, Operands, Visited, ICmps, Depth + 1);
        return;
      }
  }
  if (auto *BO = dyn_cast<BinaryOperator>(V)) {
    if (BO->getOpcode() == Instruction::Or ||
        BO->getOpcode() == Instruction::And) {
      collectIcmpBoolOperands(BO->getOperand(0), Operands, Visited, ICmps,
                              Depth + 1);
      collectIcmpBoolOperands(BO->getOperand(1), Operands, Visited, ICmps,
                              Depth + 1);
      return;
    }
  }
  if (auto *Cmp = dyn_cast<CmpInst>(V)) {
    if (ICmps)
      if (auto *IC = dyn_cast<ICmpInst>(Cmp))
        ICmps->push_back(IC);
    Operands.push_back(Cmp->getOperand(0));
    Operands.push_back(Cmp->getOperand(1));
    return;
  }
  // Opaque — record as itself.
  Operands.push_back(V);
}

/// Collect the SCEVUnknown and SCEVAddRecExpr nodes reachable from a SCEV
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

/// Structural shape of a trap branch's i1 predicate.
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

/// Match the bounded-iterator OR / AND shape:
///   OR  form: or (uge X, B)  (ult (sub B, X), K)
///   AND form: and(ult X, B)  (uge (sub B, X), K)
/// Returns true iff one arm ordering matches the requested (predicate-pair,
/// K-kind) tuple. K-kind is constant for the *ConstK shapes and an L-affine
/// SCEVAddRecExpr for the *VarK shapes.
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

/// True when V is an icmp with an operand whose SCEV is an AddRec for L.
static bool icmpHasLoopAddRecOperand(Value *V, ScalarEvolution &SE, Loop *L) {
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
  return icmpHasLoopAddRecOperand(Arm1, SE, L) &&
         icmpHasLoopAddRecOperand(Arm2, SE, L);
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

/// HEURISTIC that estimates which trap checks are likely precondition
/// validation traps for incoming (function-argument) values
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

namespace {
/// SCEV nature of a trap predicate's leaf operands.
struct OperandSCEVInfo {
  unsigned NumLeafOps = 0;
  bool IsAffine = false;
  bool HasInLoopUnknown = false;
  bool HasNonUnitStrideForLAddRec = false;
  bool HasNegativeStrideForLAddRec = false;
  bool HasNonConstantStrideForLAddRec = false;
  bool NotProvenMonotonic = false;
};
/// Per-edge exit count for the exiting block (not the loop's overall count).
struct EdgeBTCInfo {
  bool Computable = false;
  bool Symbolic = false;
};
struct DominanceInfo {
  bool DominatesLatch = false;
  bool LoopLatchBTCComputable = false;
};
} // anonymous namespace

/// SCEV nature of the leaf operands of trap predicate Cond:
///   IsAffine           — some operand SCEV is an affine AddRec.
///   HasInLoopUnknown   — some operand is a SCEVUnknown defined in a containing
///                        loop; opaque to SCEV.
///   NotProvenMonotonic — a compared affine AddRec lacks the no-wrap flag its
///                        comparison needs (nuw for unsigned, nsw for signed).
/// Stride flags (informational) for an affine innermost-loop AddRec:
///   HasNonUnitStrideForLAddRec     — |step| > 1.
///   HasNegativeStrideForLAddRec    — negative constant step.
///   HasNonConstantStrideForLAddRec — runtime (non-constant) step.
static OperandSCEVInfo
computeOperandSCEVInfo(Value *Cond, ScalarEvolution &SE, LoopInfo &LI,
                       Loop *Innermost, ArrayRef<Loop *> ContainingLoops) {
  OperandSCEVInfo R;
  SmallVector<Value *, 8> LeafOperands;
  SmallVector<ICmpInst *, 4> LeafICmps;
  SmallPtrSet<Value *, 16> Visited;
  collectIcmpBoolOperands(Cond, LeafOperands, Visited, &LeafICmps);
  R.NumLeafOps = LeafOperands.size();
  for (Value *V : LeafOperands) {
    if (!V || isa<Constant>(V) || !SE.isSCEVable(V->getType()))
      continue;
    const SCEV *SC = SE.getSCEV(V);
    if (isa<SCEVCouldNotCompute>(SC))
      continue;
    SCEVNodeCollector Coll;
    SCEVTraversal<SCEVNodeCollector>(Coll).visitAll(SC);
    for (const SCEVAddRecExpr *AR : Coll.AddRecs)
      if (AR->isAffine())
        R.IsAffine = true;
    // A SCEVUnknown whose defining instruction lives in a containing loop is
    // opaque to SCEV (a load/call the trip count cannot see through).
    for (const SCEVUnknown *U : Coll.Unknowns) {
      auto *I = dyn_cast_or_null<Instruction>(U->getValue());
      if (!I)
        continue;
      Loop *DefLoop = LI.getLoopFor(I->getParent());
      for (Loop *DL = DefLoop; DL && !R.HasInLoopUnknown;
           DL = DL->getParentLoop())
        for (Loop *CL : ContainingLoops)
          if (DL == CL) {
            R.HasInLoopUnknown = true;
            break;
          }
      if (R.HasInLoopUnknown)
        break;
    }
    // Stride fragility of an affine innermost-loop AddRec (informational).
    if (Innermost) {
      for (const SCEVAddRecExpr *AR : Coll.AddRecs) {
        if (AR->getLoop() != Innermost || !AR->isAffine())
          continue;
        if (auto *StepC = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE))) {
          const APInt &Step = StepC->getAPInt();
          if (Step.isNegative())
            R.HasNegativeStrideForLAddRec = true;
          APInt AbsStep = Step.isNegative() ? -Step : Step;
          if (AbsStep.ugt(1))
            R.HasNonUnitStrideForLAddRec = true;
        } else {
          R.HasNonUnitStrideForLAddRec = true;
          R.HasNonConstantStrideForLAddRec = true;
        }
      }
    }
  }
  // NotProvenMonotonic: an affine innermost-loop AddRec compared without the
  // no-wrap flag its comparison needs (unsigned -> nuw, signed -> nsw), so the
  // trip count cannot be bounded.
  for (ICmpInst *Cmp : LeafICmps) {
    if (Cmp->isEquality())
      continue;
    bool Signed = Cmp->isSigned();
    for (Value *Op : {Cmp->getOperand(0), Cmp->getOperand(1)}) {
      if (!SE.isSCEVable(Op->getType()))
        continue;
      auto *AR = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(Op));
      if (!AR || AR->getLoop() != Innermost || !AR->isAffine())
        continue;
      if (Signed ? !AR->hasNoSignedWrap() : !AR->hasNoUnsignedWrap())
        R.NotProvenMonotonic = true;
    }
  }
  return R;
}

/// Per-edge exit count for exiting block BB (not the loop's overall
/// backedge-taken count): Computable is the exact SE.getExitCount; Symbolic is
/// the SymbolicMaximum bucket, checked only when the exact count is unknown.
static EdgeBTCInfo computeEdgeBTC(ScalarEvolution &SE, Loop *Innermost,
                                  BasicBlock *BB, bool IsLoopExit) {
  EdgeBTCInfo R;
  if (Innermost && IsLoopExit) {
    const SCEV *EC = SE.getExitCount(Innermost, BB);
    R.Computable = !isa<SCEVCouldNotCompute>(EC);
    if (!R.Computable) {
      const SCEV *SymEC =
          SE.getExitCount(Innermost, BB, ScalarEvolution::SymbolicMaximum);
      R.Symbolic = !isa<SCEVCouldNotCompute>(SymEC);
    }
  }
  return R;
}

/// Whether the trap branch's BB dominates the latch (the condition fires every
/// iteration) and whether the loop's latch exit count is computable.
static DominanceInfo computeDominanceInfo(ScalarEvolution &SE,
                                          const DominatorTree &DT,
                                          Loop *Innermost, BasicBlock *BB) {
  DominanceInfo R;
  BasicBlock *Latch = Innermost ? Innermost->getLoopLatch() : nullptr;
  R.DominatesLatch = Latch && DT.dominates(BB, Latch);
  if (Latch)
    R.LoopLatchBTCComputable =
        !isa<SCEVCouldNotCompute>(SE.getExitCount(Innermost, Latch));
  return R;
}

/// Human-readable TBAA access-type name for an instruction, or "" if none.
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

/// Short kind tag for a reload-blocking writer.
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

/// Cached (Loop, Load) query: do in-loop stores / mem-intrinsics / calls
/// may-alias the load? Tracked separately so the consumer can distinguish
/// scalar-store from struct-copy (mem-intrinsic) aliasing.
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

/// First in-loop store / mem-intrinsic that may-alias the load, or null.
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

/// Like firstAliasingStore but also considers side-effecting calls (used by
/// the flag-gated LoopLoadAlias annotation).
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

namespace {
/// Reload / opaque-operand classification: why an in-loop unknown operand
/// blocks the trip count. Only in-loop SCEVUnknown operands contribute.
struct ReloadOpaqueInfo {
  bool HasStoreReload = false;
  bool HasMemIntrinsicReload = false;
  bool HasCallReload = false;
  bool HasUnaliasedLoadOperand = false;
  bool HasInLoopPhiOperand = false;
  bool HasInLoopFreezeOperand = false;
  bool HasInLoopSelectOperand = false;
  bool HasOtherInLoopUnknownOperand = false;
  bool HasOpaqueOperandNoInLoopUnknown = false;
  bool HasOuterLoopAddRecOperand = false;
  Instruction *ReloadStore = nullptr;
  LoadInst *ReloadLoad = nullptr;
};
} // anonymous namespace

/// Classify each in-loop opaque operand of trap predicate Cond: AA-based reload
/// (store / mem-intrinsic / call) vs phi / freeze / select / other, plus the
/// outer-loop-AddRec and opaque-without-in-loop-unknown buckets.
static ReloadOpaqueInfo computeReloadOpaqueInfo(
    Value *Cond, ScalarEvolution &SE, LoopInfo &LI, Loop *Innermost,
    AAResults &AA,
    DenseMap<std::pair<Loop *, LoadInst *>, std::tuple<bool, bool, bool>>
        &ReloadCache) {
  ReloadOpaqueInfo R;
  SmallVector<Value *, 8> LeafOperands;
  SmallPtrSet<Value *, 16> Visited;
  collectIcmpBoolOperands(Cond, LeafOperands, Visited);
  for (Value *V : LeafOperands) {
    if (!V || isa<Constant>(V) || !SE.isSCEVable(V->getType()))
      continue;
    const SCEV *SC = SE.getSCEV(V);
    if (isa<SCEVCouldNotCompute>(SC))
      continue;
    SCEVNodeCollector Coll;
    SCEVTraversal<SCEVNodeCollector>(Coll).visitAll(SC);
    // Opaque w.r.t. L: non-invariant and not an innermost-loop AddRec.
    bool OpIsOpaqueForL = false;
    if (Innermost) {
      OpIsOpaqueForL = !SE.isLoopInvariant(SC, Innermost);
      if (auto *AR = dyn_cast<SCEVAddRecExpr>(SC))
        if (AR->getLoop() == Innermost)
          OpIsOpaqueForL = false;
    }
    bool OpHasInLoopUnknown = false;
    for (const SCEVUnknown *U : Coll.Unknowns) {
      auto *I = dyn_cast_or_null<Instruction>(U->getValue());
      if (!I || !Innermost || !Innermost->contains(I->getParent()))
        continue;
      OpHasInLoopUnknown = true;
      if (auto *Load = dyn_cast<LoadInst>(I)) {
        auto [SAlias, MAlias, CAlias] =
            loadAliasFlags(Load, Innermost, AA, ReloadCache);
        if (SAlias)
          R.HasStoreReload = true;
        if (MAlias)
          R.HasMemIntrinsicReload = true;
        if (CAlias)
          R.HasCallReload = true;
        if ((SAlias || MAlias) && !R.ReloadStore) {
          R.ReloadStore = firstAliasingStore(Load, Innermost, AA);
          R.ReloadLoad = Load;
        }
        if (!SAlias && !MAlias && !CAlias)
          R.HasUnaliasedLoadOperand = true;
      } else if (isa<PHINode>(I)) {
        R.HasInLoopPhiOperand = true;
      } else {
        R.HasOtherInLoopUnknownOperand = true;
        if (isa<FreezeInst>(I))
          R.HasInLoopFreezeOperand = true;
        else if (isa<SelectInst>(I))
          R.HasInLoopSelectOperand = true;
      }
    }
    if (Innermost)
      for (const SCEVAddRecExpr *AR : Coll.AddRecs)
        if (AR->getLoop() != Innermost) {
          R.HasOuterLoopAddRecOperand = true;
          break;
        }
    if (OpIsOpaqueForL && !OpHasInLoopUnknown)
      R.HasOpaqueOperandNoInLoopUnknown = true;
  }
  return R;
}

// Classify why an in-loop load blocks SCEV, caching the result per-load in
// LoadCache: several leaf cmp operands (within and across exits in L) can
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
  collectIcmpBoolOperands(BI->getCondition(), Operands, Visited);
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
// (Superset of the reason buckets.)
static bool isBlockedByReload(CondBrInst *BI, Loop *L, ScalarEvolution &SE) {
  SmallVector<Value *, 8> Operands;
  SmallPtrSet<Value *, 16> Visited;
  collectIcmpBoolOperands(BI->getCondition(), Operands, Visited);
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

/// Emit one LoopTrapEdge remark per conditional branch whose one successor is a
/// trap block
static void emitPerTrapEdge(Function &F, LoopInfo &LI,
                            OptimizationRemarkEmitter &ORE, ScalarEvolution &SE,
                            AAResults &AA, const DominatorTree &DT,
                            unsigned InvocationSeq) {
  std::string Name = "LoopTrapEdge";
  DenseMap<std::pair<Loop *, LoadInst *>, std::tuple<bool, bool, bool>>
      ReloadCache;
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
        if (!L->contains(Succ) && isTrapEdgeBlock(Succ)) {
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
  for (BasicBlock &BB : F) {
    auto *BI = dyn_cast<CondBrInst>(BB.getTerminator());
    if (!BI)
      continue;
    BasicBlock *TrapSucc = nullptr;
    for (BasicBlock *Succ : BI->successors())
      if (isTrapEdgeBlock(Succ)) {
        TrapSucc = Succ;
        break;
      }
    if (!TrapSucc)
      continue;

    Loop *Innermost = LI.getLoopFor(&BB);
    // Chain of containing loops for the source BB, innermost first. Used to
    // decide whether an operand's SCEVUnknown is defined inside the nest.
    SmallVector<Loop *, 4> ContainingLoops;
    for (Loop *L = Innermost; L; L = L->getParentLoop())
      ContainingLoops.push_back(L);

    bool IsLoopExit = Innermost && !Innermost->contains(TrapSucc);

    // Predicate-tree shape (in-loop and out-of-loop edges alike). Sizes the
    // reach of shape-specific levers: NUW propagation on a bounds-check OR
    // targets OrBoundsCheckConstBound; the AND-form lever targets
    // AndBoundsCheckConstBound; the variable-bound extension targets the
    // *BoundsCheckVarBound shapes; a generic SCEV exit-count extension targets
    // {Or,And}TwoAddRecICmp.
    TrapPredicateShape PredShape =
        classifyTrapPredicateShape(BI->getCondition(), SE, Innermost);

    OperandSCEVInfo SF = computeOperandSCEVInfo(BI->getCondition(), SE, LI,
                                                Innermost, ContainingLoops);

    EdgeBTCInfo BTC = computeEdgeBTC(SE, Innermost, &BB, IsLoopExit);
    if (BTC.Computable)
      SF.NotProvenMonotonic = false;
    unsigned NUnc = UncomputableTrapExits.lookup(Innermost);
    bool LoopHasOtherUnknownBTCTrap = BTC.Computable ? (NUnc > 0) : (NUnc > 1);

    // Out-of-loop / structural-redundancy fields.
    bool IsEntryProx = isEntryProximate(F, &BB, DT, LI);

    DominanceInfo Dom = computeDominanceInfo(SE, DT, Innermost, &BB);

    ReloadOpaqueInfo RO = computeReloadOpaqueInfo(BI->getCondition(), SE, LI,
                                                  Innermost, AA, ReloadCache);
    unsigned ReloadStoreLine = 0, ReloadLoadLine = 0;
    StringRef ReloadStoreTBAA, ReloadStoreKind, ReloadLoadTBAA, ReloadLoadName;
    if (RO.ReloadStore) {
      if (const DebugLoc &DL = RO.ReloadStore->getDebugLoc())
        ReloadStoreLine = DL.getLine();
      ReloadStoreTBAA = tbaaTypeName(RO.ReloadStore);
      ReloadStoreKind = writerKind(RO.ReloadStore);
    }
    if (RO.ReloadLoad) {
      ReloadLoadTBAA = tbaaTypeName(RO.ReloadLoad);
      if (const DebugLoc &DL = RO.ReloadLoad->getDebugLoc())
        ReloadLoadLine = DL.getLine();
      if (Value *Ptr = RO.ReloadLoad->getPointerOperand())
        ReloadLoadName = Ptr->getName();
    }

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
        << " num_leaf_operands=" << NV("NumLeafOperands", SF.NumLeafOps)
        << " is_affine=" << NV("IsAffine", SF.IsAffine)
        << " has_in_loop_unknown="
        << NV("HasInLoopUnknown", SF.HasInLoopUnknown)
        << " has_non_unit_stride_for_l_addrec="
        << NV("HasNonUnitStrideForLAddRec", SF.HasNonUnitStrideForLAddRec)
        << " has_non_constant_stride_for_l_addrec="
        << NV("HasNonConstantStrideForLAddRec",
              SF.HasNonConstantStrideForLAddRec)
        << " not_proven_monotonic="
        << NV("NotProvenMonotonic", SF.NotProvenMonotonic)
        << " has_negative_stride_for_l_addrec="
        << NV("HasNegativeStrideForLAddRec", SF.HasNegativeStrideForLAddRec)
        << " edge_btc_computable=" << NV("EdgeBTCComputable", BTC.Computable)
        << " edge_btc_symbolic=" << NV("EdgeBTCSymbolic", BTC.Symbolic)
        << " loop_has_other_unknown_btc_trap="
        << NV("LoopHasOtherUnknownBTCTrap", LoopHasOtherUnknownBTCTrap)
        << " predicate_shape="
        << NV("PredicateShape", trapPredicateShapeName(PredShape).str())
        << " is_entry_proximate=" << NV("IsEntryProximate", IsEntryProx);
    if (LTAEmitExplain) {
      Rem << " dominates_latch=" << NV("DominatesLatch", Dom.DominatesLatch)
          << " loop_latch_btc_computable="
          << NV("LoopLatchBTCComputable", Dom.LoopLatchBTCComputable)
          << " has_store_reload=" << NV("HasStoreReload", RO.HasStoreReload)
          << " has_mem_intrinsic_reload="
          << NV("HasMemIntrinsicReload", RO.HasMemIntrinsicReload)
          << " reload_store_line=" << NV("ReloadStoreLine", ReloadStoreLine)
          << " reload_store_kind=" << NV("ReloadStoreKind", ReloadStoreKind)
          << " reload_store_tbaa=" << NV("ReloadStoreTBAA", ReloadStoreTBAA)
          << " reload_load_tbaa=" << NV("ReloadLoadTBAA", ReloadLoadTBAA)
          << " reload_load_name=" << NV("ReloadLoadName", ReloadLoadName)
          << " reload_load_line=" << NV("ReloadLoadLine", ReloadLoadLine)
          << " has_call_reload=" << NV("HasCallReload", RO.HasCallReload)
          << " has_unaliased_load_operand="
          << NV("HasUnaliasedLoadOperand", RO.HasUnaliasedLoadOperand)
          << " has_in_loop_phi_operand="
          << NV("HasInLoopPhiOperand", RO.HasInLoopPhiOperand)
          << " has_in_loop_freeze_operand="
          << NV("HasInLoopFreezeOperand", RO.HasInLoopFreezeOperand)
          << " has_in_loop_select_operand="
          << NV("HasInLoopSelectOperand", RO.HasInLoopSelectOperand)
          << " has_other_in_loop_unknown_operand="
          << NV("HasOtherInLoopUnknownOperand", RO.HasOtherInLoopUnknownOperand)
          << " has_opaque_operand_no_in_loop_unknown="
          << NV("HasOpaqueOperandNoInLoopUnknown",
                RO.HasOpaqueOperandNoInLoopUnknown)
          << " has_outer_loop_addrec_operand="
          << NV("HasOuterLoopAddRecOperand", RO.HasOuterLoopAddRecOperand)
          << " invocation_seq=" << NV("InvocationSeq", InvocationSeq);
    }
    ORE.emit(Rem);
  }
  // Flag-gated per-load alias annotation: for each load in an INNERMOST loop
  // may-clobbered by an in-loop writer (store / memcpy / call), emit a
  // LoopLoadAlias record naming the first such writer. Clobbered loads only.
  if (LTAEmitLoadAlias) {
    std::string LName = "LoopLoadAlias";
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
            continue;
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

/// Check for an unreachable instruction that has an edge to any of \p L basic
/// blocks. if `--use-bounds-safety-traps-only` is used make sure that the trap and
/// branch instructions have -fbounds-safety annotation.
static bool hasUnreachableInst(Loop *L) {
  SmallVector<BasicBlock *, 4> LoopExitBlocks;
  L->getExitBlocks(LoopExitBlocks);
  for (auto *BB : LoopExitBlocks) {
    auto *I = BB->getTerminator();
    // check for trap instructions. If `BoundsSafetyTrapsOnly` is false then we
    // ignore if the trap has a -fbounds-safety annotation.
    if (!isa<UnreachableInst>(I))
      continue;
    if (BoundsSafetyTrapsOnly && !isBoundsSafetyAnnotated(I))
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

static std::string getSideEffectReasons(const Instruction &I) {
  std::string Buf;
  raw_string_ostream OS(Buf);
  if (isa<CallInst>(I) && I.mayReadOrWriteMemory())
    OS << "Instruction might have a volatile memory access";
  else {
    if (I.mayWriteToMemory())
      OS << "Instruction may write to memory\n";
    if (I.mayThrow())
      OS << "Instruction may throw an exception\n";
    if (!I.willReturn())
      OS << "Instruction may not return\n";
  }
  return Buf;
}

/// Check if \p L can be hoisted or not and emit a detailed remark about why
/// it can't be hoisted.
static CheckLoopHoistType processLoops(Loop *L, ScalarEvolution &SE,
                                       OptimizationRemarkEmitter &ORE) {
  CheckLoopHoistType HoistType;
  bool SymbolicMaxBackEdgeComputable =
      !isa<SCEVCouldNotCompute>(SE.getSymbolicMaxBackedgeTakenCount(L));
  bool HasSideEffects = false;
  if (!hasUnreachableInst(L))
    return CheckLoopHoistType::SKIP;

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
          else if (NewTrapSemantics)
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
  // Emit a remark for the loop
  auto ORA = OptimizationRemarkAnalysis(REMARK_PASS, "LoopTrap",
                                        &L->getHeader()->front());
  ORA << "Loop: " << L->getName() << " ";
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
                        OptimizationRemarkEmitter &ORE, ScalarEvolution &SE) {
  unsigned TotalCanHoistLoops = 0;
  unsigned TotalUnHoistableLoops = 0;
  for (auto *L : LI.getLoopsInPreorder()) {
    CheckLoopHoistType Type = processLoops(L, SE, ORE);
    if (Type == CheckLoopHoistType::MAYBE_CAN_HOIST)
      TotalCanHoistLoops++;
    else if (Type == CheckLoopHoistType::CANNOT_HOIST)
      TotalUnHoistableLoops++;
  }

  OptimizationRemarkAnalysis Rem(REMARK_PASS, "LoopTrapSummary", &F);
  Rem << "Trap checks results:\n";
  Rem << "Total count of loops with traps "
      << NV("TotalCount", TotalCanHoistLoops + TotalUnHoistableLoops) << "\n";
  Rem << "Loops that maybe can be hoisted: "
      << NV("CountHoist", TotalCanHoistLoops) << "\n";
  Rem << "Loops that cannot be hoisted: "
      << NV("CountCannotHoist", TotalUnHoistableLoops) << "\n";
  ORE.emit(Rem);
}

PreservedAnalyses LoopTrapAnalysisPass::run(Function &F,
                                            FunctionAnalysisManager &AM) {
  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
  auto &ORE = AM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  emitRemarks(F, LI, ORE, SE);
  if (LTAEmitExplain) {
    auto &AA = AM.getResult<AAManager>(F);
    auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
    unsigned Seq = ++InvocationCount[&F];
    emitLoopPrimitives(F, LI, ORE, SE, AA, Seq);
    emitPerTrapEdge(F, LI, ORE, SE, AA, DT, Seq);
  }
  return PreservedAnalyses::all();
}

void LoopTrapAnalysisPass::printPipeline(
    raw_ostream &OS, function_ref<StringRef(StringRef)> MapClassName2PassName) {
  OS << "loop-trap-analysis";
}
