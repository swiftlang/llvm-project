; RUN: opt < %s -passes='cgscc(coro-split)' -S | FileCheck %s

target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"
target triple = "arm64-apple-macosx14.0.0"

%swift.async_func_pointer = type <{ i32, i32 }>
%GeoPayload = type <{ [48 x i8], [1 x i8] }>

@my_async_function_fp = global %swift.async_func_pointer <{ i32 trunc (i64 sub (i64 ptrtoint (ptr @my_async_function to i64), i64 ptrtoint (ptr @my_async_function_fp to i64)) to i32), i32 16 }>

; Checks that the frame is built packed and correctly sized when a spilled
; value's real alignment (32) exceeds the frame's capped alignment (16).
; CHECK: %my_async_function.Frame = type <{ %GeoPayload, [7 x i8], { ptr, double, ptr }, <4 x double> }>
; CHECK: @my_async_function_fp = global %swift.async_func_pointer <{ i32 {{.*}}, i32 112 }>

define swifttailcc void @my_async_function(ptr %0, <4 x double> %1, { ptr, double, ptr } %2) presplitcoroutine {
entry:
  %3 = alloca %GeoPayload, align 16
  %4 = tail call token @llvm.coro.id.async(i32 0, i32 16, i32 0, ptr @my_async_function_fp)
  %5 = tail call ptr @llvm.coro.begin(token %4, ptr null)
  %6 = tail call ptr @llvm.coro.async.resume()
  store ptr %6, ptr %0, align 8
  %7 = call { ptr, ptr } (i32, ptr, ptr, ...) @llvm.coro.suspend.async.sl_p0p0s(i32 0, ptr %6, ptr @__swift_async_resume_project_context, ptr @my_async_function.resume.0, ptr null, ptr %3, ptr null, i64 0)
  %vec.lo = shufflevector <4 x double> %1, <4 x double> zeroinitializer, <2 x i32> <i32 0, i32 1>
  %pair.mid = extractvalue { ptr, double, ptr } %2, 1
  unreachable
}

declare token @llvm.coro.id.async(i32, i32, i32, ptr)
declare ptr @llvm.coro.begin(token, ptr writeonly)
declare ptr @llvm.coro.async.resume()

define ptr @__swift_async_resume_project_context(ptr %0) {
entry:
  ret ptr %0
}

declare swifttailcc void @my_async_function.resume.0()
declare { ptr, ptr } @llvm.coro.suspend.async.sl_p0p0s(i32, ptr, ptr, ...)

