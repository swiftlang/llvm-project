; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

; Exercises the reload/alias + opaque-operand classification via the operand NV
; fields (HasStoreReload, HasInLoopPhiOperand, HasOtherInLoopUnknownOperand, ...).
; All three loops have a non-computable per-edge trip count
; (EdgeBTCComputable=false), so these blocker fields are what a consumer keys on.

declare i32 @opaque()

; In-loop trap exit whose condition reloads %p, which a same-loop store to %q
; could alias and modify -> Opaque-...-StoreReload.
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

; In-loop trap exit whose condition operand is the result of @opaque(), which
; has no characterizable SCEV -> Opaque-...-OpaqueOperand-Other.
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

; In-loop trap exit whose condition depends on %acc, a non-IV phi updated only
; on some iterations (via a select) -> Opaque-...-InLoopPhiOperand.
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

; Full LoopTrapEdge record(s), pinned line-by-line.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}store_reload
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}store_reload
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
; CHECK-NEXT:   - EdgeBTCComputable:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' edge_btc_symbolic='
; CHECK-NEXT:   - EdgeBTCSymbolic:{{ +}}'true'
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
; CHECK-NEXT:   - HasStoreReload:{{ +}}'true'
; CHECK-NEXT:   - String:{{ +}}' has_mem_intrinsic_reload='
; CHECK-NEXT:   - HasMemIntrinsicReload:{{ +}}'false'
; CHECK-NEXT:   - String:{{ +}}' reload_store_line='
; CHECK-NEXT:   - ReloadStoreLine:{{ +}}'0'
; CHECK-NEXT:   - String:{{ +}}' reload_store_kind='
; CHECK-NEXT:   - ReloadStoreKind:{{ +}}store
; CHECK-NEXT:   - String:{{ +}}' reload_store_tbaa='
; CHECK-NEXT:   - ReloadStoreTBAA:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_tbaa='
; CHECK-NEXT:   - ReloadLoadTBAA:{{ +}}''
; CHECK-NEXT:   - String:{{ +}}' reload_load_name='
; CHECK-NEXT:   - ReloadLoadName:{{ +}}p
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
;
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}opaque_other
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}opaque_other
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
; CHECK-NEXT:   - EdgeBTCComputable:{{ +}}'false'
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
; CHECK-NEXT:   - HasOtherInLoopUnknownOperand:{{ +}}'true'
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
;
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}in_loop_phi_operand
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}in_loop_phi_operand
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
; CHECK-NEXT:   - EdgeBTCComputable:{{ +}}'false'
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
; CHECK-NEXT:   - HasInLoopPhiOperand:{{ +}}'true'
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
