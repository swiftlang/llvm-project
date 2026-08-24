; RUN: opt -passes='loop-trap-analysis' --use-bounds-safety-traps-only -pass-remarks-missed='loop-trap-analysis' -disable-output -pass-remarks-output=%t.opt.yaml %s
; RUN: FileCheck --check-prefixes OPT-REM  --input-file=%t.opt.yaml %s

; OPT-REM: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopPrimitives
; OPT-REM-NEXT: Function:        write_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Loop '
; OPT-REM-NEXT:   - LoopHeader:      for.body
; OPT-REM-NEXT:   - String:          ' depth='
; OPT-REM-NEXT:   - Depth:           '1'
; OPT-REM-NEXT:   - String:          ' parent='
; OPT-REM-NEXT:   - ParentHeader:    '-'
; OPT-REM-NEXT:   - String:          ' innermost='
; OPT-REM-NEXT:   - IsInnermost:     'true'
; OPT-REM-NEXT:   - String:          ' blocks='
; OPT-REM-NEXT:   - BlockCount:      '2'
; OPT-REM-NEXT:   - String:          ' trap_exits='
; OPT-REM-NEXT:   - TrapExitCount:   '1'
; OPT-REM-NEXT:   - String:          ' cond_trap_edges='
; OPT-REM-NEXT:   - CondTrapEdgeCount: '1'
; OPT-REM-NEXT:   - String:          ' hoistable_cond_trap_edges='
; OPT-REM-NEXT:   - HoistableCondTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_invariant='
; OPT-REM-NEXT:   - TrapCondInvariant: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_iv_derived='
; OPT-REM-NEXT:   - TrapCondIVDerived: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_non_iv='
; OPT-REM-NEXT:   - TrapCondNonIV:   '1'
; OPT-REM-NEXT:   - String:          ' btc_known='
; OPT-REM-NEXT:   - BTCKnown:        'false'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc='
; OPT-REM-NEXT:   - TrapExitsUnknownBTC: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCDueToReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_other='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCOtherReason: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_store_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCStoreReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_call_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCCallReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_other_blocker='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCOtherBlocker: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTC: '1'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCDueToReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_other='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCOtherReason: '1'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_store_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCStoreReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_call_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCCallReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_other_blocker='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCOtherBlocker: '1'
; OPT-REM-NEXT:   - String:          ' trap_exits_computable_btc='
; OPT-REM-NEXT:   - TrapExitsComputableBTC: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_computable_btc='
; OPT-REM-NEXT:   - NonTrapExitsComputableBTC: '1'
; OPT-REM-NEXT:   - String:          ' trap_exits_symbolic_btc='
; OPT-REM-NEXT:   - TrapExitsSymbolicBTC: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_symbolic_btc='
; OPT-REM-NEXT:   - NonTrapExitsSymbolicBTC: '0'
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopPrimitivesSummary
; OPT-REM-NEXT: Function:        write_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Function '
; OPT-REM-NEXT:   - Function:        write_checks
; OPT-REM-NEXT:   - String:          ' total_loops='
; OPT-REM-NEXT:   - TotalLoops:      '1'
; OPT-REM-NEXT:   - String:          ' innermost='
; OPT-REM-NEXT:   - Innermost:       '1'
; OPT-REM-NEXT:   - String:          ' loops_with_traps='
; OPT-REM-NEXT:   - LoopsWithTraps:  '1'
; OPT-REM-NEXT:   - String:          ' loops_with_traps_unknown_btc='
; OPT-REM-NEXT:   - LoopsWithTrapsUnknownBTC: '1'
; OPT-REM-NEXT:   - String:          ' max_depth='
; OPT-REM-NEXT:   - MaxDepth:        '1'
; OPT-REM-NEXT:   - String:          ' depth1='
; OPT-REM-NEXT:   - Depth1:          '1'
; OPT-REM-NEXT:   - String:          ' depth2='
; OPT-REM-NEXT:   - Depth2:          '0'
; OPT-REM-NEXT:   - String:          ' depth3+='
; OPT-REM-NEXT:   - Depth3Plus:      '0'
; OPT-REM-NEXT:   - String:          ' unique_trap_blocks='
; OPT-REM-NEXT:   - UniqueTrapBlocks: '0'
; OPT-REM-NEXT:   - String:          ' unique_trap_blocks_reachable_from_loop='
; OPT-REM-NEXT:   - UniqueTrapBlocksReachableFromLoop: '0'
; OPT-REM-NEXT:   - String:          ' in_loop_trap_edges='
; OPT-REM-NEXT:   - InLoopTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' in_loop_hoistable_trap_edges='
; OPT-REM-NEXT:   - InLoopHoistableTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' out_of_loop_trap_edges='
; OPT-REM-NEXT:   - OutOfLoopTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_invariant_total='
; OPT-REM-NEXT:   - TrapCondInvariantTotal: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_iv_derived_total='
; OPT-REM-NEXT:   - TrapCondIVDerivedTotal: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_non_iv_total='
; OPT-REM-NEXT:   - TrapCondNonIVTotal: '1'
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopTrap
; OPT-REM-NEXT: Function:        write_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Loop: '
; OPT-REM-NEXT:   - String:          for.body
; OPT-REM-NEXT:   - String:          ' '
; OPT-REM-NEXT:   - String:          'TrapExits: '
; OPT-REM-NEXT:   - TrapExitCount:   '1'
; OPT-REM-NEXT:   - String:          ' '
; OPT-REM-NEXT:   - String:          "cannot be hoisted: \n"
; OPT-REM-NEXT:   - String:           |
; OPT-REM-NEXT: {{^[ 	]+$}}
; OPT-REM-NEXT:       The following instructions have side effects:
; OPT-REM-NEXT:   - String:          '	'
; OPT-REM-NEXT:   - String:          '  store i32 1, ptr %ptr.ind, align 4'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          "Reason:\n"
; OPT-REM-NEXT:   - String:          "Store.may-write-to-memory\n"
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopTrapSummary
; OPT-REM-NEXT: Function:        write_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          "Trap checks results:\n"
; OPT-REM-NEXT:   - String:          'Total count of loops with traps '
; OPT-REM-NEXT:   - TotalCount:      '1'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops that maybe can be hoisted: '
; OPT-REM-NEXT:   - CountHoist:      '0'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops that cannot be hoisted: '
; OPT-REM-NEXT:   - CountCannotHoist: '1'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops with trap check hoisted to preheader: '
; OPT-REM-NEXT:   - CountHoisted:    '0'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT: ...
define void @write_checks(ptr %base, i32 %N) {
entry:
  %ptr.lb = getelementptr i32, ptr %base, i32 0
  %ptr.ub = getelementptr i32, ptr %base, i32 %N 
  %cmp9.not = icmp eq i32 %N, 0
  br i1 %cmp9.not, label %for.cond.cleanup, label %for.body.preheader

for.body.preheader:                               ; preds = %entry
  br label %for.body

for.cond.cleanup:                                 ; preds = %cont6, %entry
  ret void

for.body:                                         ; preds = %for.body.preheader, %cont6
  %indvars.iv = phi i32 [ 0, %for.body.preheader ], [ %indvars.iv.next, %cont6 ]
  %ptr.ind = getelementptr i32, ptr %base, i32 %indvars.iv
  %cmp.ult = icmp ult ptr %ptr.ind, %ptr.ub, !annotation !1
  %cmp.uge = icmp uge ptr %ptr.ind, %ptr.lb, !annotation !2
  %or.cond = and i1 %cmp.ult, %cmp.uge, !annotation !2
  br i1 %or.cond, label %cont6, label %trap, !annotation !1

trap:                                             ; preds = %for.body
  tail call void @llvm.ubsantrap(i8 25), !annotation !3
  unreachable, !annotation !3

cont6:                                            ; preds = %for.body
  store i32 1, ptr %ptr.ind, align 4
  %indvars.iv.next = add nuw nsw i32 %indvars.iv, 1
  %exitcond.not = icmp eq i32 %indvars.iv.next, %N
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}

