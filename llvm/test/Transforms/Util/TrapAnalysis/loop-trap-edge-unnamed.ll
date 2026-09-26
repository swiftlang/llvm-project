; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

; Same counted-trap loop as loop-trap-edge-basic.ll but with no source-level
; block names (numeric IR, as produced by stripped names or a non-C frontend).
; bbLabel falls back to printAsOperand, which for any block in a function emits
; a `%N` slot, so src_bb / trap_bb / loop_header are the numeric labels and are
; never empty.


define void @counted_trap(ptr %0, i32 %1) {
  br label %3

3:
  %4 = phi i32 [ 0, %2 ], [ %9, %7 ]
  %5 = icmp ult i32 %4, %1
  br i1 %5, label %7, label %6

6:
  call void @llvm.trap()
  unreachable

7:
  %8 = getelementptr i32, ptr %0, i32 %4
  store i32 0, ptr %8, align 4
  %9 = add nuw nsw i32 %4, 1
  %10 = icmp eq i32 %9, %1
  br i1 %10, label %11, label %3

11:
  ret void
}

attributes #0 = { cold noreturn nounwind memory(inaccessiblemem: write) }

; The complete LoopTrapEdge record; the block labels are numeric slots.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}counted_trap
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}counted_trap
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}'%3'
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}'%6'
; CHECK-NEXT:   - String:{{ +}}' loop_depth='
; CHECK-NEXT:   - LoopDepth:{{ +}}'1'
; CHECK-NEXT:   - String:{{ +}}' loop_header='
; CHECK-NEXT:   - LoopHeader:{{ +}}'%3'
; CHECK-NEXT:   - String:{{ +}}' is_innermost='
; CHECK-NEXT:   - IsInnermost:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' is_loop_exit='
; CHECK-NEXT:   - IsLoopExit:{{ +}}'true'
; CHECK-NEXT: ...
