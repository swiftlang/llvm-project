; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s \
; RUN:   --implicit-check-not=LoopTrapEdge --implicit-check-not=fakecall \
; RUN:   --implicit-check-not=bare

declare void @sideeffect()

; The loop guards a real trap block (%realtrap: @llvm.trap + unreachable) plus
; two blocks that also end in `unreachable` but are NOT trap blocks:
;   %fakecall - unreachable preceded by a normal, returning call, and
;   %bare     - a lone `unreachable` with no preceding instruction.
; isTrapEdgeBlock accepts only %realtrap, so exactly one LoopTrapEdge is
; emitted and the two non-trap edges contribute nothing (enforced by the
; --implicit-check-not directives above).
define void @mixed(ptr %base, i32 %n) {
entry:
  br label %body

body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %c0 = icmp ult i32 %iv, %n
  br i1 %c0, label %s1, label %realtrap

realtrap:
  call void @llvm.trap()
  unreachable

s1:
  %c1 = icmp eq i32 %iv, 3
  br i1 %c1, label %s2, label %fakecall

fakecall:
  call void @sideeffect()
  unreachable

s2:
  %c2 = icmp eq i32 %iv, 5
  br i1 %c2, label %latch, label %bare

bare:
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

; The single LoopTrapEdge record, pinned line-by-line; it targets %realtrap.

; Full LoopTrapEdge record(s), pinned line-by-line.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: Function:{{ +}}mixed
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}mixed
; CHECK-NEXT:   - String:{{ +}}' src_bb='
; CHECK-NEXT:   - SourceBB:{{ +}}body
; CHECK-NEXT:   - String:{{ +}}' trap_bb='
; CHECK-NEXT:   - TrapBB:{{ +}}realtrap
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
; CHECK-NEXT:   - String:{{ +}}' invocation_seq='
; CHECK-NEXT:   - InvocationSeq:{{ +}}'1'
; CHECK-NEXT: ...