; OPT-REM: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopPrimitives
; OPT-REM-NEXT: Function:        accumulate_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Loop '
; OPT-REM-NEXT:   - LoopHeader:      for.body
; OPT-REM-NEXT:   - String:          ' depth='
; OPT-REM-NEXT:   - Depth:           '1'
; OPT-REM-NEXT:   - String:          ' parent='
; OPT-REM-NEXT:   - ParentHeader:    '-'
; OPT-REM-NEXT:   - String:          ' innermost='
; OPT-REM-NEXT:   - IsInnermost:     'true'
; OPT-REM-NEXT:   - String:          ' blocks='
; OPT-REM-NEXT:   - BlockCount:      '2'
; OPT-REM-NEXT:   - String:          ' trap_exits='
; OPT-REM-NEXT:   - TrapExitCount:   '1'
; OPT-REM-NEXT:   - String:          ' cond_trap_edges='
; OPT-REM-NEXT:   - CondTrapEdgeCount: '1'
; OPT-REM-NEXT:   - String:          ' hoistable_cond_trap_edges='
; OPT-REM-NEXT:   - HoistableCondTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_invariant='
; OPT-REM-NEXT:   - TrapCondInvariant: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_iv_derived='
; OPT-REM-NEXT:   - TrapCondIVDerived: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_non_iv='
; OPT-REM-NEXT:   - TrapCondNonIV:   '1'
; OPT-REM-NEXT:   - String:          ' btc_known='
; OPT-REM-NEXT:   - BTCKnown:        'false'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc='
; OPT-REM-NEXT:   - TrapExitsUnknownBTC: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCDueToReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_other='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCOtherReason: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_store_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCStoreReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_call_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCCallReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_other_blocker='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCOtherBlocker: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTC: '1'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCDueToReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_other='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCOtherReason: '1'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_store_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCStoreReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_call_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCCallReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_other_blocker='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCOtherBlocker: '1'
; OPT-REM-NEXT:   - String:          ' trap_exits_computable_btc='
; OPT-REM-NEXT:   - TrapExitsComputableBTC: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_computable_btc='
; OPT-REM-NEXT:   - NonTrapExitsComputableBTC: '1'
; OPT-REM-NEXT:   - String:          ' trap_exits_symbolic_btc='
; OPT-REM-NEXT:   - TrapExitsSymbolicBTC: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_symbolic_btc='
; OPT-REM-NEXT:   - NonTrapExitsSymbolicBTC: '0'
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopPrimitivesSummary
; OPT-REM-NEXT: Function:        accumulate_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Function '
; OPT-REM-NEXT:   - Function:        accumulate_checks
; OPT-REM-NEXT:   - String:          ' total_loops='
; OPT-REM-NEXT:   - TotalLoops:      '1'
; OPT-REM-NEXT:   - String:          ' innermost='
; OPT-REM-NEXT:   - Innermost:       '1'
; OPT-REM-NEXT:   - String:          ' loops_with_traps='
; OPT-REM-NEXT:   - LoopsWithTraps:  '1'
; OPT-REM-NEXT:   - String:          ' loops_with_traps_unknown_btc='
; OPT-REM-NEXT:   - LoopsWithTrapsUnknownBTC: '1'
; OPT-REM-NEXT:   - String:          ' max_depth='
; OPT-REM-NEXT:   - MaxDepth:        '1'
; OPT-REM-NEXT:   - String:          ' depth1='
; OPT-REM-NEXT:   - Depth1:          '1'
; OPT-REM-NEXT:   - String:          ' depth2='
; OPT-REM-NEXT:   - Depth2:          '0'
; OPT-REM-NEXT:   - String:          ' depth3+='
; OPT-REM-NEXT:   - Depth3Plus:      '0'
; OPT-REM-NEXT:   - String:          ' unique_trap_blocks='
; OPT-REM-NEXT:   - UniqueTrapBlocks: '0'
; OPT-REM-NEXT:   - String:          ' unique_trap_blocks_reachable_from_loop='
; OPT-REM-NEXT:   - UniqueTrapBlocksReachableFromLoop: '0'
; OPT-REM-NEXT:   - String:          ' in_loop_trap_edges='
; OPT-REM-NEXT:   - InLoopTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' in_loop_hoistable_trap_edges='
; OPT-REM-NEXT:   - InLoopHoistableTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' out_of_loop_trap_edges='
; OPT-REM-NEXT:   - OutOfLoopTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_invariant_total='
; OPT-REM-NEXT:   - TrapCondInvariantTotal: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_iv_derived_total='
; OPT-REM-NEXT:   - TrapCondIVDerivedTotal: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_non_iv_total='
; OPT-REM-NEXT:   - TrapCondNonIVTotal: '1'
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopTrap
; OPT-REM-NEXT: Function:        accumulate_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Loop: '
; OPT-REM-NEXT:   - String:          for.body
; OPT-REM-NEXT:   - String:          ' '
; OPT-REM-NEXT:   - String:          'TrapExits: '
; OPT-REM-NEXT:   - TrapExitCount:   '1'
; OPT-REM-NEXT:   - String:          ' '
; OPT-REM-NEXT:   - String:          "can be hoisted\n"
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopTrapSummary
; OPT-REM-NEXT: Function:        accumulate_checks
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          "Trap checks results:\n"
; OPT-REM-NEXT:   - String:          'Total count of loops with traps '
; OPT-REM-NEXT:   - TotalCount:      '1'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops that maybe can be hoisted: '
; OPT-REM-NEXT:   - CountHoist:      '1'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops that cannot be hoisted: '
; OPT-REM-NEXT:   - CountCannotHoist: '0'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops with trap check hoisted to preheader: '
; OPT-REM-NEXT:   - CountHoisted:    '0'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT: ...
define void @accumulate_checks(ptr %base, i32 %N) {
entry:
  %ptr.lb = getelementptr i32, ptr %base, i32 0
  %ptr.ub = getelementptr i32, ptr %base, i32 %N 
  %cmp9.not = icmp eq i32 %N, 0
  br i1 %cmp9.not, label %for.cond.cleanup, label %for.body.preheader

for.body.preheader:                               ; preds = %entry
  br label %for.body

for.cond:                                         ; preds = %for.body
  %indvars.iv.next = add nuw nsw i32 %indvars.iv, 1
  %exitcond.not = icmp eq i32 %indvars.iv.next, %N
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body 

for.cond.cleanup:                                 ; preds = %for.cond, %entry
  ret void

for.body:                                         ; preds = %for.body.preheader, %for.cond
  %indvars.iv = phi i32 [ 0, %for.body.preheader ], [ %indvars.iv.next, %for.cond ]
  %ptr.ind = getelementptr i32, ptr %base, i32 %indvars.iv
  %cmp.ult = icmp ult ptr %ptr.ind, %ptr.ub, !annotation !1
  %cmp.uge = icmp uge ptr %ptr.ind, %ptr.lb, !annotation !2
  %or.cond = and i1 %cmp.ult, %cmp.uge, !annotation !2
  br i1 %or.cond, label %for.cond, label %trap, !annotation !1

trap:                                             ; preds = %for.body
  tail call void @llvm.ubsantrap(i8 25), !annotation !3
  unreachable, !annotation !3
}

