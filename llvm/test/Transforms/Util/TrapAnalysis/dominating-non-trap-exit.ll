; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

; Two trap checks around a non-trap early exit (%early branches out to %ret). The
; trap that DOMINATES the early exit (%trap1, from %body) stays hoistable, so
; has_dominating_non_trap_exit=false. The trap DOMINATED BY the early exit
; (%trap2, from %after) cannot be hoisted -- the loop may leave via %early first
; -- so has_dominating_non_trap_exit=true.
define void @two_traps(ptr %base, i32 %n, i32 %lim) {
entry:
  br label %body

body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %c1 = icmp uge i32 %iv, 100
  br i1 %c1, label %trap1, label %early

trap1:
  call void @llvm.trap()
  unreachable

early:
  %done = icmp eq i32 %iv, %lim
  br i1 %done, label %ret, label %after

after:
  %c2 = icmp uge i32 %iv, 200
  br i1 %c2, label %trap2, label %latch

trap2:
  call void @llvm.trap()
  unreachable

latch:
  %p = getelementptr i32, ptr %base, i32 %iv
  store i32 0, ptr %p, align 4
  %iv.next = add nuw nsw i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %ret, label %body

ret:
  ret void
}

; Trap from %body dominates the early exit -> hoistable.
; CHECK:      - SourceBB:{{ +}}body
; CHECK:      - TrapBB:{{ +}}trap1
; CHECK:      - String:{{ +}}' has_dominating_non_trap_exit='
; CHECK-NEXT: - HasDominatingNonTrapExit:{{ +}}'false'
; Trap from %after is dominated by the early exit -> not hoistable.
; CHECK:      - SourceBB:{{ +}}after
; CHECK:      - TrapBB:{{ +}}trap2
; CHECK:      - String:{{ +}}' has_dominating_non_trap_exit='
; CHECK-NEXT: - HasDominatingNonTrapExit:{{ +}}'true'
