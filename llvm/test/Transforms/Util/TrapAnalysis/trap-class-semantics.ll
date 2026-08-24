; RUN: opt -passes='loop-trap-analysis' --loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

declare void @llvm.trap()
declare void @llvm.ubsantrap(i8 immarg)
declare void @llvm.looptrap()
declare i32 @opaque()

; Counted loop whose trap exit calls @llvm.ubsantrap.
define void @counted_ubsantrap(ptr %base, i32 %n) {
entry:
  %c0 = icmp eq i32 %n, 0
  br i1 %c0, label %exit, label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult i32 %iv, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.ubsantrap(i8 25)
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
; CHECK: Function:{{ +}}counted_ubsantrap
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Counted loop whose trap exit calls @llvm.looptrap.
define void @counted_looptrap(ptr %base, i32 %n) {
entry:
  %c0 = icmp eq i32 %n, 0
  br i1 %c0, label %exit, label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult i32 %iv, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.looptrap()
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
; CHECK: Function:{{ +}}counted_looptrap
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Counted loop whose trap exit calls @llvm.trap.
define void @counted_trap(ptr %base, i32 %n) {
entry:
  %c0 = icmp eq i32 %n, 0
  br i1 %c0, label %exit, label %body
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
; CHECK: Function:{{ +}}counted_trap
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountKnown

; Trap exit driven by a pointer IV with a non-unit constant stride (i8, +2),
; so the trip count is not computable.
define void @stride_nonunit(ptr %base, ptr %bound) {
entry:
  br label %body
body:
  %iv = phi ptr [ %base, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult ptr %iv, %bound
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  %iv.next = getelementptr i8, ptr %iv, i32 2
  %l = load i8, ptr %iv
  %e = icmp eq i8 %l, 0
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}stride_nonunit
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-NonUnitStride

; Trap exit driven by a pointer IV whose stride is a runtime (non-constant)
; value %s.
define void @stride_variable(ptr %base, ptr %bound, i32 %s) {
entry:
  br label %body
body:
  %iv = phi ptr [ %base, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult ptr %iv, %bound
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  %iv.next = getelementptr i32, ptr %iv, i32 %s
  %l = load i32, ptr %iv
  %e = icmp eq i32 %l, 0
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}stride_variable
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic-NonConstantStride

; Trap check outside any loop, sitting directly on the function entry block.
define void @entry_proximate(i32 %n) {
entry:
  %c = icmp slt i32 %n, 0
  br i1 %c, label %trap, label %cont
trap:
  call void @llvm.trap()
  unreachable
cont:
  br label %body
body:
  %iv = phi i32 [ 0, %cont ], [ %iv.next, %body ]
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}entry_proximate
; CHECK: TrapClass:{{ +}}Invariant-OutsideLoop-EntryProximate

; Trap check outside any loop, reached only after a long straight-line chain
; of blocks from entry (not adjacent to entry).
define void @out_single_cmp(i32 %n, i32 %x) {
entry:
  br label %d1
d1:
  %a = add i32 %x, 1
  br label %d2
d2:
  %b = add i32 %a, 1
  br label %d3
d3:
  %cc = add i32 %b, 1
  br label %d4
d4:
  %dd = add i32 %cc, 1
  br label %chk
chk:
  %c = icmp slt i32 %dd, 0
  br i1 %c, label %trap, label %cont
trap:
  call void @llvm.trap()
  unreachable
cont:
  br label %body
body:
  %iv = phi i32 [ 0, %cont ], [ %iv.next, %body ]
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}out_single_cmp
; CHECK: TrapClass:{{ +}}Invariant-OutsideLoop-SingleComparison

; In-loop trap exit whose condition reloads %p, which a same-loop store to %q
; could alias and modify.
define void @store_reload(ptr %p, ptr %q) {
entry:
  br label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %v = load i32, ptr %p
  %cmp = icmp ult i32 %iv, %v
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  store i32 %iv, ptr %q
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, 100
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}store_reload
; CHECK: TrapClass:{{ +}}Opaque-InLoopExit-TripCountUnknown-StoreReload

; In-loop trap exit whose condition depends on %acc, a non-IV phi updated only
; on some iterations (via a select).
define void @in_loop_phi_operand(i32 %n, ptr %p) {
entry:
  br label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  %cmp = icmp slt i32 %acc, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  %ld = load i32, ptr %p
  %pos = icmp sgt i32 %ld, 0
  %acc.inc = add i32 %acc, 1
  %acc.next = select i1 %pos, i32 %acc.inc, i32 %acc
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}in_loop_phi_operand
; CHECK: TrapClass:{{ +}}Opaque-InLoopExit-TripCountUnknown-InLoopPhiOperand

; In-loop trap exit whose condition operand is the result of @opaque(), which
; has no characterizable SCEV.
define void @opaque_other(i32 %n) {
entry:
  br label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %o = call i32 @opaque()
  %cmp = icmp slt i32 %o, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, 100
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}opaque_other
; CHECK: TrapClass:{{ +}}Opaque-InLoopExit-TripCountUnknown-OpaqueOperand-Other

; In-loop trap exit on an accumulator (%acc += %iv) whose addrec is not proven
; monotonic (carries only a weak no-wrap flag).
define void @not_proven_monotonic(i32 %n) {
entry:
  br label %body
body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  %cmp = icmp slt i32 %acc, %n
  br i1 %cmp, label %latch, label %trap
trap:
  call void @llvm.trap()
  unreachable
latch:
  %acc.next = add i32 %acc, %iv
  %iv.next = add i32 %iv, 1
  %e = icmp eq i32 %iv.next, 100
  br i1 %e, label %exit, label %body
exit:
  ret void
}
; CHECK: Function:{{ +}}not_proven_monotonic
; CHECK: TrapClass:{{ +}}Affine-InLoopExit-TripCountUnknown-NotProvenMonotonic
