; RUN: llc -mtriple=x86_64-unknown-windows-msvc %s -o - | FileCheck %s
; RUN: llc -mtriple=i686-windows-msvc %s -o - | FileCheck %s --check-prefix=CHECK-32

define void @simple(i8* swiftasync %ctx) "frame-pointer"="all" {
; CHECK-LABEL: simple:
; CHECK: btsq    $60, %rbp
; CHECK: pushq   %rbp
; CHECK: pushq   %r14
; CHECK: leaq    8(%rsp), %rbp

; [...]

; CHECK: addq    $16, %rsp
; CHECK: popq    %rbp
; CHECK: btrq    $60, %rbp
; CHECK: retq

; CHECK-32-LABEL: simple:
; CHECK-32: movl 8(%ebp), [[TMP:%.*]]
; CHECK-32: movl [[TMP]], {{.*}}(%ebp)

  ret void
}

define void @more_csrs(i8* swiftasync %ctx) "frame-pointer"="all" {
; CHECK-LABEL: more_csrs:
; CHECK: btsq    $60, %rbp
; CHECK: pushq   %rbp
; CHECK: .seh_pushreg %rbp
; CHECK: pushq   %r14
; CHECK: .seh_pushreg %r14
; CHECK: leaq    8(%rsp), %rbp
; CHECK: subq    $8, %rsp
; CHECK: pushq   %r15
; CHECK: .seh_pushreg %r15

; [...]

; CHECK: popq    %r15
; CHECK: addq    $16, %rsp
; CHECK: popq    %rbp
; CHECK: btrq    $60, %rbp
; CHECK: retq
  call void asm sideeffect "", "~{r15}"()
  ret void
}

define void @locals(i8* swiftasync %ctx) "frame-pointer"="all" {
; CHECK-LABEL: locals:
; CHECK: btsq    $60, %rbp
; CHECK: pushq   %rbp
; CHECK: .seh_pushreg %rbp
; CHECK: pushq   %r14
; CHECK: .seh_pushreg %r14
; CHECK: leaq    8(%rsp), %rbp
; CHECK: subq    $88, %rsp

; CHECK: leaq    -48(%rbp), %rcx
; CHECK: callq   bar

; CHECK: addq    $80, %rsp
; CHECK: addq    $16, %rsp
; CHECK: popq    %rbp
; CHECK: btrq    $60, %rbp
; CHECK: retq

  %var = alloca i32, i32 10
  call void @bar(i32* %var)
  ret void
}

define void @use_input_context(i8* swiftasync %ctx, i8** %ptr) "frame-pointer"="all" {
; CHECK-LABEL: use_input_context:
; CHECK: movq    %r14, (%rcx)

  store i8* %ctx, i8** %ptr
  ret void
}

define i8** @context_in_func() "frame-pointer"="non-leaf" {
; CHECK-LABEL: context_in_func:
; CHECK: leaq    -8(%rbp), %rax

; CHECK-32-LABEL: context_in_func
; CHECK-32: movl %esp, %eax

  %ptr = call i8** @llvm.swift.async.context.addr()
  ret i8** %ptr
}

define void @write_frame_context(i8* swiftasync %ctx, i8* %newctx) "frame-pointer"="non-leaf" {
; CHECK-LABEL: write_frame_context:
; CHECK: movq    %rbp, [[TMP:%.*]]
; CHECK: subq    $8, [[TMP]]
; CHECK: movq    %rcx, ([[TMP]])

  %ptr = call i8** @llvm.swift.async.context.addr()
  store i8* %newctx, i8** %ptr
  ret void
}

define void @simple_fp_elim(i8* swiftasync %ctx) "frame-pointer"="non-leaf" {
; CHECK-LABEL: simple_fp_elim:
; CHECK-NOT: btsq

  ret void
}

declare void @bar(i32*)
declare i8** @llvm.swift.async.context.addr()
