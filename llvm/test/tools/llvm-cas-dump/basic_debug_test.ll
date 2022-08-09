; RUN: llc -O0 --filetype=obj --cas-backend --cas=%t.casdb --mccas-casid -o %t.casid %s
; RUN: llvm-cas-dump --cas=%t.casdb --dwarf-sections-only --debug-abbrev-offsets --casid-file %t.casid | FileCheck %s
; RUN: llvm-cas-dump --cas=%t.casdb --dwarf-sections-only --dwarf-dump --casid-file %t.casid | FileCheck %s --check-prefix=DWARF

; This test is created from a C program like:
; int foo() { return 10; }

; CHECK:      mc:assembler  llvmcas://{{.*}}
; CHECK-NEXT:   mc:header  llvmcas://{{.*}}
; CHECK-NEXT:   mc:group  llvmcas://{{.*}}
; CHECK-NEXT:     mc:section  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_abbrev  llvmcas://{{.*}}
; CHECK-NEXT:       mc:padding  llvmcas://{{.*}}
; CHECK-NEXT:     mc:section  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_abbrev_offsets  llvmcas://{{.*}}
; CHECK-NEXT:         0
; CHECK-NEXT:       mc:debug_info_cu  llvmcas://{{.*}}
; CHECK-NEXT:       mc:padding  llvmcas://{{.*}}
; CHECK-NEXT:     mc:section  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_string  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_string  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_string  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_string  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_string  llvmcas://{{.*}}
; CHECK-NEXT:     mc:section  llvmcas://{{.*}}
; CHECK-NEXT:       mc:debug_line  llvmcas://{{.*}}
; CHECK-NEXT:       mc:padding  llvmcas://{{.*}}
; CHECK-NEXT:   mc:data_in_code  llvmcas://{{.*}}
; CHECK-NEXT:   mc:symbol_table  llvmcas://{{.*}}
; CHECK-NEXT:     mc:cstring  llvmcas://{{.*}}
; CHECK-NEXT:     mc:cstring  llvmcas://{{.*}}
; CHECK-NEXT:     mc:cstring  llvmcas://{{.*}}
; CHECK-NEXT:     mc:cstring  llvmcas://{{.*}}

; DWARF: mc:debug_info_cu
; DWARF: Compile Unit: length = 0x00000047, format = DWARF32, version = 0x0004, abbr_offset = 0x0000, addr_size = 0x08
; DWARF: DW_TAG_compile_unit
; DWARF:   DW_AT_producer	("some_clang")
; DWARF:   DW_AT_language	(DW_LANG_C99)
; DWARF:   DW_AT_name	("test.c")
; DWARF:   DW_AT_stmt_list	(0x00000000)
; DWARF:   DW_AT_comp_dir	("some_dir")
; DWARF:   DW_AT_low_pc	(0x0000000000000000)
; DWARF:   DW_AT_high_pc	(0x0000000000000008)

; DWARF: mc:debug_string
; DWARF:   0x00000000: "some_clang"
; DWARF: mc:debug_string
; DWARF:   0x00000000: "test.c"
; DWARF: mc:debug_string
; DWARF:   0x00000000: "some_dir"
; DWARF: mc:debug_string
; DWARF:   0x00000000: "foo"
; DWARF: mc:debug_string
; DWARF:   0x00000000: "int"

; DWARF: mc:debug_line
; DWARF:   debug_line[0x00000000]
; DWARF:   Line table prologue:
; DWARF:       total_length: 0x00000039
; DWARF:             format: DWARF32
; DWARF:            version: 4
; DWARF:    prologue_length: 0x0000001e
; DWARF: 0x0000000000000000      2      0      1   0             0  is_stmt
; DWARF: 0x0000000000000004      3      3      1   0             0  is_stmt prologue_end
; DWARF: 0x0000000000000008      3      3      1   0             0  is_stmt end_sequence

target triple = "arm64-apple-macosx12.0.0"

define i32 @foo() !dbg !9 {
entry:
  ret i32 10, !dbg !14
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "some_clang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None, sysroot: "/", casFriendly: DebugAbbrev)
!1 = !DIFile(filename: "test.c", directory: "some_dir")
!2 = !{i32 7, !"Dwarf Version", i32 4}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"PIC Level", i32 2}
!6 = !{i32 7, !"uwtable", i32 2}
!7 = !{i32 7, !"frame-pointer", i32 1}
!8 = !{!"some_clang"}
!9 = distinct !DISubprogram(name: "foo", scope: !1, file: !1, line: 2, type: !10, scopeLine: 2, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !13)
!10 = !DISubroutineType(types: !11)
!11 = !{!12}
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !{}
!14 = !DILocation(line: 3, column: 3, scope: !9)
