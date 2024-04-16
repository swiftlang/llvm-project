; This tests that llvm.used.conditional information is
; properly considered in the thin-link liveness analysis
; which drives thin-lto dce during promotion.

; RUN: rm -rf %t && mkdir -p %t
; RUN: split-file %s %t
; RUN: opt -module-summary %t/ModuleA.ll -o %t/ModuleA.bc
; RUN: opt -module-summary %t/ModuleB.ll -o %t/ModuleB.bc
; RUN: llvm-lto2 run -save-temps %t/ModuleA.bc %t/ModuleB.bc -o %t/output.bc \
; RUN:     -r %t/ModuleA.bc,_local_live_dep,px \
; RUN:     -r %t/ModuleA.bc,_external_dead_dep,l \
; RUN:     -r %t/ModuleA.bc,_live_conditional_target_any_live_cross_link_unit,l \
; RUN:     -r %t/ModuleA.bc,_external_dep_kept_live_by_conditional_record,l \
; RUN:     -r %t/ModuleA.bc,_external_live_dep,x \
; RUN:     -r %t/ModuleA.bc,_dep_defined_outside_link_unit,x \
; RUN:     -r %t/ModuleA.bc,_dead_conditional_target_all_live,pl \
; RUN:     -r %t/ModuleA.bc,_dead_conditional_target_any_live,pl \
; RUN:     -r %t/ModuleA.bc,_live_conditional_target_all_live,pl \
; RUN:     -r %t/ModuleA.bc,_live_conditional_target_any_live,pl \
; RUN:     -r %t/ModuleB.bc,_external_dead_dep,pl \
; RUN:     -r %t/ModuleB.bc,_external_dep_kept_live_by_conditional_record,pl \
; RUN:     -r %t/ModuleB.bc,_external_live_dep,px
; RUN: llvm-dis %t/output.bc.1.5.precodegen.bc -o %t/ModuleA-Precodegen.ll
; RUN: llvm-dis %t/output.bc.2.5.precodegen.bc -o %t/ModuleB-Precodegen.ll
; RUN: cat %t/ModuleA-Precodegen.ll | FileCheck %s --check-prefix=REMOVED
; RUN: cat %t/ModuleB-Precodegen.ll | FileCheck %s --check-prefix=REMOVED
; RUN: cat %t/ModuleA-Precodegen.ll | FileCheck %s --check-prefix=LIVE-A
; RUN: cat %t/ModuleB-Precodegen.ll | FileCheck %s --check-prefix=LIVE-B

; Repeat the whole spiel with the old LTO backend
; RUN: rm -rf %t && mkdir -p %t
; RUN: mkdir %t/temps
; RUN: split-file %s %t
; RUN: opt -module-summary %t/ModuleA.ll -o %t/ModuleA.bc
; RUN: opt -module-summary %t/ModuleB.ll -o %t/ModuleB.bc
; RUN: llvm-lto -thinlto-action=run %t/ModuleA.bc %t/ModuleB.bc --thinlto-save-temps=%t/temps/ --exported-symbol=_local_live_dep --exported-symbol=_external_live_dep
; RUN: llvm-dis %t/temps/0.4.opt.bc -o %t/ModuleA-Precodegen.ll
; RUN: llvm-dis %t/temps/1.4.opt.bc -o %t/ModuleB-Precodegen.ll
; RUN: cat %t/ModuleA-Precodegen.ll | FileCheck %s --check-prefix=REMOVED
; RUN: cat %t/ModuleB-Precodegen.ll | FileCheck %s --check-prefix=REMOVED
; RUN: cat %t/ModuleA-Precodegen.ll | FileCheck %s --check-prefix=LIVE-A
; RUN: cat %t/ModuleB-Precodegen.ll | FileCheck %s --check-prefix=LIVE-B

;--- ModuleA.ll

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-unknown-ios12.0.0"

