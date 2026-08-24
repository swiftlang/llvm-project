; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

declare void @llvm.trap()

; A counted loop whose in-loop check branches to an @llvm.trap+unreachable
; block. The per-edge explain output should emit one LoopTrapEdge record for
; the cond-br in %body whose trap successor is %trap.
define void @counted_trap(ptr %base, i32 %n) {
entry:
  br label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult i32 %iv, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
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

; CHECK:      --- !Analysis
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}counted_trap
; CHECK:        - Function:{{ +}}counted_trap
; CHECK:        - SourceBB:{{ +}}body
; CHECK:        - TrapBB:{{ +}}trap
; CHECK:        - LoopDepth:{{ +}}'1'
; CHECK:        - LoopHeader:{{ +}}body
; CHECK:        - IsInnermost:{{ +}}'true'
; CHECK:        - IsLoopExit:{{ +}}'true'
