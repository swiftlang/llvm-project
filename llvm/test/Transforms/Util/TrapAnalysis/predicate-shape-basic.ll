; Per-edge LoopTrapEdge remarks carry a PredicateShape classification of the
; trap branch's i1 condition (plus a NumLeafOperands count). These fields are
; gated by -loop-trap-analysis-explain. Each function below shapes its trap
; predicate differently; the CHECKs assert the classified shape.

; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

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

; Full LoopTrapEdge record(s), pinned line-by-line.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}single_icmp
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}single_icmp
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}body
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}trap
; CHECK-NEXT:   - String:{{ +}}' loop_depth='
; CHECK-NEXT:   - LoopDepth:{{ +}}'1'
; CHECK-NEXT:   - String:{{ +}}' loop_header='
; CHECK-NEXT:   - LoopHeader:{{ +}}body
; CHECK-NEXT:   - String:{{ +}}' is_innermost='
; CHECK-NEXT:   - IsInnermost:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' is_loop_exit='
; CHECK-NEXT:   - IsLoopExit:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' num_leaf_operands='
; CHECK-NEXT:   - NumLeafOperands:{{ +}}'2'
; CHECK-NEXT:   - String:{{ +}}' is_affine='
; CHECK-NEXT:   - IsAffine:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_unknown='
; CHECK-NEXT:   - HasInLoopUnknown:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_non_unit_stride_for_l_addrec='
; CHECK-NEXT:   - HasNonUnitStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_non_constant_stride_for_l_addrec='
; CHECK-NEXT:   - HasNonConstantStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' not_proven_monotonic='
; CHECK-NEXT:   - NotProvenMonotonic:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_negative_stride_for_l_addrec='
; CHECK-NEXT:   - HasNegativeStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_computable='
; CHECK-NEXT:   - EdgeBTCComputable:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_symbolic='
; CHECK-NEXT:   - EdgeBTCSymbolic:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' predicate_shape='
; CHECK-NEXT:   - PredicateShape:{{ +}}SingleICmp
; CHECK-NEXT:   - String:{{ +}}' is_entry_proximate='
; CHECK-NEXT:   - IsEntryProximate:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' dominates_latch='
; CHECK-NEXT:   - DominatesLatch:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' loop_latch_btc_computable='
; CHECK-NEXT:   - LoopLatchBTCComputable:{{ +}}'true'
; CHECK-NEXT: ...
;
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}or_boundscheck_const
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}or_boundscheck_const
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}entry
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}trap
; CHECK-NEXT:   - String:{{ +}}' loop_depth='
; CHECK-NEXT:   - LoopDepth:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' loop_header='
; CHECK-NEXT:   - LoopHeader:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' is_innermost='
; CHECK-NEXT:   - IsInnermost:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' is_loop_exit='
; CHECK-NEXT:   - IsLoopExit:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' num_leaf_operands='
; CHECK-NEXT:   - NumLeafOperands:{{ +}}'4'
; CHECK-NEXT:   - String:{{ +}}' is_affine='
; CHECK-NEXT:   - IsAffine:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_unknown='
; CHECK-NEXT:   - HasInLoopUnknown:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_non_unit_stride_for_l_addrec='
; CHECK-NEXT:   - HasNonUnitStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_non_constant_stride_for_l_addrec='
; CHECK-NEXT:   - HasNonConstantStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' not_proven_monotonic='
; CHECK-NEXT:   - NotProvenMonotonic:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_negative_stride_for_l_addrec='
; CHECK-NEXT:   - HasNegativeStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_computable='
; CHECK-NEXT:   - EdgeBTCComputable:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_symbolic='
; CHECK-NEXT:   - EdgeBTCSymbolic:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' predicate_shape='
; CHECK-NEXT:   - PredicateShape:{{ +}}OrBoundsCheck-ConstBound
; CHECK-NEXT:   - String:{{ +}}' is_entry_proximate='
; CHECK-NEXT:   - IsEntryProximate:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' dominates_latch='
; CHECK-NEXT:   - DominatesLatch:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' loop_latch_btc_computable='
; CHECK-NEXT:   - LoopLatchBTCComputable:{{ +}}'false'
; CHECK-NEXT: ...
;
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}or_two_cmp
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}or_two_cmp
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}entry
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}trap
; CHECK-NEXT:   - String:{{ +}}' loop_depth='
; CHECK-NEXT:   - LoopDepth:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' loop_header='
; CHECK-NEXT:   - LoopHeader:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' is_innermost='
; CHECK-NEXT:   - IsInnermost:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' is_loop_exit='
; CHECK-NEXT:   - IsLoopExit:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' num_leaf_operands='
; CHECK-NEXT:   - NumLeafOperands:{{ +}}'4'
; CHECK-NEXT:   - String:{{ +}}' is_affine='
; CHECK-NEXT:   - IsAffine:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_unknown='
; CHECK-NEXT:   - HasInLoopUnknown:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_non_unit_stride_for_l_addrec='
; CHECK-NEXT:   - HasNonUnitStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_non_constant_stride_for_l_addrec='
; CHECK-NEXT:   - HasNonConstantStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' not_proven_monotonic='
; CHECK-NEXT:   - NotProvenMonotonic:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_negative_stride_for_l_addrec='
; CHECK-NEXT:   - HasNegativeStrideForLAddRec:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_computable='
; CHECK-NEXT:   - EdgeBTCComputable:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_symbolic='
; CHECK-NEXT:   - EdgeBTCSymbolic:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' predicate_shape='
; CHECK-NEXT:   - PredicateShape:{{ +}}OtherMulti
; CHECK-NEXT:   - String:{{ +}}' is_entry_proximate='
; CHECK-NEXT:   - IsEntryProximate:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' dominates_latch='
; CHECK-NEXT:   - DominatesLatch:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' loop_latch_btc_computable='
; CHECK-NEXT:   - LoopLatchBTCComputable:{{ +}}'false'
; CHECK-NEXT: ...
