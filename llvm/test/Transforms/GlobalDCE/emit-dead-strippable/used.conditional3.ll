; RUN: opt -S -passes='lto<O2>' < %s | FileCheck %s

; Test that when performing llvm.used.conditional removals, we discover globals
; that might trigger other llvm.used.conditional liveness. See the following
; diagram of dependencies between the globals:
;
; @a -\
;      \ (conditional, see !1, satisfied)
;       \-> @b -\
;                \ (regular usage)
;                 \-> @c -\
;                          \ (conditional, see !2, satisfied)
;                           \-> @d

@a = internal unnamed_addr constant i64 42
@b = internal unnamed_addr constant ptr @c
@c = internal unnamed_addr constant i64 42
@d = internal unnamed_addr constant i64 42

; All four, and mainly @d need to stay alive:
; CHECK: @a = internal unnamed_addr constant i64 42
; CHECK: @b = internal unnamed_addr constant ptr @c
; CHECK: @c = internal unnamed_addr constant i64 42
; CHECK: @d = internal unnamed_addr constant i64 42

@llvm.used = appending global [3 x ptr] [
  ptr @a,
  ptr @b,
  ptr @d
], section "llvm.metadata"

%clr = type { i64*, i64, i64* }
@b_clr = private constant %clr { i64* @b, i64 1, i64* @a }, section "__DATA,__llvm_condlive"
@d_clr = private constant %clr { i64* @d, i64 1, i64* @c }, section "__DATA,__llvm_condlive"

@llvm.compiler.used = appending global [2 x ptr] [
    ptr @b_clr,
    ptr @d_clr
]
