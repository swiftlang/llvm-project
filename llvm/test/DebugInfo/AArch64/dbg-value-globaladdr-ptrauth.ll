;; Check that an "llvm.ptrauth" signed pointer wrapper is never named as the
;; location of a variable.

; RUN: llc -O2 -mtriple=arm64e-apple-macosx -stop-after=finalize-isel < %s \
; RUN:   | FileCheck %s --check-prefix=SDAG --implicit-check-not='DBG_VALUE @'
; RUN: llc -O2 -mtriple=arm64e-apple-macosx -global-isel -stop-after=ir-translator \
; RUN:   < %s | FileCheck %s --check-prefix=GISEL --implicit-check-not='DBG_VALUE @'
; RUN: llc -O2 -mtriple=arm64e-apple-macosx -filetype=obj < %s \
; RUN:   | llvm-objdump -r - | FileCheck %s --check-prefix=RELOC \
; RUN:       --implicit-check-not=fn.ptrauth

@fn.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @fn, i32 0, i64 0, i64 0 }, section "llvm.ptrauth", align 8

;; Nothing else uses the signed pointer, so there is nothing left to describe
;; the variable by.
; SDAG-LABEL: name: wrapper_only
; SDAG: DBG_VALUE $noreg, $noreg, ![[#]], !DIExpression()
; GISEL-LABEL: name: wrapper_only
; GISEL: DBG_VALUE $noreg, 0, ![[#]], !DIExpression()
define void @wrapper_only() !dbg !6 {
entry:
    #dbg_value(ptr @fn.ptrauth, !9, !DIExpression(), !10)
  tail call void @sink(ptr null), !dbg !10
  ret void, !dbg !10
}

;; The signed pointer is materialized for the call, so the variable is described
;; by the register holding it.
; SDAG-LABEL: name: wrapper_passed
; SDAG: DBG_VALUE %[[#]], $noreg, ![[#]], !DIExpression()
; GISEL-LABEL: name: wrapper_passed
; GISEL: [[SIGNED:%[0-9]+]]:_(p0) = G_PTRAUTH_GLOBAL_VALUE
; GISEL: DBG_VALUE [[SIGNED]](p0), $noreg, ![[#]], !DIExpression()
define void @wrapper_passed() !dbg !11 {
entry:
    #dbg_value(ptr @fn.ptrauth, !12, !DIExpression(), !13)
  tail call void @sink(ptr @fn.ptrauth), !dbg !13
  ret void, !dbg !13
}

;; Nothing, and the address pool in particular, may refer to the wrapper.
; RELOC: RELOCATION RECORDS FOR [__debug_addr]:

declare i64 @fn(double)
declare void @sink(ptr)

!llvm.module.flags = !{!0, !1}
!llvm.dbg.cu = !{!2}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !{i32 7, !"Dwarf Version", i32 5}
!2 = distinct !DICompileUnit(language: DW_LANG_C11, file: !3, producer: "clang", isOptimized: true, emissionKind: FullDebug)
!3 = !DIFile(filename: "t.c", directory: "/")
!4 = !DISubroutineType(types: !{null})
!5 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!6 = distinct !DISubprogram(name: "wrapper_only", scope: !3, file: !3, line: 1, type: !4, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2)
!7 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!8 = !DISubroutineType(types: !{!5})
!9 = !DILocalVariable(name: "f", scope: !6, file: !3, line: 2, type: !7)
!10 = !DILocation(line: 2, column: 1, scope: !6)
!11 = distinct !DISubprogram(name: "wrapper_passed", scope: !3, file: !3, line: 5, type: !4, scopeLine: 5, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2)
!12 = !DILocalVariable(name: "g", scope: !11, file: !3, line: 6, type: !7)
!13 = !DILocation(line: 6, column: 1, scope: !11)
