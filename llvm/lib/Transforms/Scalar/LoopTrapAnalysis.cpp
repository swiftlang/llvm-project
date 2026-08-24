//===-- LoopTrapAnalysis.cpp - Loop Trap Count pass -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/LoopTrapAnalysis.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Instructions.h"
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
    cl::desc("Emit the explanatory trap analysis: the refined TrapClass "
             "classification plus per-loop and per-edge explanatory fields "
             "(DominatesLatch / IV-update / operand-class). When false "
             "(default), only the pre-explanation fields and compact TrapClass "
             "are emitted, so the framework can be A/B compared and reverted "
             "by toggling this flag alone."));

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

/// Minimal trap-block predicate for the per-edge explain output: \p BB ends in
/// `unreachable` immediately preceded by a trap-like terminating call,
/// identified by its semantic property rather than an intrinsic allowlist: a
/// `noreturn` call touching only inaccessible memory (the shared property of
/// @llvm.trap / @llvm.ubsantrap and any future trap intrinsic).
static bool isTrapEdgeBlock(BasicBlock *BB) {
  if (!BB || BB->empty())
    return false;
  Instruction *Term = BB->getTerminator();
  if (!isa<UnreachableInst>(Term))
    return false;
  if (Term == &BB->front())
    return false;
  if (auto *CI = dyn_cast<CallInst>(Term->getPrevNode()))
    return CI->doesNotReturn() && CI->onlyAccessesInaccessibleMemory();
  return false;
}

/// Count loop-exit edges of \p L whose successor is a trap block (see
/// isTrapEdgeBlock).
///
/// Strict attribution: count a trap exit only if it has a predecessor whose
/// immediate containing loop is L (not a nested sub-loop). Otherwise an inner
/// trap's unreachable target -- present in every enclosing loop's exit set --
/// would be counted at every nesting level.
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

/// Emit one machine-readable LoopPrimitives remark per loop in F (all nesting
/// depths), plus a per-function LoopPrimitivesSummary with loop-depth tallies.
/// Always emits (does not gate on hasUnreachableInst) so every loop is
/// captured, including trap-free ones. Gated by -loop-trap-analysis-explain.
static void emitLoopPrimitives(Function &F, LoopInfo &LI,
                               OptimizationRemarkEmitter &ORE,
                               ScalarEvolution &SE, StringRef Tag) {
  unsigned TotalLoops = 0;
  unsigned Innermost = 0;
  unsigned MaxDepth = 0;
  unsigned Depth1 = 0, Depth2 = 0, Depth3Plus = 0;

  std::string PrimName = Tag.empty() ? std::string("LoopPrimitives")
                                     : ("LoopPrimitives" + Tag).str();
  std::string SumName = Tag.empty() ? std::string("LoopPrimitivesSummary")
                                    : ("LoopPrimitivesSummary" + Tag).str();

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
    bool BTCKnown = !isa<SCEVCouldNotCompute>(SE.getBackedgeTakenCount(L));

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

/// Emit one LoopTrapEdge remark per conditional branch whose one successor is a
/// trap block (see isTrapEdgeBlock). Gated by -loop-trap-analysis-explain.
static void emitPerTrapEdge(Function &F, LoopInfo &LI,
                            OptimizationRemarkEmitter &ORE, StringRef Tag) {
  std::string Name =
      Tag.empty() ? std::string("LoopTrapEdge") : ("LoopTrapEdge" + Tag).str();
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
    bool IsLoopExit = Innermost && !Innermost->contains(TrapSucc);

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
        << " is_loop_exit=" << NV("IsLoopExit", IsLoopExit);
    ORE.emit(Rem);
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
    emitLoopPrimitives(F, LI, ORE, SE, Tag);
    emitPerTrapEdge(F, LI, ORE, Tag);
  }
  return PreservedAnalyses::all();
}

void LoopTrapAnalysisPass::printPipeline(
    raw_ostream &OS, function_ref<StringRef(StringRef)> MapClassName2PassName) {
  OS << "loop-trap-analysis";
  if (!Tag.empty())
    OS << "<tag=" << Tag << ">";
}