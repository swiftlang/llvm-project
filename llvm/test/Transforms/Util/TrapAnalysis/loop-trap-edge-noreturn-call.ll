; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s \
; RUN:   --implicit-check-not=LoopTrapEdge --implicit-check-not=fatalbb

declare void @fatal() noreturn

; Only a trap intrinsic (noreturn, accesses only inaccessible memory) is a trap
; edge. A non-intrinsic `noreturn` call before `unreachable` (%fatalbb, e.g. a
; fatal-error function or longjmp) is not, so no LoopTrapEdge targets it; the
; intrinsic block (%trapbb) is the only edge.
define void @noreturn_call(ptr %base, i32 %n) {
entry:
  br label %body

body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %c0 = icmp ult i32 %iv, %n
  br i1 %c0, label %s1, label %fatalbb

fatalbb:
  call void @fatal()
  unreachable

s1:
  %c1 = icmp eq i32 %iv, 3
  br i1 %c1, label %latch, label %trapbb

trapbb:
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

; The single LoopTrapEdge record targets %trapbb (the intrinsic), not %fatalbb.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}noreturn_call
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}noreturn_call
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}s1
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}trapbb
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
