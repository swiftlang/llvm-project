; RUN: llc -O0 --filetype=obj --cas-backend --cas=%t.casdb --mccas-verify -o %t.bin %s

; RUN: llc -O0 --filetype=obj --cas-backend --cas=%t.casdb --mccas-casid -o %t.casid %s
; RUN: llvm-cas-dump --cas=%t.casdb --dwarf-sections-only --casid-file %t.casid | FileCheck %s

target triple = "arm64-apple-macosx12.0.0"

define i32 @foo1() !dbg !9 {
entry:
  ret i32 2, !dbg !14
}

define i32 @foo2() !dbg !15 {
entry:
  ret i32 2, !dbg !16
}

; CHECK:        mc:group        llvmcas://
; CHECK-NEXT:     mc:section      llvmcas://
; CHECK-NEXT:       mc:debug_abbrev llvmcas://
; CHECK-NEXT:       mc:debug_abbrev llvmcas://
; CHECK-NOT: debug_abbrev


!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "someclang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None, sysroot: "/", casFriendly: DebugAbbrev)
!1 = !DIFile(filename: "test.c", directory: "my/dir")
!2 = !{i32 7, !"Dwarf Version", i32 4}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"PIC Level", i32 2}
!6 = !{i32 7, !"uwtable", i32 2}
!7 = !{i32 7, !"frame-pointer", i32 1}
!8 = !{!"someclang"}
!9 = distinct !DISubprogram(name: "foo1", scope: !1, file: !1, line: 1, type: !10, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !13)
!10 = !DISubroutineType(types: !11)
!11 = !{!12}
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !{}
!14 = !DILocation(line: 2, column: 3, scope: !9)
!15 = distinct !DISubprogram(name: "foo2", scope: !1, file: !1, line: 5, type: !10, scopeLine: 5, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !13)
!16 = !DILocation(line: 6, column: 3, scope: !15)
