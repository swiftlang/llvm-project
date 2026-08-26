#define ASYNC_CONTEXT_REG x22

  .text

//--------------------------------------
// enter_swift(): plain C-callable entry point, since the mangled names below
// cannot be spelled in C.
//--------------------------------------
  .globl _enter_swift
  .p2align 2
_enter_swift:
  .cfi_startproc
  stp x29, x30, [sp, #-16]!
  .cfi_def_cfa_offset 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  mov x29, sp
  .cfi_def_cfa w29, 16
  bl _$s1a6calleryyF
  ldp x29, x30, [sp], #16
  ret
  .cfi_endproc

//--------------------------------------
// a.caller() -> (): sync.  Primes the async context register, then calls
// waitComplete.  This is the innermost frame that goes missing.
//--------------------------------------
  .globl _$s1a6calleryyF
  .p2align 2
_$s1a6calleryyF:
  .cfi_startproc
  stp x29, x30, [sp, #-16]!
  .cfi_def_cfa_offset 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  mov x29, sp
  .cfi_def_cfa w29, 16
  // The buggy path reads this register to build its CFA.  Point it at readable
  // memory so it produces a plan instead of bailing out early.
  adrp ASYNC_CONTEXT_REG, _async_context@PAGE
  add ASYNC_CONTEXT_REG, ASYNC_CONTEXT_REG, _async_context@PAGEOFF
  bl _$s1a12waitCompleteySbSbF
  ldp x29, x30, [sp], #16
  ret
  .cfi_endproc

//--------------------------------------
// a.reportAndDie() -> Swift.Never: faults, giving the test a deterministic stop.
//--------------------------------------
  .globl _$s1a12reportAndDies5NeverOyF
  .p2align 2
_$s1a12reportAndDies5NeverOyF:
  .cfi_startproc
  mov w8, #8
  mov w9, #1
  str x9, [x8]                  // EXC_BAD_ACCESS / SIGSEGV
Lspin:
  b Lspin
  .cfi_endproc

//--------------------------------------
// a.doPanic(Swift.UInt32) -> (): also ends in a `bl`, which is what the Swift
// optimizer emits for a body that is a single call to a Never function.
//--------------------------------------
  .globl _$s1a7doPanicyys6UInt32VF
  .p2align 2
_$s1a7doPanicyys6UInt32VF:
  .cfi_startproc
  stp x29, x30, [sp, #-16]!
  .cfi_def_cfa_offset 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  mov x29, sp
  .cfi_def_cfa w29, 16
  bl _$s1a12reportAndDies5NeverOyF
  .cfi_endproc

//--------------------------------------
// a.waitComplete(Swift.Bool) -> Swift.Bool: sync, and the `bl` is its last
// instruction.  The frame under test.
//--------------------------------------
  .globl _$s1a12waitCompleteySbSbF
  .p2align 2
_$s1a12waitCompleteySbSbF:
  .cfi_startproc
  stp x29, x30, [sp, #-16]!
  .cfi_def_cfa_offset 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  mov x29, sp
  .cfi_def_cfa w29, 16
  bl _$s1a7doPanicyys6UInt32VF
  .cfi_endproc

//--------------------------------------
// a.lynxWrite() async -> (): the decoy.  Must be laid out immediately after
// waitComplete; the test asserts the adjacency.  Never executed.
//--------------------------------------
  .globl _$s1a9lynxWriteyyYaF
  .p2align 2
_$s1a9lynxWriteyyYaF:
  .cfi_startproc
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  ldp x29, x30, [sp], #16
  ret
  .cfi_endproc

  .data
  .p2align 3
// A minimal async context: { parent context, resume pc }.  Only needs to be
// readable for the buggy path to build a plan from it.
_async_context:
  .quad 0
  .quad 0
