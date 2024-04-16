; This tests that conditionally live record information
; added to the module summary is preserved across
; assembly and disassembly

; RUN: rm -rf %t && mkdir -p %t
; RUN: opt -module-summary %s -o %t/ModuleWithSummary.bc
; RUN: llvm-dis %t/ModuleWithSummary.bc -o %t/ModuleWithSummary.ll
; RUN: cat %t/ModuleWithSummary.ll | FileCheck %s
; RUN: llvm-as %t/ModuleWithSummary.ll -o %t/ReassembledModuleWithSummary.bc
; RUN: llvm-dis %t/ReassembledModuleWithSummary.bc -o %t/ReassembledModuleWithSummary.ll
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s
; Dependency order is not guaranteed to be stable, but all dependencies should be included
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s --check-prefix=ANY-FOO
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s --check-prefix=ANY-BAR
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s --check-prefix=ANY-BAZ
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s --check-prefix=ALL-FOO
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s --check-prefix=ALL-BAR
; RUN: cat %t/ReassembledModuleWithSummary.ll | FileCheck %s --check-prefix=ALL-BAZ

@conditional_gv_all_live = internal constant i64 42
@conditional_gv_any_live = internal constant i64 42
@conditional_gv_format = internal constant i64 42

@dep_foo = external constant i64
@dep_bar = constant i64 42
@dep_baz = constant [10 x i64] [i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 7, i64 8, i64 9, i64 10]

@__conditional_gv_all_live_cl = internal constant { ptr, i64, ptr, ptr, ptr } { ptr @conditional_gv_all_live, i64 3, ptr @dep_bar, ptr @dep_baz, ptr @dep_foo }, section "__DATA,__llvm_condlive", align 8
@__conditional_gv_any_live_cl = internal constant { ptr, i64, ptr, ptr, ptr } { ptr @conditional_gv_any_live, i64 1, ptr @dep_bar, ptr @dep_baz, ptr @dep_foo }, section "__DATA,__llvm_condlive", align 8
@__conditional_gv_format_cl = internal constant { ptr, i64, ptr } { ptr @conditional_gv_format, i64 1, ptr @dep_foo }, section "__DATA,__llvm_condlive", align 8

@llvm.used = appending global [3 x ptr] [ptr @__conditional_gv_all_live_cl, ptr @__conditional_gv_any_live_cl, ptr @__conditional_gv_format_cl]

; CHECK-DAG: ^{{[0-9]+}} = condLiveRecord: (record: {{[0-9]+}} ("__conditional_gv_any_live_cl"), target: {{[0-9]+}} ("conditional_gv_any_live"), requiredLive: 1, deps:
; CHECK-DAG: ^{{[0-9]+}} = condLiveRecord: (record: {{[0-9]+}} ("__conditional_gv_all_live_cl"), target: {{[0-9]+}} ("conditional_gv_all_live"), requiredLive: 3, deps:
; CHECK-DAG: ^{{[0-9]+}} = condLiveRecord: (record: {{[0-9]+}} ("__conditional_gv_format_cl"), target: {{[0-9]+}} ("conditional_gv_format"), requiredLive: 1, deps: {{[0-9]+}} ("dep_foo"))

; ANY-FOO: condLiveRecord: {{.*}}target: {{[0-9]+}} ("conditional_gv_any_live"),{{.*}} deps:{{.*[0-9]+}} ("dep_foo")
; ANY-BAR: condLiveRecord: {{.*}}target: {{[0-9]+}} ("conditional_gv_any_live"),{{.*}} deps:{{.*[0-9]+}} ("dep_bar")
; ANY-BAZ: condLiveRecord: {{.*}}target: {{[0-9]+}} ("conditional_gv_any_live"),{{.*}} deps:{{.*[0-9]+}} ("dep_baz")
; ALL-FOO: condLiveRecord: {{.*}}target: {{[0-9]+}} ("conditional_gv_all_live"),{{.*}} deps:{{.*[0-9]+}} ("dep_foo")
; ALL-BAR: condLiveRecord: {{.*}}target: {{[0-9]+}} ("conditional_gv_all_live"),{{.*}} deps:{{.*[0-9]+}} ("dep_bar")
; ALL-BAZ: condLiveRecord: {{.*}}target: {{[0-9]+}} ("conditional_gv_all_live"),{{.*}} deps:{{.*[0-9]+}} ("dep_baz")
