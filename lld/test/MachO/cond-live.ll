; REQUIRES: aarch64

; RUN: mkdir -p %t && rm -rf %t/*
; RUN: split-file %s %t

; RUN: llc -filetype=obj %t/lib.ll -o %t/lib.o
; RUN: %lld -dylib -arch arm64 %t/lib.o -o %t/libLib.dylib

; RUN: llc -filetype=obj %t/main.ll -o %t/main.o

; RUN: %lld -dylib -arch arm64 %t/main.o -L %t -lLib -o %t-no-dead.out
; RUN: llvm-nm %t-no-dead.out | FileCheck %s --check-prefix=NO_DEAD

; RUN: %lld -dylib -arch arm64 %t/main.o -dead_strip -L %t -lLib -o %t/main
; RUN: llvm-nm %t/main | FileCheck %s

; RUN: %lld -dylib -arch arm64 %t/main.o -dead_strip -L %t -lLibTbd -o %t/main-tbd
; RUN: llvm-nm %t/main-tbd | FileCheck %s

;--- main.ll

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx14.0.0"

%clt2 = type { ptr, i64, ptr, ptr }

; 1. No dead-strip case. Every symbol is alive
; NO_DEAD: _dep_1
; NO_DEAD: _dep_2
; NO_DEAD: _dep_3
; NO_DEAD: _dep_4
; NO_DEAD: _dep_5
; NO_DEAD: _dep_6
; NO_DEAD: _target1
; NO_DEAD: _target2
; NO_DEAD: _target3

; 2. Dead-strip case with conditional live entries.
;  a. conditional live entry for target1
;    - target1 depends on dep_1 and dep_2 with the required live count = 1.
;    - As dep_1 is alive (accessed in main), target1 becomes alive.
;  b. conditional live entry for target 2
;    - target2 depends on dep_3 and dep_4 with the required live count = 2.
;    - Although dep_3 is alive (accessed in main), target2 is still dead.

; CHECK: _dep_1
; CHECK: _dep_3
; CHECK-NOT: _dep_5
; CHECK-NOT: _dep_6
; CHECK-NOT: _ce_target1
; CHECK-NOT: _ce_target2
; CHECK-NOT: _ce_target3
; CHECK: _target1
; CHECK-NOT: _target2
; CHECK: _target3

@target1 = internal constant i64 1
@dep_1 = internal constant i64 2
@dep_2 = internal constant i64 3
@ce_target1 = private constant %clt2 { ptr @target1, i64 1, ptr @dep_1, ptr @dep_2 }, section "__DATA,__llvm_condlive"

@target2 = internal constant i64 4
@dep_3 = internal constant i64 5
@dep_4 = internal constant i64 6
@ce_target2 = private constant %clt2 { ptr @target2, i64 2, ptr @dep_3, ptr @dep_4 }, section "__DATA,__llvm_condlive"

@target3 = internal constant i64 7
@dep_5 = internal constant i64 8
@dep_6 = external constant i64
@ce_target3 = private constant %clt2 { ptr @target3, i64 1, ptr @dep_5, ptr @dep_6 }, section "__DATA,__llvm_condlive"

define i64 @main() {
  %1 = load i64, ptr @dep_1
  %2 = load i64, ptr @dep_3
  %3 = add i64 %1, %2
  ret i64 %3
}

@llvm.used = appending global [3 x ptr] [ptr @target1, ptr @target2, ptr @target3], section "llvm.metadata"
@llvm.compiler.used = appending global [3 x ptr] [ptr @ce_target1, ptr @ce_target2, ptr @ce_target3], section "llvm.metadata"

;--- lib.ll

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx14.0.0"

@dep_6 = constant i64 9

;--- libLibTbd.tbd
--- !tapi-tbd
tbd-version:     4
targets:         [ arm64-macos ]
install-name:    '%t/libLibTbd.dylib'
exports:
  - targets:         [ arm64-macos ]
    symbols:         [ '_dep_6' ]
...
