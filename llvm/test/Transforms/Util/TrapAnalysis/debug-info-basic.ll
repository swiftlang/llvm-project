; A LoopTrapEdge record is anchored at the trapping conditional branch, so when
; the input carries debug info the record's DebugLoc points back at that branch's
; source line/column. This checks the DebugLoc is emitted and matches the !dbg on
; the in-loop trap branch (line 8), not the trap call or another instruction.

; RUN: opt -passes='loop-trap-analysis' -loop-trap-analysis-explain -disable-output \
; RUN:   -pass-remarks-output=%t.yaml %s
; RUN: FileCheck --input-file=%t.yaml %s

define void @counted_trap(ptr %base, i32 %n) !dbg !4 {
entry:
  br label %body

body:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %latch ]
  %cmp = icmp ult i32 %iv, %n
  br i1 %cmp, label %latch, label %trap, !dbg !7

trap:
  call void @llvm.trap(), !dbg !8
  unreachable

latch:
  %iv.next = add nuw nsw i32 %iv, 1
  %e = icmp eq i32 %iv.next, %n
  br i1 %e, label %exit, label %body

exit:
  ret void
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly)
!1 = !DIFile(filename: "scan.c", directory: "/tmp")
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "counted_trap", scope: !1, file: !1, line: 5, type: !5, scopeLine: 5, spFlags: DISPFlagDefinition, unit: !0)
!5 = !DISubroutineType(types: !6)
!6 = !{null}
!7 = !DILocation(line: 8, column: 7, scope: !4)
!8 = !DILocation(line: 9, column: 5, scope: !4)

; The record's DebugLoc is the trapping branch's !dbg (line 8, col 7).

; Full LoopTrapEdge record(s), pinned line-by-line.

; Full LoopTrapEdge record(s), pinned line-by-line.
; CHECK:      Name:{{ +}}LoopTrapEdge
; CHECK-NEXT: DebugLoc:{{ +}}{ File: scan.c, Line: 8, Column: 7 }
; CHECK-NEXT: Function:{{ +}}counted_trap
; CHECK-NEXT: Args:
; CHECK-NEXT:   - String:{{ +}}'Function '
; CHECK-NEXT:   - Function:{{ +}}counted_trap
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
; CHECK-NEXT: ...
