; RUN: opt -S -passes='lto<O2>' < %s | FileCheck %s

;
; (1) a dead global referenced in @llvm.used, but marked as conditionally used
; by both @condition_live and @condition_dead, with an *ANY* type, is alive
;
define internal void @func_conditionally_by_two_conditions_any() {
  ; CHECK: @func_conditionally_by_two_conditions_any
  ret void
}

;
; (2) a dead global referenced in @llvm.used, but marked as conditionally used
; by both @condition_live and @condition_dead, with an *ALL* type, is removed
;
define internal void @func_conditionally_by_two_conditions_all() {
  ; CHECK-NOT: @func_conditionally_by_two_conditions_all
  ret void
}

@condition_live = internal unnamed_addr constant i64 42
@condition_dead = internal unnamed_addr constant i64 42

%clr = type { void()*, i64, i64*, i64* }
@conditionally_used_by_two_conditions_any_clr = private constant %clr { void()* @func_conditionally_by_two_conditions_any, i64 1, i64* @condition_live, i64* @condition_dead }, section "__DATA,__llvm_condlive"
@conditionally_used_by_two_conditions_all_clr = private constant %clr { void()* @func_conditionally_by_two_conditions_all, i64 2, i64* @condition_live, i64* @condition_dead }, section "__DATA,__llvm_condlive"

@llvm.compiler.used = appending global [2 x i8*] [
    i8* bitcast (%clr* @conditionally_used_by_two_conditions_all_clr to i8*),
    i8* bitcast (%clr* @conditionally_used_by_two_conditions_any_clr to i8*)
]

@llvm.used = appending global [3 x i8*] [
   i8* bitcast (i64* @condition_live to i8*),
   i8* bitcast (void ()* @func_conditionally_by_two_conditions_any to i8*),
   i8* bitcast (void ()* @func_conditionally_by_two_conditions_all to i8*)
], section "llvm.metadata"
