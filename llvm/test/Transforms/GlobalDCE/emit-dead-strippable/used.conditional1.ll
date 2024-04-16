; RUN: opt -S -passes='lto<O2>' < %s | FileCheck %s

;
; (1) a regular dead global referenced in @llvm.used is kept alive
;
define internal void @func_marked_as_used() {
  ; CHECK: @func_marked_as_used
  ret void
}

;
; (2) a dead global referenced in @llvm.used, but marked as conditionally used
; in !llvm.used.conditional where the condition is alive, is kept alive
;
define internal void @func_conditionally_used_and_live() {
  ; CHECK: @func_conditionally_used_and_live
  ret void
}

;
; (3) a dead global referenced in @llvm.used, but marked as conditionally used
; in !llvm.used.conditional where the condition is dead, is eliminated
;
define internal void @func_conditionally_used_and_dead() {
  ; CHECK-NOT: @func_conditionally_used_and_dead
  ret void
}

@condition_live = internal unnamed_addr constant i64 42
@condition_dead = internal unnamed_addr constant i64 42

%clr = type { void()*, i64, i64* }

@llvm.used = appending global [4 x i8*] [
   i8* bitcast (void ()* @func_marked_as_used to i8*),
   i8* bitcast (i64* @condition_live to i8*),
   i8* bitcast (void ()* @func_conditionally_used_and_live to i8*),
   i8* bitcast (void ()* @func_conditionally_used_and_dead to i8*)
], section "llvm.metadata"

@llvm.compiler.used = appending global [2 x i8*] [
    i8* bitcast (%clr* @conditionally_used_and_live_clr to i8*),
    i8* bitcast (%clr* @conditionally_used_and_dead_clr to i8*)
]

@conditionally_used_and_live_clr = private constant %clr { void()* @func_conditionally_used_and_live, i64 1, i64* @condition_live }, section "__DATA,__llvm_condlive"
@conditionally_used_and_dead_clr = private constant %clr { void()* @func_conditionally_used_and_dead, i64 1, i64* @condition_dead }, section "__DATA,__llvm_condlive"
