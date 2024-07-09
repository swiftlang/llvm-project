; RUN: llc -mtriple=arm64e-apple-ios %s -o - | FileCheck %s

@var1 = external dso_local global [1024 x i8], align 8
@var2 = external dso_local global [1024 x i8], align 8

define void @func1(i64 %offset) #0 {
; CHECK-LABEL: func1:
; CHECK-NOT: brk
; CHECK: b _OUTLINED_FUNCTION_0
; CHECK: pacibsp
  br i1 undef, label %t4, label %t3

t3:
  call void @foo()
  ret void

t4:

  %gep = getelementptr inbounds [1024 x i8], [1024 x i8]* @var1, i64 %offset
  tail call fastcc void @take_pointer([1024 x i8]* %gep)
  ret void
}

; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK: brk
; CHECK: b _take_pointer

define void @func2(i64 %offset) #0 {
  br i1 undef, label %t4, label %t3

t3:
  call void @foo()
  ret void

t4:
  %gep = getelementptr inbounds [1024 x i8], [1024 x i8]* @var2,  i64 %offset
  tail call fastcc void @take_pointer([1024 x i8]* %gep) #7
  ret void
}

declare void @foo()

declare void @take_pointer([1024 x i8]*)

attributes #0 = { minsize "ptrauth-auth-traps" "ptrauth-returns" }
