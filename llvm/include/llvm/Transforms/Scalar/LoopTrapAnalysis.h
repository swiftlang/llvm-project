//===- LoopTrapAnalysis.h ---------------------------*- C++ -*-------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_LoopTrapAnalysis_H
#define LLVM_TRANSFORMS_SCALAR_LoopTrapAnalysis_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

struct LoopTrapAnalysisPass
    : public OptionalPassInfoMixin<LoopTrapAnalysisPass> {
  // Per-Function run() counter, emitted as InvocationSeq (gated by
  // -loop-trap-analysis-explain) so consumers can dedup repeated invocations
  // by max(seq) per (function, src_bb, trap_bb). mutable: bookkeeping side
  // channel, not analysis state; single-threaded per FunctionAnalysisManager.
  mutable DenseMap<const Function *, unsigned> InvocationCount;

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &);

  void printPipeline(raw_ostream &OS,
                     function_ref<StringRef(StringRef)> MapClassName2PassName);
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_LoopTrapAnalysis_H
