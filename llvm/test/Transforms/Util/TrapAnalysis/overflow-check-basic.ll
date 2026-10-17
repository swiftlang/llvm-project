; A trap edge whose guard is the overflow bit of a checked-arithmetic intrinsic
; (extractvalue(llvm.{s,u}{add,sub,mul}.with.overflow, 1)) is classified as
; OverflowCheck rather than an index/bounds comparison; a plain icmp-guarded edge
; on the same loop shape keeps its normal class. Gated by
; -loop-trap-analysis-explain.

; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s


; The in-loop trap exit is guarded by the overflow bit of a checked add ->
; OverflowCheck.
define void @overflow_inc(i64 %n) {
entry:
  br label %body

body:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %latch ]
  %ov = call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %iv, i64 1)
  %bit = extractvalue { i64, i1 } %ov, 1
  br i1 %bit, label %trap, label %latch

trap:
  call void @llvm.trap()
  unreachable

latch:
  %iv.next = extractvalue { i64, i1 } %ov, 0
  %done = icmp eq i64 %iv.next, %n
  br i1 %done, label %exit, label %body

exit:
  ret void
}

; The same loop shape guarded by a plain bounds icmp is not reclassified.
define void @bounds_check(ptr %base, i64 %n) {
entry:
  br label %body

body:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult i64 %iv, %n
  br i1 %cmp, label %latch, label %trap

trap:
  call void @llvm.trap()
  unreachable

latch:
  %iv.next = add nuw nsw i64 %iv, 1
  %e = icmp eq i64 %iv.next, %n
  br i1 %e, label %exit, label %body

exit:
  ret void
}

; Full LoopTrapEdge record(s), pinned line-by-line.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}overflow_inc
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}overflow_inc
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
; CHECK-NEXT:   - NumLeafOperands:{{ +}}'1'
; CHECK-NEXT:   - String:{{ +}}' is_affine='
; CHECK-NEXT:   - IsAffine:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_unknown='
; CHECK-NEXT:   - HasInLoopUnknown:{{ +}}'true'
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
; CHECK-NEXT:   - String:{{ +}}' loop_has_other_unknown_btc_trap='
; CHECK-NEXT:   - LoopHasOtherUnknownBTCTrap:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' predicate_shape='
; CHECK-NEXT:   - PredicateShape:{{ +}}OtherMulti
; CHECK-NEXT:   - String:{{ +}}' is_entry_proximate='
; CHECK-NEXT:   - IsEntryProximate:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' dominates_latch='
; CHECK-NEXT:   - DominatesLatch:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' loop_latch_btc_computable='
; CHECK-NEXT:   - LoopLatchBTCComputable:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' has_store_reload='
; CHECK-NEXT:   - HasStoreReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_mem_intrinsic_reload='
; CHECK-NEXT:   - HasMemIntrinsicReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' reload_store_line='
; CHECK-NEXT:   - ReloadStoreLine:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' reload_store_kind='
; CHECK-NEXT:   - ReloadStoreKind:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_store_tbaa='
; CHECK-NEXT:   - ReloadStoreTBAA:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_tbaa='
; CHECK-NEXT:   - ReloadLoadTBAA:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_name='
; CHECK-NEXT:   - ReloadLoadName:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_line='
; CHECK-NEXT:   - ReloadLoadLine:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' has_call_reload='
; CHECK-NEXT:   - HasCallReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_unaliased_load_operand='
; CHECK-NEXT:   - HasUnaliasedLoadOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_phi_operand='
; CHECK-NEXT:   - HasInLoopPhiOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_freeze_operand='
; CHECK-NEXT:   - HasInLoopFreezeOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_select_operand='
; CHECK-NEXT:   - HasInLoopSelectOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_other_in_loop_unknown_operand='
; CHECK-NEXT:   - HasOtherInLoopUnknownOperand:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' has_opaque_operand_no_in_loop_unknown='
; CHECK-NEXT:   - HasOpaqueOperandNoInLoopUnknown:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_outer_loop_addrec_operand='
; CHECK-NEXT:   - HasOuterLoopAddRecOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_overflow_bit_leaf='
; CHECK-NEXT:   - HasOverflowBitLeaf:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' has_checked_arith_value_operand='
; CHECK-NEXT:   - HasCheckedArithValueOperand:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' invocation_seq='
; CHECK-NEXT:   - InvocationSeq:{{ +}}'1'
; CHECK-NEXT: ...
;
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}bounds_check
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}bounds_check
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
; CHECK-NEXT:   - String:{{ +}}' loop_has_other_unknown_btc_trap='
; CHECK-NEXT:   - LoopHasOtherUnknownBTCTrap:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' predicate_shape='
; CHECK-NEXT:   - PredicateShape:{{ +}}SingleICmp
; CHECK-NEXT:   - String:{{ +}}' is_entry_proximate='
; CHECK-NEXT:   - IsEntryProximate:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' dominates_latch='
; CHECK-NEXT:   - DominatesLatch:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' loop_latch_btc_computable='
; CHECK-NEXT:   - LoopLatchBTCComputable:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' has_store_reload='
; CHECK-NEXT:   - HasStoreReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_mem_intrinsic_reload='
; CHECK-NEXT:   - HasMemIntrinsicReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' reload_store_line='
; CHECK-NEXT:   - ReloadStoreLine:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' reload_store_kind='
; CHECK-NEXT:   - ReloadStoreKind:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_store_tbaa='
; CHECK-NEXT:   - ReloadStoreTBAA:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_tbaa='
; CHECK-NEXT:   - ReloadLoadTBAA:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_name='
; CHECK-NEXT:   - ReloadLoadName:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_line='
; CHECK-NEXT:   - ReloadLoadLine:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' has_call_reload='
; CHECK-NEXT:   - HasCallReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_unaliased_load_operand='
; CHECK-NEXT:   - HasUnaliasedLoadOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_phi_operand='
; CHECK-NEXT:   - HasInLoopPhiOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_freeze_operand='
; CHECK-NEXT:   - HasInLoopFreezeOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_in_loop_select_operand='
; CHECK-NEXT:   - HasInLoopSelectOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_other_in_loop_unknown_operand='
; CHECK-NEXT:   - HasOtherInLoopUnknownOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_opaque_operand_no_in_loop_unknown='
; CHECK-NEXT:   - HasOpaqueOperandNoInLoopUnknown:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_outer_loop_addrec_operand='
; CHECK-NEXT:   - HasOuterLoopAddRecOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_overflow_bit_leaf='
; CHECK-NEXT:   - HasOverflowBitLeaf:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' has_checked_arith_value_operand='
; CHECK-NEXT:   - HasCheckedArithValueOperand:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' invocation_seq='
; CHECK-NEXT:   - InvocationSeq:{{ +}}'1'
; CHECK-NEXT: ...
