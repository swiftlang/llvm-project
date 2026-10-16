; RUN: opt < %s -aarch64-stack-tagging -S -o - | FileCheck %s

; Test that Swift preinit memsets use untagged pointers and occur before tagging.
; In Swift codegen, LLDB uses preinit memsets to zero-initialize memory before 
; actual initialization, and these memsets must use the untagged (base) pointer
; to avoid a tag fault.

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-darwin"

%TSi = type <{ i64 }>
%TSd = type <{ double }>

; CHECK-LABEL: define swiftcc void @test_swift_preinit_memset
; Function Attrs: sanitize_memtag
define swiftcc void @test_swift_preinit_memset() #0 {
entry:
  %x = alloca %TSi, align 8
  call void @llvm.memset.p0.i64(ptr align 8 %x, i8 0, i64 8, i1 false), !Swift.isSwiftLLDBpreinit !17

  ; CHECK: %x = alloca { %TSi, [8 x i8] }, align 16
  ; Swift preinit memsets should use the untagged pointer and happen before any tagging intrinsics
  ; CHECK-NEXT: call void @llvm.memset.p0.i64(ptr align 8 %x, i8 0, i64 8, i1 false){{.*}}!Swift.isSwiftLLDBpreinit
  ; CHECK-NEXT: [[XTAGGED:%[^ ]+]] = call ptr @llvm.aarch64.tagp.{{.*}}(ptr %x

  %y = alloca %TSd, align 8
  call void @llvm.memset.p0.i64(ptr align 8 %y, i8 0, i64 8, i1 false), !Swift.isSwiftLLDBpreinit !17
  ; CHECK: %y = alloca { %TSd, [8 x i8] }, align 16
  ; CHECK-NEXT: call void @llvm.memset.p0.i64(ptr align 8 %y, i8 0, i64 8, i1 false){{.*}}!Swift.isSwiftLLDBpreinit
  ; CHECK-NEXT: [[YTAGGED:%[^ ]+]] = call ptr @llvm.aarch64.tagp.{{.*}}(ptr %y

  call void @llvm.lifetime.start.p0(i64 8, ptr %x)
  ; Lifetime intrinsics use the untagged pointers
  ; CHECK: call void @llvm.lifetime.start.p0(ptr %x)

  %x._value = getelementptr inbounds nuw %TSi, ptr %x, i32 0, i32 0
  ; CHECK: %x._value = getelementptr inbounds nuw %TSi, ptr [[XTAGGED]], i32 0, i32 0
  store i64 0, ptr %x._value, align 8

  call void @llvm.lifetime.start.p0(i64 8, ptr %y)
  ; CHECK: call void @llvm.lifetime.start.p0(ptr %y)

  %y._value = getelementptr inbounds nuw %TSd, ptr %y, i32 0, i32 0
  ; CHECK: %y._value = getelementptr inbounds nuw %TSd, ptr [[YTAGGED]], i32 0, i32 0
  store double 42.000000e+00, ptr %y._value, align 8

  call void asm sideeffect "nop", ""()

  call void @llvm.lifetime.end.p0(i64 8, ptr %y)
  call void @llvm.lifetime.end.p0(i64 8, ptr %x)

  ; CHECK: call void @llvm.lifetime.end.p0(ptr %y)
  ; CHECK: call void @llvm.lifetime.end.p0(ptr %x)

  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

attributes #0 = { sanitize_memtag }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }

!17 = !{}
