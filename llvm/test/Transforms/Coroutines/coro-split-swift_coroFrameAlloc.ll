; Tests that coro-split pass splits the coroutine into f, f.resume and f.destroy
; RUN: opt < %s -passes='cgscc(coro-split),simplifycfg,early-cse' -S | FileCheck %s

%test_simple.Frame = type { i64, i64, i64, i64, i64 }

define swiftcc ptr @test_simple(ptr noalias dereferenceable(32) %0) #0 {
entry:
  %call.aggresult = alloca <{ i64, i64, i64, i64, i64 }>, align 8
  %1 = alloca <{ i64, i64, i64, i64, i64 }>, align 8
  %2 = call ptr @swift_coroFrameAlloc(i64 40, i64 38223)
  store ptr %2, ptr %0, align 8
  call swiftcc void @marker(i32 1000)
  call void @llvm.lifetime.start.p0(i64 40, ptr %call.aggresult)
  call swiftcc void @val(ptr noalias nocapture sret(<{ i64, i64, i64, i64, i64 }>) %call.aggresult)
  %call.aggresult.elt = getelementptr inbounds <{ i64, i64, i64, i64, i64 }>, ptr %call.aggresult, i32 0, i32 0
  %3 = load i64, ptr %call.aggresult.elt, align 8
  %.spill.addr = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 0
  store i64 %3, ptr %.spill.addr, align 8
  %call.aggresult.elt1 = getelementptr inbounds <{ i64, i64, i64, i64, i64 }>, ptr %call.aggresult, i32 0, i32 1
  %4 = load i64, ptr %call.aggresult.elt1, align 8
  %.spill.addr9 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 1
  store i64 %4, ptr %.spill.addr9, align 8
  %call.aggresult.elt2 = getelementptr inbounds <{ i64, i64, i64, i64, i64 }>, ptr %call.aggresult, i32 0, i32 2
  %5 = load i64, ptr %call.aggresult.elt2, align 8
  %.spill.addr12 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 2
  store i64 %5, ptr %.spill.addr12, align 8
  %call.aggresult.elt3 = getelementptr inbounds <{ i64, i64, i64, i64, i64 }>, ptr %call.aggresult, i32 0, i32 3
  %6 = load i64, ptr %call.aggresult.elt3, align 8
  %.spill.addr15 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 3
  store i64 %6, ptr %.spill.addr15, align 8
  %call.aggresult.elt4 = getelementptr inbounds <{ i64, i64, i64, i64, i64 }>, ptr %call.aggresult, i32 0, i32 4
  %7 = load i64, ptr %call.aggresult.elt4, align 8
  %.spill.addr18 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 4
  store i64 %7, ptr %.spill.addr18, align 8
  call void @llvm.lifetime.end.p0(i64 40, ptr %call.aggresult)
  ret ptr @test_simple.resume.0
}

define internal swiftcc void @test_simple.resume.0(ptr noalias noundef nonnull align 8 dereferenceable(32) %0, i1 %1) #1 {
entryresume.0:
  %2 = load ptr, ptr %0, align 8
  %3 = alloca <{ i64, i64, i64, i64, i64 }>, align 8
  %call.aggresult = alloca <{ i64, i64, i64, i64, i64 }>, align 8
  br i1 %1, label %5, label %4

4:                                                ; preds = %entryresume.0
  %.reload.addr19 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 4
  %.reload20 = load i64, ptr %.reload.addr19, align 8
  %.reload.addr16 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 3
  %.reload17 = load i64, ptr %.reload.addr16, align 8
  %.reload.addr13 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 2
  %.reload14 = load i64, ptr %.reload.addr13, align 8
  %.reload.addr10 = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 1
  %.reload11 = load i64, ptr %.reload.addr10, align 8
  %.reload.addr = getelementptr inbounds %test_simple.Frame, ptr %2, i32 0, i32 0
  %.reload = load i64, ptr %.reload.addr, align 8
  call swiftcc void @marker(i32 2000)
  br label %coro.end

5:                                               ; preds = %entry
  call swiftcc void @marker(i32 3000)
  br label %coro.end

coro.end:                                         ; preds = %10, %11
  %12 = call i1 @llvm.coro.end(ptr %3, i1 false, token none)
  unreachable
}

declare noalias ptr @swift_coroFrameAlloc(i64, i64)
declare swiftcc void @marker(i32) #1
declare swiftcc void @val(ptr noalias nocapture sret(<{ i64, i64, i64, i64, i64 }>))

; CHECK-LABEL: @test_simple(
; CHECK-NEXT:  entry:
; CHECK:    [[TMP0:%.*]] = call ptr @swift_coroFrameAlloc
; CHECK:    getelementptr inbounds %test_simple.Frame, ptr [[TMP0]]
; CHECK:    ret ptr @test_simple.resume.0

; CHECK-LABEL: @test_simple.resume.0(
; CHECK: getelementptr inbounds %test_simple.Frame