; RUN: opt -passes='module(coro-early),cgscc(coro-split),coro-cleanup' -S %s | FileCheck %s
target datalayout = "E-p:64:64"

%swift.type = type { i64 }
%swift.opaque = type opaque
%T4red215EmptyCollectionV = type opaque
%TSi = type <{ i64 }>

define ptr @f(ptr %buffer, i32 %n) {
entry:
  %id = call token @llvm.coro.id.retcon(i32 8, i32 4, ptr %buffer, ptr ptrauth (ptr @prototype, i32 2, i64 12867), ptr @allocate, ptr @deallocate)
  %hdl = call ptr @llvm.coro.begin(token %id, ptr null)
  br label %loop

loop:
  %n.val = phi i32 [ %n, %entry ], [ %inc, %resume ]
  call void @print(i32 %n.val)
  %unwind0 = call i1 (...) @llvm.coro.suspend.retcon.i1()
  br i1 %unwind0, label %cleanup, label %resume

resume:
  %inc = add i32 %n.val, 1
  br label %loop

cleanup:
  call i1 @llvm.coro.end(ptr %hdl, i1 0)
  unreachable
}

; CHECK-LABEL: define ptr @f(ptr %buffer, i32 %n)
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[T1:%.*]] = getelementptr inbounds [[FRAME_T:%.*]], ptr %buffer, i32 0, i32 0
; CHECK-NEXT:    store i32 %n, ptr [[T1]]
; CHECK-NEXT:    call void @print(i32 %n)
; CHECK-NEXT:    ret ptr ptrauth (ptr [[RESUME:@.*]], i32 2, i64 12867)

; CHECK:      define internal ptr [[RESUME]](ptr noalias noundef nonnull align 4 dereferenceable(8) %0, i1 zeroext %1) {
; CHECK:         ptrauth (ptr [[RESUME]], i32 2, i64 12867)

define ptr @g(ptr %buffer, i32 %n) {
entry:

  %id = call token @llvm.coro.id.retcon(i32 8, i32 4, ptr %buffer, ptr ptrauth (ptr @prototype, i32 2, i64 8723, ptr inttoptr(i64 1 to ptr)), ptr @allocate, ptr @deallocate)
  %hdl = call ptr @llvm.coro.begin(token %id, ptr null)
  br label %loop

loop:
  %n.val = phi i32 [ %n, %entry ], [ %inc, %resume ]
  call void @print(i32 %n.val)
  %unwind0 = call i1 (...) @llvm.coro.suspend.retcon.i1()
  br i1 %unwind0, label %cleanup, label %resume

resume:
  %inc = add i32 %n.val, 1
  br label %loop

cleanup:
  call i1 @llvm.coro.end(ptr %hdl, i1 0)
  unreachable
}

; CHECK-LABEL: define ptr @g(ptr %buffer, i32 %n)
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[T1:%.*]] = getelementptr inbounds [[FRAME_T:%.*]], ptr %buffer, i32 0, i32 0
; CHECK-NEXT:    store i32 %n, ptr [[T1]]
; CHECK-NEXT:    call void @print(i32 %n)
; CHECK-NEXT:    [[T0:%.*]] = ptrtoint ptr %buffer to i64
; CHECK-NEXT:    [[T1:%.*]] = call i64 @llvm.ptrauth.blend(i64 [[T0]], i64 8723)
; CHECK-NEXT:    [[T2:%.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr [[RESUME:@.*]] to i64), i32 2, i64 [[T1]])
; CHECK-NEXT:    [[T3:%.*]] = inttoptr i64 [[T2]] to ptr
; CHECK-NEXT:    ret ptr [[T3]]

; CHECK:      define internal ptr [[RESUME]](ptr noalias noundef nonnull align 4 dereferenceable(8) %0, i1 zeroext %1) {

; CHECK: common.ret:
; CHECK: [[RETVAL:%.*]] = phi ptr [ [[T4:%.*]], %resume ], [ null,
; CHECK: ret ptr [[RETVAL]]

; CHECK:         call void @print(i32 %inc)
; CHECK-NEXT:    [[T0:%.*]] = ptrtoint ptr %0 to i64
; CHECK-NEXT:    [[T1:%.*]] = call i64 @llvm.ptrauth.blend(i64 [[T0]], i64 8723)
; CHECK-NEXT:    [[T2:%.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr [[RESUME]] to i64), i32 2, i64 [[T1]])
; CHECK-NEXT:    [[T3:%.*]] = inttoptr i64 [[T2]] to ptr

declare noalias ptr @malloc(i64) #5
declare void @free(ptr nocapture) #5

declare token @llvm.coro.id.retcon(i32, i32, ptr, ptr, ptr, ptr)
declare ptr @llvm.coro.begin(token, ptr)
declare i1 @llvm.coro.suspend.retcon.i1(...)
declare i1 @llvm.coro.end(ptr, i1)
declare ptr @llvm.coro.prepare.retcon(ptr)

declare ptr @prototype(ptr, i1 zeroext)

declare noalias ptr @allocate(i32 %size)
declare void @deallocate(ptr %ptr)

declare void @print(i32)
