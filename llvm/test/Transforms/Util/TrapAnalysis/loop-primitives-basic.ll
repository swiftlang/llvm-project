; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

declare void @llvm.trap()

; A nested loop nest. The outer loop (%outer) and the inner loop (%inner) each
; guard a conditional branch to an @llvm.trap+unreachable block, giving each
; loop one trap exit. The per-loop LoopPrimitives records should report the
; inner loop as innermost with depth 2 and the outer loop at depth 1.
define void @nested_trap(ptr %base, i32 %n, i32 %m) {
entry:
  br label %outer

outer:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  %ocmp = icmp ult i32 %i, %n
  br i1 %ocmp, label %inner.preheader, label %outer.trap

outer.trap:
  call void @llvm.trap()
  unreachable

inner.preheader:
  br label %inner

inner:
  %j = phi i32 [ 0, %inner.preheader ], [ %j.next, %inner.latch ]
  %icmp = icmp ult i32 %j, %m
  br i1 %icmp, label %inner.latch, label %inner.trap

inner.trap:
  call void @llvm.trap()
  unreachable

inner.latch:
  %j.next = add nuw nsw i32 %j, 1
  %je = icmp eq i32 %j.next, %m
  br i1 %je, label %outer.latch, label %inner

outer.latch:
  %i.next = add nuw nsw i32 %i, 1
  %ie = icmp eq i32 %i.next, %n
  br i1 %ie, label %exit, label %outer

exit:
  ret void
}

; One LoopPrimitives record per loop (all depths). Outer loop first
; (preorder): depth 1, not innermost, one trap exit.
; CHECK:      Name:{{ +}}LoopPrimitives
; CHECK-NEXT: Function:{{ +}}nested_trap
; CHECK:        - LoopHeader:{{ +}}outer
; CHECK:        - Depth:{{ +}}'1'
; CHECK:        - ParentHeader:{{ +}}'-'
; CHECK:        - IsInnermost:{{ +}}'false'
; CHECK:        - TrapExitCount:{{ +}}'1'
; CHECK:        - BTCKnown:{{ +}}'false'

; Inner loop: depth 2, innermost, one trap exit, computable backedge count.
; CHECK:      Name:{{ +}}LoopPrimitives
; CHECK-NEXT: Function:{{ +}}nested_trap
; CHECK:        - LoopHeader:{{ +}}inner
; CHECK:        - Depth:{{ +}}'2'
; CHECK:        - ParentHeader:{{ +}}outer
; CHECK:        - IsInnermost:{{ +}}'true'
; CHECK:        - TrapExitCount:{{ +}}'1'
; CHECK:        - BTCKnown:{{ +}}'true'

; Per-function summary with loop-depth tallies.
; CHECK:      Name:{{ +}}LoopPrimitivesSummary
; CHECK:        - TotalLoops:{{ +}}'2'
; CHECK:        - Innermost:{{ +}}'1'
; CHECK:        - MaxDepth:{{ +}}'2'
; CHECK:        - Depth1:{{ +}}'1'
; CHECK:        - Depth2:{{ +}}'1'