; OPT-REM: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopPrimitives
; OPT-REM-NEXT: Function:        trip_count_unknown
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Loop '
; OPT-REM-NEXT:   - LoopHeader:      loop
; OPT-REM-NEXT:   - String:          ' depth='
; OPT-REM-NEXT:   - Depth:           '1'
; OPT-REM-NEXT:   - String:          ' parent='
; OPT-REM-NEXT:   - ParentHeader:    '-'
; OPT-REM-NEXT:   - String:          ' innermost='
; OPT-REM-NEXT:   - IsInnermost:     'true'
; OPT-REM-NEXT:   - String:          ' blocks='
; OPT-REM-NEXT:   - BlockCount:      '3'
; OPT-REM-NEXT:   - String:          ' trap_exits='
; OPT-REM-NEXT:   - TrapExitCount:   '1'
; OPT-REM-NEXT:   - String:          ' cond_trap_edges='
; OPT-REM-NEXT:   - CondTrapEdgeCount: '1'
; OPT-REM-NEXT:   - String:          ' hoistable_cond_trap_edges='
; OPT-REM-NEXT:   - HoistableCondTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_invariant='
; OPT-REM-NEXT:   - TrapCondInvariant: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_iv_derived='
; OPT-REM-NEXT:   - TrapCondIVDerived: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_non_iv='
; OPT-REM-NEXT:   - TrapCondNonIV:   '1'
; OPT-REM-NEXT:   - String:          ' btc_known='
; OPT-REM-NEXT:   - BTCKnown:        'false'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc='
; OPT-REM-NEXT:   - TrapExitsUnknownBTC: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCDueToReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_other='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCOtherReason: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_store_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCStoreReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_call_reload='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCCallReload: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_unknown_btc_other_blocker='
; OPT-REM-NEXT:   - TrapExitsUnknownBTCOtherBlocker: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTC: '3'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCDueToReload: '2'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_other='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCOtherReason: '1'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_store_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCStoreReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_call_reload='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCCallReload: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_unknown_btc_other_blocker='
; OPT-REM-NEXT:   - NonTrapExitsUnknownBTCOtherBlocker: '3'
; OPT-REM-NEXT:   - String:          ' trap_exits_computable_btc='
; OPT-REM-NEXT:   - TrapExitsComputableBTC: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_computable_btc='
; OPT-REM-NEXT:   - NonTrapExitsComputableBTC: '0'
; OPT-REM-NEXT:   - String:          ' trap_exits_symbolic_btc='
; OPT-REM-NEXT:   - TrapExitsSymbolicBTC: '0'
; OPT-REM-NEXT:   - String:          ' non_trap_exits_symbolic_btc='
; OPT-REM-NEXT:   - NonTrapExitsSymbolicBTC: '0'
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopPrimitivesSummary
; OPT-REM-NEXT: Function:        trip_count_unknown
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Function '
; OPT-REM-NEXT:   - Function:        trip_count_unknown
; OPT-REM-NEXT:   - String:          ' total_loops='
; OPT-REM-NEXT:   - TotalLoops:      '1'
; OPT-REM-NEXT:   - String:          ' innermost='
; OPT-REM-NEXT:   - Innermost:       '1'
; OPT-REM-NEXT:   - String:          ' loops_with_traps='
; OPT-REM-NEXT:   - LoopsWithTraps:  '1'
; OPT-REM-NEXT:   - String:          ' loops_with_traps_unknown_btc='
; OPT-REM-NEXT:   - LoopsWithTrapsUnknownBTC: '1'
; OPT-REM-NEXT:   - String:          ' max_depth='
; OPT-REM-NEXT:   - MaxDepth:        '1'
; OPT-REM-NEXT:   - String:          ' depth1='
; OPT-REM-NEXT:   - Depth1:          '1'
; OPT-REM-NEXT:   - String:          ' depth2='
; OPT-REM-NEXT:   - Depth2:          '0'
; OPT-REM-NEXT:   - String:          ' depth3+='
; OPT-REM-NEXT:   - Depth3Plus:      '0'
; OPT-REM-NEXT:   - String:          ' unique_trap_blocks='
; OPT-REM-NEXT:   - UniqueTrapBlocks: '0'
; OPT-REM-NEXT:   - String:          ' unique_trap_blocks_reachable_from_loop='
; OPT-REM-NEXT:   - UniqueTrapBlocksReachableFromLoop: '0'
; OPT-REM-NEXT:   - String:          ' in_loop_trap_edges='
; OPT-REM-NEXT:   - InLoopTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' in_loop_hoistable_trap_edges='
; OPT-REM-NEXT:   - InLoopHoistableTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' out_of_loop_trap_edges='
; OPT-REM-NEXT:   - OutOfLoopTrapEdges: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_invariant_total='
; OPT-REM-NEXT:   - TrapCondInvariantTotal: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_iv_derived_total='
; OPT-REM-NEXT:   - TrapCondIVDerivedTotal: '0'
; OPT-REM-NEXT:   - String:          ' trap_cond_non_iv_total='
; OPT-REM-NEXT:   - TrapCondNonIVTotal: '1'
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopTrap
; OPT-REM-NEXT: Function:        trip_count_unknown
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          'Loop: '
; OPT-REM-NEXT:   - String:          loop
; OPT-REM-NEXT:   - String:          ' '
; OPT-REM-NEXT:   - String:          'TrapExits: '
; OPT-REM-NEXT:   - TrapExitCount:   '1'
; OPT-REM-NEXT:   - String:          ' '
; OPT-REM-NEXT:   - String:          "cannot be hoisted: \n"
; OPT-REM-NEXT:   - String:          "Backedge is not computable.\n"
; OPT-REM-NEXT: ...
; OPT-REM-NEXT: --- !Analysis
; OPT-REM-NEXT: Pass:            loop-trap-analysis
; OPT-REM-NEXT: Name:            LoopTrapSummary
; OPT-REM-NEXT: Function:        trip_count_unknown
; OPT-REM-NEXT: Args:
; OPT-REM-NEXT:   - String:          "Trap checks results:\n"
; OPT-REM-NEXT:   - String:          'Total count of loops with traps '
; OPT-REM-NEXT:   - TotalCount:      '1'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops that maybe can be hoisted: '
; OPT-REM-NEXT:   - CountHoist:      '0'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops that cannot be hoisted: '
; OPT-REM-NEXT:   - CountCannotHoist: '1'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT:   - String:          'Loops with trap check hoisted to preheader: '
; OPT-REM-NEXT:   - CountHoisted:    '0'
; OPT-REM-NEXT:   - String:          "\n"
; OPT-REM-NEXT: ...
define void @trip_count_unknown(ptr %A, ptr %B, i32 %N, i32 %M) {
entry:
  %cmp37.not = icmp eq i32 %N, 0
  br i1 %cmp37.not, label %exit, label %for.body.lr.ph

for.body.lr.ph:                                   ; preds = %entry
  %idx.ext3 = zext i32 %M to i64
  %add.ptr4 = getelementptr inbounds i32, ptr %B, i64 %idx.ext3
  %wide.trip.count = zext i32 %N to i64
  br label %loop

loop:                                         
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %next ]
  %a.iv.next = getelementptr i32, ptr %A, i64 %indvars.iv
  %b.iv.next = getelementptr i32, ptr %B, i64 %indvars.iv
  %cond = icmp ule i64 %indvars.iv, %wide.trip.count
  %cmp.b.ult = icmp ult ptr %b.iv.next, %add.ptr4, !annotation !1
  %cmp.b.uge = icmp uge ptr %b.iv.next, %B, !annotation !2
  %or.cond = and i1 %cmp.b.ult, %cmp.b.uge, !annotation !2
  %b.at.i = load i32, ptr %b.iv.next
  %loop.cond = icmp eq i32 %b.at.i, 0
  br i1 %loop.cond, label %cond1, label %exit

cond1:                                           ; preds = %for.body
  br i1 %or.cond, label %trap, label %next, !annotation !2

next: 
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  br i1 %loop.cond, label %loop, label %exit

trap:                                             
  tail call void @llvm.ubsantrap(i8 25), !annotation !3
  unreachable, !annotation !3

exit:                                          
  ret void
}

declare void @llvm.ubsantrap(i8 immarg) 

!1 = !{!"bounds-safety-check-ptr-lt-upper-bound"}
!2 = !{!"bounds-safety-check-ptr-ge-lower-bound"}
!3 = !{!"bounds-safety-check-ptr-lt-upper-bound", !"bounds-safety-check-ptr-ge-lower-bound"}

; OPT-REM-NOT: --- !Analysis
