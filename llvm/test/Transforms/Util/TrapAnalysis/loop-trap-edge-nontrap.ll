; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s \
; RUN:   --implicit-check-not=LoopTrapEdge --implicit-check-not=fakecall \
; RUN:   --implicit-check-not=bare

declare void @sideeffect()

; The loop guards a real trap block (%realtrap: @llvm.trap + unreachable) plus
; two blocks that also end in `unreachable` but are NOT trap blocks:
;   %fakecall - unreachable preceded by a normal, returning call, and
;   %bare     - a lone `unreachable` with no preceding instruction.
; isTrapEdgeBlock accepts only %realtrap, so exactly one LoopTrapEdge is
; emitted and the two non-trap edges contribute nothing (enforced by the
; --implicit-check-not directives above).
define void @mixed(ptr %base, i32 %n) {
entry:
  br label %body

body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %c0 = icmp ult i32 %iv, %n
  br i1 %c0, label %s1, label %realtrap

realtrap:
  call void @llvm.trap()
  unreachable

s1:
  %c1 = icmp eq i32 %iv, 3
  br i1 %c1, label %s2, label %fakecall

fakecall:
  call void @sideeffect()
  unreachable

s2:
  %c2 = icmp eq i32 %iv, 5
  br i1 %c2, label %latch, label %bare

bare:
  unreachable

latch:
  %p = getelementptr i32, ptr %base, i32 %iv
  store i32 0, ptr %p, align 4
  %iv.next = add nuw nsw i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body

exit:
  ret void
}

; The single LoopTrapEdge record, pinned line-by-line; it targets %realtrap.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}mixed
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}mixed
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}body
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}realtrap
; CHECK-NEXT:   - String:{{ +}}' loop_depth='
; CHECK-NEXT:   - LoopDepth:{{ +}}'1'
; CHECK-NEXT:   - String:{{ +}}' loop_header='
; CHECK-NEXT:   - LoopHeader:{{ +}}body
; CHECK-NEXT:   - String:{{ +}}' is_innermost='
; CHECK-NEXT:   - IsInnermost:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' is_loop_exit='
; CHECK-NEXT:   - IsLoopExit:{{ +}}'true'
; CHECK-NEXT: ...
