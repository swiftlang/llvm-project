; RUN: rm -rf %t && mkdir -p %t
; RUN: llc -O2 -mtriple=aarch64-unknown-linux-gnu --filetype=obj -o %t/file.o %s
; RUN: llvm-dwarfdump %t/file.o -name=_ZN4llvmlsERNS_11raw_ostreamERKNS_5ErrorE -c | FileCheck %s

; This testcase was obtained by looking at FileCheck.cpp and reducing it down via llvm-reduce

; CHECK: DW_TAG_variable
; CHECK-NEXT: DW_AT_location	(0x{{[0-9a-f]+}}: 
; CHECK-NEXT: [0x{{[0-9a-f]+}}, 0x{{[0-9a-f]+}}): DW_OP_reg{{[0-9]+}} W{{[0-9]+}})
; CHECK-NEXT: DW_AT_name	("P")

target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macosx15.0.0"

define ptr @_ZNK4llvm5Error6getPtrEv(ptr %this) {
entry:
  %0 = ptrtoint ptr %this to i64
  %and = and i64 %0, -2
  %1 = inttoptr i64 %and to ptr
  ret ptr %1
}

define ptr @_ZN4llvmlsERNS_11raw_ostreamERKNS_5ErrorE(ptr %E) !dbg !4 {
entry:
  %call = call ptr @_ZNK4llvm5Error6getPtrEv(ptr %E), !dbg !13
    #dbg_value(ptr %call, !9, !DIExpression(), !14)
  %tobool.not = icmp eq ptr %call, null
  br i1 %tobool.not, label %if.else, label %if.then

if.then:                                          ; preds = %entry
  store volatile ptr null, ptr %call, align 8
  unreachable

if.else:                                          ; preds = %entry
  ret ptr null
}

!llvm.module.flags = !{!0}
!llvm.dbg.cu = !{!1}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !2, producer: "clang version 21.0.0git (\0A\0A\0Agit@github.com:llvm/llvm-project.git 6be6400848eeec027d0cca0662c105683bcc896b\0A\0A\0A)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !3, retainedTypes: !3, globals: !3, imports: !3, splitDebugInlining: false, nameTableKind: Apple, sysroot: "/Library/Developer/CommandLineTools/SDKs/MacOSX15.3.sdk", sdk: "MacOSX15.3.sdk")
!2 = !DIFile(filename: "/Users/shubhamrastogi/Development/llvm-project-instr-ref/llvm-project/llvm/lib/FileCheck/FileCheck.cpp", directory: "/Users/shubhamrastogi/Development/llvm-project-instr-ref/llvm-project/build-instr-ref-stage2", checksumkind: CSK_MD5, checksum: "ac1d2352ab68b965fe7993c780cf92d7")
!3 = !{}
!4 = distinct !DISubprogram(name: "operator<<", linkageName: "_ZN4llvmlsERNS_11raw_ostreamERKNS_5ErrorE", scope: !6, file: !5, line: 320, type: !7, scopeLine: 320, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !1, retainedNodes: !8)
!5 = !DIFile(filename: "llvm/include/llvm/Support/Error.h", directory: "/Users/shubhamrastogi/Development/llvm-project-instr-ref/llvm-project", checksumkind: CSK_MD5, checksum: "f166cdaeb719f8f71fbae8128cde93e4")
!6 = !DINamespace(name: "llvm", scope: null)
!7 = distinct !DISubroutineType(types: !3)
!8 = !{!9}
!9 = !DILocalVariable(name: "P", scope: !10, file: !5, line: 321, type: !11)
!10 = distinct !DILexicalBlock(scope: !4, file: !5, line: 321, column: 15)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "ErrorInfoBase", scope: !6, file: !5, line: 44, size: 64, flags: DIFlagTypePassByReference | DIFlagNonTrivial, elements: !3, vtableHolder: !12, identifier: "_ZTSN4llvm13ErrorInfoBaseE")
!13 = !DILocation(line: 321, column: 21, scope: !10)
!14 = !DILocation(line: 0, scope: !10)
