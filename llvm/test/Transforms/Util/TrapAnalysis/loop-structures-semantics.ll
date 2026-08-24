; RUN: opt -passes='loop-trap-analysis' --loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

declare void @llvm.trap()

; Single innermost loop with one trap exit (depth 1).
define void @single_loop(ptr %base, i32 %n) {
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
  store i32 0, ptr %p
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}single_loop
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Nested loops with the trap exit only in the INNER loop: exactly one edge,
; attributed at depth 2 (the outer loop is not double-counted).
define void @nested_inner_trap(i32 %n, i32 %m) {
entry:
  br label %outer
outer:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  br label %inner
inner:
  %j = phi i32 [ 0, %outer ], [ %j.next, %inner.latch ]
  %cmp = icmp ult i32 %j, %m
  br i1 %cmp, label %inner.latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
inner.latch:
  %j.next = add i32 %j, 1
  %ej = icmp eq i32 %j.next, %m
  br i1 %ej, label %outer.latch, label %inner
outer.latch:
  %i.next = add i32 %i, 1
  %ei = icmp eq i32 %i.next, %n
  br i1 %ei, label %exit, label %outer
exit:
  ret void
}
; CHECK: Function:{{ +}}nested_inner_trap
; CHECK: LoopDepth:{{ +}}'2'
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Loop with multiple exits: one trap exit plus one normal (non-trap) exit.
define void @multi_exit(ptr %p, i32 %n) {
entry:
  br label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult i32 %iv, %n
  br i1 %cmp, label %chk, label %trap
trap:
  call void @llvm.trap()
  unreachable
chk:
  %v = load i32, ptr %p
  %z = icmp eq i32 %v, 0
  br i1 %z, label %exit, label %latch
latch:
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}multi_exit
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Nested loops with the trap exit in the OUTER loop: attributed at depth 1.
define void @nested_outer_trap(i32 %n, i32 %m) {
entry:
  br label %outer
outer:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  %co = icmp ult i32 %i, %n
  br i1 %co, label %inner, label %trap
trap:
  call void @llvm.trap()
  unreachable
inner:
  %j = phi i32 [ 0, %outer ], [ %j.next, %inner ]
  %j.next = add i32 %j, 1
  %ej = icmp eq i32 %j.next, %m
  br i1 %ej, label %outer.latch, label %inner
outer.latch:
  %i.next = add i32 %i, 1
  %ei = icmp eq i32 %i.next, %n
  br i1 %ei, label %exit, label %outer
exit:
  ret void
}
; CHECK: Function:{{ +}}nested_outer_trap
; CHECK: LoopDepth:{{ +}}'1'
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Rotated / guarded loop: a preheader guard dominates the loop body.
define void @guarded_loop(ptr %base, i32 %n) {
entry:
  %g = icmp sgt i32 %n, 0
  br i1 %g, label %preheader, label %exit
preheader:
  br label %body
body:
  %iv = phi i32 [ 0, %preheader ], [ %iv.next, %latch ]
  %cmp = icmp ult i32 %iv, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  %p = getelementptr i32, ptr %base, i32 %iv
  store i32 0, ptr %p
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}guarded_loop
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown
