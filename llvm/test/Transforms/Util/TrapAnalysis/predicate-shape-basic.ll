; Per-edge LoopTrapEdge remarks carry a PredicateShape classification of the
; trap branch's i1 condition (plus a NumLeafOperands count). These fields are
; gated by -loop-trap-analysis-explain. Each function below shapes its trap
; predicate differently; the CHECKs assert the classified shape.

; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

declare void @llvm.trap()

; A single icmp guard -> SingleICmp (NumLeafOperands 2).
define void @single_icmp(ptr %base, i32 %n) {
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

; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK:      Function:{{ +}}single_icmp
; CHECK:        - SourceBB:{{ +}}body
; CHECK:        - NumLeafOperands:{{ +}}'2'
; CHECK:        - PredicateShape:{{ +}}SingleICmp

; A bounds-check OR with the sub-arithmetic second arm -> OrBoundsCheck-ConstBound
;   or(uge(x, n), ult(sub(n, x), 4))
define void @or_boundscheck_const(ptr %base, i32 %x, i32 %n) {
entry:
  %c1 = icmp uge i32 %x, %n
  %sub = sub i32 %n, %x
  %c2 = icmp ult i32 %sub, 4
  %or = or i1 %c1, %c2
  br i1 %or, label %trap, label %ok
trap:
  call void @llvm.trap()
  unreachable
ok:
  ret void
}

; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK:      Function:{{ +}}or_boundscheck_const
; CHECK:        - SourceBB:{{ +}}entry
; CHECK:        - NumLeafOperands:{{ +}}'4'
; CHECK:        - PredicateShape:{{ +}}OrBoundsCheck-ConstBound

; A plain OR of two unrelated comparisons (no sub-arithmetic) -> OtherMulti.
define void @or_two_cmp(ptr %base, i32 %x, i32 %n) {
entry:
  %c1 = icmp uge i32 %x, %n
  %c2 = icmp ult i32 %x, 4
  %or = or i1 %c1, %c2
  br i1 %or, label %trap, label %ok
trap:
  call void @llvm.trap()
  unreachable
ok:
  ret void
}

; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK:      Function:{{ +}}or_two_cmp
; CHECK:        - SourceBB:{{ +}}entry
; CHECK:        - NumLeafOperands:{{ +}}'4'
; CHECK:        - PredicateShape:{{ +}}OtherMulti
