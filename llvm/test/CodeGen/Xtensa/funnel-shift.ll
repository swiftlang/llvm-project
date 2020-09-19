; RUN: llc -O1 -mtriple=xtensa -mcpu=esp32 %s -o - | FileCheck %s

define dso_local i32 @test_fshr(i32 %value, i32 %shift) nounwind {
; CHECK-LABEL: @test_fshr
; CHECK: ssr a3
; CHECK: src a2, a2, a2
entry:
  %0 = tail call i32 @llvm.fshr.i32(i32 %value, i32 %value, i32 %shift)
  ret i32 %0
}

define dso_local i32 @test_fshl(i32 %value, i32 %shift) nounwind {
; CHECK-LABEL: @test_fshl
; CHECK: ssl a3
; CHECK: src a2, a2, a2
entry:
  %0 = tail call i32 @llvm.fshl.i32(i32 %value, i32 %value, i32 %shift)
  ret i32 %0
}

declare i32 @llvm.fshr.i32(i32, i32, i32) nounwind
declare i32 @llvm.fshl.i32(i32, i32, i32) nounwind
