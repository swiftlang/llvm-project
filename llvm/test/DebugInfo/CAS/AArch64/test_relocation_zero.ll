;RUN: llc -O0 --filetype=obj --cas-backend  --cas=%t/cas  --mccas-casid %s -o %t/test.id
;RUN: llvm-cas-dump --cas=%t/cas --casid-file %t/test.id --hex-dump --hex-dump-one-line | FileCheck %s

; This file checks to see if relocation addends are zero-ed out correctly in the CAS. The offset's 30, 43, 94, and 107 should all have 8 0x00 bytes.

;CHECK: mc:debug_info_cu llvmcas://{{[0-9a-z]+}}
;CHECK-NEXT: 0x00 0x00 0x00 0x00
;CHECK-SAME: 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
;CHECK-SAME: 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
;CHECK-NEXT: mc:debug_info_cu llvmcas://{{[0-9a-z]+}}
;CHECK-NEXT: 0x00 0x00 0x00 0x00
;CHECK-SAME: 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
;CHECK-SAME: 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00

target triple = "arm64-apple-macosx12.0.0"

define void @foo(i32 noundef %x) #0 !dbg !9 {
  ret void, !dbg !16
}
define void @bar(i32 noundef %x) #0 !dbg !17 {
  ret void, !dbg !20
}
!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !7}
!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 16.0.0 (https://github.com/apple/llvm-project.git 4a36109b6b7cbe6f88be348fc0073875b19636ed)", emissionKind: FullDebug, casFriendly: DebugAbbrev)
!1 = !DIFile(filename: "c.c", directory: "/Users/shubham/Development/testclang")
!2 = !{i32 7, !"Dwarf Version", i32 4}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!7 = !{i32 7, !"frame-pointer", i32 1}
!9 = distinct !DISubprogram(name: "foo", file: !1, line: 1, type: !10, unit: !0, retainedNodes: !13)
!10 = !DISubroutineType(types: !11)
!11 = !{null, !12}
!12 = !DIBasicType(name: "int", encoding: DW_ATE_signed)
!13 = !{}
!16 = !DILocation(line: 2, column: 3, scope: !9)
!17 = distinct !DISubprogram(name: "bar", file: !1, line: 4, type: !10, unit: !0, retainedNodes: !13)
!20 = !DILocation(line: 5, scope: !17)