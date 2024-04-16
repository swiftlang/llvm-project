; RUN: opt -S -passes='lto<O2>' < %s | FileCheck %s

; Test !llvm.used.conditional with circular dependencies

@globalA = internal unnamed_addr constant i8* bitcast (i8** @globalB to i8*)
@globalB = internal unnamed_addr constant i8* bitcast (i8** @globalA to i8*)

; CHECK-NOT: @globalA
; CHECK-NOT: @globalB

@llvm.used = appending global [2 x i8*] [
  i8* bitcast (i8** @globalA to i8*),
  i8* bitcast (i8** @globalB to i8*)
], section "llvm.metadata"

%clr = type { i8**, i64, i8** }
@a_clr = private constant %clr { i8** @globalA, i64 1, i8** @globalB }, section "__DATA,__llvm_condlive"
@b_clr = private constant %clr { i8** @globalB, i64 1, i8** @globalA }, section "__DATA,__llvm_condlive"

@llvm.compiler.used = appending global [2 x ptr] [
    ptr @a_clr,
    ptr @b_clr
], section "llvm.metadata"
