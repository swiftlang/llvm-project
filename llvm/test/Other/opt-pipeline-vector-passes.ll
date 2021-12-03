; RUN: opt -disable-verify -debug-pass-manager -passes='default<O1>' -S %s 2>&1 | FileCheck %s --check-prefixes=NEWPM_O1
; RUN: opt -disable-verify -debug-pass-manager -passes='default<O2>' -S %s 2>&1 | FileCheck %s --check-prefixes=NEWPM_O2
; RUN: opt -disable-verify -debug-pass-manager -passes='default<O2>' -extra-vectorizer-passes -S %s 2>&1 | FileCheck %s --check-prefixes=NEWPM_O2_EXTRA

; REQUIRES: asserts

; SLP does not run at -O1. Loop vectorization runs, but it only
; works on loops explicitly annotated with pragmas.

; OLDPM_O1-LABEL:  Pass Arguments:
; OLDPM_O1:        Loop Vectorization
; OLDPM_O1-NOT:    SLP Vectorizer
; OLDPM_O1:        Optimize scalar/vector ops

; Everything runs at -O2.
; O2-LABEL:  Running pass: LoopVectorizePass
; O2-NOT:    Running pass: EarlyCSEPass
; O2-NOT:    Running pass: LICMPass
; O2:        Running pass: SLPVectorizerPass
; O2:        Running pass: VectorCombinePass

; There should be no difference with the new pass manager.
; This is tested more thoroughly in other test files.

; NEWPM_O1-LABEL:  Running pass: LoopVectorizePass
; NEWPM_O1-NOT:    Running pass: SLPVectorizerPass
; NEWPM_O1:        Running pass: VectorCombinePass

; NEWPM_O2-LABEL:  Running pass: LoopVectorizePass
; NEWPM_O2:        Running pass: SLPVectorizerPass
; NEWPM_O2:        Running pass: VectorCombinePass

; NEWPM_O2_EXTRA-LABEL: Running pass: LoopVectorizePass
; NEWPM_O2_EXTRA: Running pass: EarlyCSEPass
; NEWPM_O2_EXTRA: Running pass: CorrelatedValuePropagationPass
; NEWPM_O2_EXTRA: Running pass: InstCombinePass
; NEWPM_O2_EXTRA: Running pass: LICMPass
; NEWPM_O2_EXTRA: Running pass: SimpleLoopUnswitchPass
; NEWPM_O2_EXTRA: Running pass: SimplifyCFGPass
; NEWPM_O2_EXTRA: Running pass: InstCombinePass
; NEWPM_O2_EXTRA: Running pass: SLPVectorizerPass
; NEWPM_O2_EXTRA: Running pass: EarlyCSEPass
; NEWPM_O2_EXTRA: Running pass: VectorCombinePass

define i64 @f(i1 %cond, i32* %src, i32* %dst) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.i = getelementptr i32, i32* %src, i64 %i
  %src.v = load i32, i32* %src.i
  %add = add i32 %src.v, 10
  %dst.i = getelementptr i32, i32* %dst, i64 %i
  store i32 %add, i32* %dst.i
  %inc = add nuw nsw i64 %i, 1
  %ec = icmp ne i64 %inc, 1000
  br i1 %ec, label %loop, label %exit

exit:
  ret i64 %i
}
