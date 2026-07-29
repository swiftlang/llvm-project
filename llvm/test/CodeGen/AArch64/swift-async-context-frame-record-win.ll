; RUN: llc -mtriple aarch64-unknown-windows-msvc -O2 %s -o - | FileCheck %s

; The swift async context slot is allocated directly below the frame record. If
; it is instead allocated above it, MachineFrameInfo's view of the callee-save
; area disagrees with the prologue by 8 bytes, leaving a hole that PEI's stack
; slot scavenger hands to a live local -- landing it on the saved caller x29.
;
; Check that x29 is only ever used as a base for loads here, never stored
; through at the frame record itself.

; CHECK-LABEL: test:
; CHECK:       add     x29, sp, #[[FPOFF:[0-9]+]]
; CHECK-NOT:   str     {{[wx][0-9]+}}, [x29]
; CHECK-NOT:   str     {{[wx][0-9]+}}, [x29, #8]
; CHECK-NOT:   stur    {{[wx][0-9]+}}, [x29]
; CHECK-NOT:   stp     {{[wx][0-9]+}}, {{[wx][0-9]+}}, [x29]
; CHECK:       ldp     x29, x30, [sp, #[[FPOFF]]]

declare ptr @llvm.swift.async.context.addr() nounwind
declare swiftcc void @swift_task_dealloc()

define swifttailcc void @test(ptr %0, ptr %1, ptr %2, ptr %3, ptr %4, ptr %5,
                              ptr %6, ptr %7, ptr %8, ptr %9) {
entryresume.0:
  %10 = tail call ptr @llvm.swift.async.context.addr()
  %.reload71 = load ptr, ptr null, align 8
  call swiftcc void @swift_task_dealloc()
  %Destroy24 = load ptr, ptr %0, align 8
  tail call void %Destroy24(ptr %.reload71, ptr %9)
  %Destroy25 = load ptr, ptr %6, align 8
  tail call void %Destroy25(ptr %3, ptr null)
  %Destroy26 = load ptr, ptr %8, align 8
  tail call void %Destroy26(ptr %2, ptr %7)
  %Destroy27 = load ptr, ptr %4, align 8
  tail call void %Destroy27(ptr %1, ptr %5)
  ret void
}
