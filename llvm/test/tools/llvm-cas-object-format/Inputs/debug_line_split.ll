; ModuleID = './test.c'
source_filename = "./test.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @foo(i32 noundef %n) #0 !dbg !9 {
entry:
  %n.addr = alloca i32, align 4
  store i32 %n, i32* %n.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %n.addr, metadata !15, metadata !DIExpression()), !dbg !16
  %0 = load i32, i32* %n.addr, align 4, !dbg !17
  %div = sdiv i32 %0, 2, !dbg !18
  ret i32 %div, !dbg !19
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @bar(i32 noundef %x) #0 !dbg !20 {
entry:
  %x.addr = alloca i32, align 4
  store i32 %x, i32* %x.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %x.addr, metadata !21, metadata !DIExpression()), !dbg !22
  %0 = load i32, i32* %x.addr, align 4, !dbg !23
  %call = call i32 @foo(i32 noundef %0), !dbg !24
  %1 = load i32, i32* %x.addr, align 4, !dbg !25
  %call1 = call i32 @foo(i32 noundef %1), !dbg !26
  %mul = mul nsw i32 %call, %call1, !dbg !27
  ret i32 %mul, !dbg !28
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main(i32 noundef %argc, i8** noundef %argv) #0 !dbg !29 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  store i32 0, i32* %retval, align 4
  store i32 %argc, i32* %argc.addr, align 4
  call void @llvm.dbg.declare(metadata i32* %argc.addr, metadata !35, metadata !DIExpression()), !dbg !36
  store i8** %argv, i8*** %argv.addr, align 8
  call void @llvm.dbg.declare(metadata i8*** %argv.addr, metadata !37, metadata !DIExpression()), !dbg !38
  %0 = load i32, i32* %argc.addr, align 4, !dbg !39
  %call = call i32 @bar(i32 noundef %0), !dbg !40
  ret i32 %call, !dbg !41
}

attributes #0 = { noinline nounwind optnone ssp uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 15.0.0 (git@github.com:apple/llvm-project.git d70b109980a620c1041801e0eef829da0ea7aef6)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None, sysroot: "/", casFriendly: DebugAbbrev)
!1 = !DIFile(filename: "test.c", directory: "/Users/shubham/Development/testCASSymbols")
!2 = !{i32 7, !"Dwarf Version", i32 4}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"PIC Level", i32 2}
!6 = !{i32 7, !"uwtable", i32 2}
!7 = !{i32 7, !"frame-pointer", i32 2}
!8 = !{!"clang version 15.0.0 (git@github.com:apple/llvm-project.git)"}
!9 = distinct !DISubprogram(name: "foo", scope: !10, file: !10, line: 1, type: !11, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !14)
!10 = !DIFile(filename: "./test.c", directory: "/Users/shubham/Development/testCASSymbols")
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !13}
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !{}
!15 = !DILocalVariable(name: "n", arg: 1, scope: !9, file: !10, line: 1, type: !13)
!16 = !DILocation(line: 1, column: 13, scope: !9)
!17 = !DILocation(line: 2, column: 9, scope: !9)
!18 = !DILocation(line: 2, column: 10, scope: !9)
!19 = !DILocation(line: 2, column: 2, scope: !9)
!20 = distinct !DISubprogram(name: "bar", scope: !10, file: !10, line: 5, type: !11, scopeLine: 5, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !14)
!21 = !DILocalVariable(name: "x", arg: 1, scope: !20, file: !10, line: 5, type: !13)
!22 = !DILocation(line: 5, column: 13, scope: !20)
!23 = !DILocation(line: 6, column: 13, scope: !20)
!24 = !DILocation(line: 6, column: 9, scope: !20)
!25 = !DILocation(line: 6, column: 22, scope: !20)
!26 = !DILocation(line: 6, column: 18, scope: !20)
!27 = !DILocation(line: 6, column: 16, scope: !20)
!28 = !DILocation(line: 6, column: 2, scope: !20)
!29 = distinct !DISubprogram(name: "main", scope: !10, file: !10, line: 9, type: !30, scopeLine: 9, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !14)
!30 = !DISubroutineType(types: !31)
!31 = !{!13, !13, !32}
!32 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !33, size: 64)
!33 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !34, size: 64)
!34 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!35 = !DILocalVariable(name: "argc", arg: 1, scope: !29, file: !10, line: 9, type: !13)
!36 = !DILocation(line: 9, column: 14, scope: !29)
!37 = !DILocalVariable(name: "argv", arg: 2, scope: !29, file: !10, line: 9, type: !32)
!38 = !DILocation(line: 9, column: 27, scope: !29)
!39 = !DILocation(line: 10, column: 13, scope: !29)
!40 = !DILocation(line: 10, column: 9, scope: !29)
!41 = !DILocation(line: 10, column: 2, scope: !29)
