// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -fblocks -triple arm64-apple-ios -fptrauth-calls -emit-llvm -no-enable-noundef-analysis -std=c++11 %s -o - | FileCheck %s

// CHECK: @global_handler = global ptr ptrauth (ptr @_Z8handler2v, i32 0)

// CHECK: define void @_Z7handlerv()
__attribute__((noinline)) void handler() {
    asm volatile("");
}

// CHECK: define void @_Z8handler2v()
__attribute__((noinline)) void handler2() {
    asm volatile("");
}
void (*global_handler)() = &handler2;

// CHECK: define void @_Z11callHandlerRFvvE(ptr nonnull %handler)
void callHandler(void (&handler)()) {
    asm volatile("");
    // Check basic usage of function reference
    ^{
        handler();
    }();
// CHECK: [[HANDLER:%.*]] = load ptr, ptr %handler.addr
// CHECK: store ptr [[HANDLER]], ptr %block.captured,
// CHECK: foobar:
    asm volatile("foobar:");

    // Check escape of function reference
    ^{
        global_handler = handler;
    }();
// CHECK: [[CAPTURE_SLOT:%.*]] = getelementptr inbounds <{ ptr, i32, i32, ptr, ptr, ptr }>, ptr %block1, i32 0, i32 5
// CHECK: [[HANDLER:%.*]] = load ptr, ptr %handler.addr
// CHECK: store ptr [[HANDLER]], ptr [[CAPTURE_SLOT]]
    asm volatile("");

    // Check return of function reference
    ^{
        return handler;
    }()();
// CHECK: [[CAPTURE_SLOT:%.*]] = getelementptr inbounds <{ ptr, i32, i32, ptr, ptr, ptr }>, ptr %block8, i32 0, i32 5
// CHECK: [[HANDLER:%.*]] = load ptr, ptr %handler.addr
// CHECK: store ptr [[HANDLER]], ptr [[CAPTURE_SLOT]]
    asm volatile("");
}