@local_dead_dep = internal constant i64 42
@external_dead_dep = external constant i64
@external_dep_kept_live_by_conditional_record = external constant i64
@local_live_dep = constant i64 42
@external_live_dep = external constant i64
@dep_defined_outside_link_unit = external constant i64

@dead_conditional_target_all_live = constant i64 42
@dead_conditional_target_any_live = constant i64 42

@live_conditional_target_all_live = constant i64 42
@live_conditional_target_any_live = constant i64 42
@live_conditional_target_any_live_cross_link_unit = constant i64 42

@__dead_conditional_target_all_live_cl = private constant { ptr, i64, ptr, ptr } { ptr @dead_conditional_target_all_live, i64 2, ptr @local_dead_dep, ptr @local_live_dep }, section "__DATA,__llvm_condlive", align 8
@__dead_conditional_target_any_live_cl = private constant { ptr, i64, ptr, ptr } { ptr @dead_conditional_target_any_live, i64 1, ptr @local_dead_dep, ptr @external_dead_dep }, section "__DATA,__llvm_condlive", align 8
@__live_conditional_target_all_live_cl = private constant { ptr, i64, ptr, ptr } { ptr @live_conditional_target_all_live, i64 2, ptr @local_live_dep, ptr @external_live_dep }, section "__DATA,__llvm_condlive", align 8
@__live_conditional_target_any_live_cl = private constant { ptr, i64, ptr, ptr, ptr } { ptr @live_conditional_target_any_live, i64 1, ptr @local_live_dep, ptr @local_dead_dep, ptr @external_dep_kept_live_by_conditional_record }, section "__DATA,__llvm_condlive", align 8
@__live_conditional_target_any_live_cross_link_unit_cl = private constant { ptr, i64, ptr, ptr } { ptr @live_conditional_target_any_live_cross_link_unit, i64 1, ptr @local_dead_dep, ptr @dep_defined_outside_link_unit }, section "__DATA,__llvm_condlive", align 8

@llvm.used = appending global [5 x ptr] [ptr @dead_conditional_target_all_live, ptr @dead_conditional_target_any_live, ptr @live_conditional_target_all_live, ptr @live_conditional_target_any_live, ptr @live_conditional_target_any_live_cross_link_unit],  section "llvm.metadata"
@llvm.compiler.used = appending global [5 x ptr] [ptr @__dead_conditional_target_all_live_cl, ptr @__dead_conditional_target_any_live_cl, ptr @__live_conditional_target_all_live_cl, ptr @__live_conditional_target_any_live_cl, ptr @__live_conditional_target_any_live_cross_link_unit_cl],  section "llvm.metadata"

; The dead conditional records and their targets should be removed
; REMOVED-NOT: dead_conditional

; Any deps mentioned in a still live conditional record, should be kept live, local_dead_dep included. The linker will clean these up.
; LIVE-A: @local_dead_dep{{.*}}42
; Since external_dep_kept_live_by_conditional_record is only used in ModuleA, thin-lto imports it into ModuleA and deletes it from ModuleB,
; but the point is, it is kept alive.
; LIVE-A: @external_dep_kept_live_by_conditional_record
; LIVE-A: @local_live_dep {{.*42}}

; The live conditional records and their targets should be kept
; LIVE-A: @live_conditional_target_all_live{{.*}}42
; LIVE-A: @live_conditional_target_any_live{{.*}}42
; LIVE-A: @live_conditional_target_any_live_cross_link_unit{{.*}}42
; LIVE-A: @__live_conditional_target_all_live_cl = private
; LIVE-A: @__live_conditional_target_any_live_cl = private
; LIVE-A: @__live_conditional_target_any_live_cross_link_unit_cl = private

;--- ModuleB.ll

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-unknown-ios12.0.0"

@external_dead_dep = constant i64 42
@external_live_dep = constant i64 42
@external_dep_kept_live_by_conditional_record = constant i64 42

; REMOVED-NOT: external_dead_dep
; LIVE-B: @external_live_dep{{.*}}42
