// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -cc1 -internal-isystem /Users/oliver/llvm-internal/debug/lib/clang/11.0.0/include -nostdsysteminc -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-strip -triple arm64-apple-ios -emit-llvm -O2 -disable-llvm-passes -o - %s | FileCheck %s
// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -cc1 -internal-isystem /Users/oliver/llvm-internal/debug/lib/clang/11.0.0/include -nostdsysteminc -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-auth  -triple arm64-apple-ios -emit-llvm -O2 -disable-llvm-passes -o - %s | FileCheck %s

#define CFSTR __builtin___CFStringMakeConstantString

void f() {
  CFSTR("Hello, World!");
}

const void *G = CFSTR("yo joe");

void h() {
  static const void *h = CFSTR("Goodbye, World!");
}

// CHECK: @__CFConstantStringClassReference = external global [0 x i32]
// CHECK: @.str = private unnamed_addr constant [14 x i8] c"Hello, World!\00", section "__TEXT,__cstring,cstring_literals", align 1
// CHECK: @_unnamed_cfstring_ = private global %struct.__NSConstantString_tag { ptr ptrauth (ptr @__CFConstantStringClassReference, i32 2, i64 27361, ptr @_unnamed_cfstring_), i32 1992, ptr @.str, i64 13 }, section "__DATA,__cfstring"
// CHECK: @.str.1 = private unnamed_addr constant [7 x i8] c"yo joe\00", section "__TEXT,__cstring,cstring_literals", align 1
// CHECK: @_unnamed_cfstring_.2 = private global %struct.__NSConstantString_tag { ptr ptrauth (ptr @__CFConstantStringClassReference, i32 2, i64 27361, ptr @_unnamed_cfstring_.2), i32 1992, ptr @.str.1, i64 6 }, section "__DATA,__cfstring"
// CHECK: @G = global ptr @_unnamed_cfstring_.2
// CHECK: @h.h = internal global ptr @_unnamed_cfstring_.4
// CHECK: @.str.3 = private unnamed_addr constant [16 x i8] c"Goodbye, World!\00", section "__TEXT,__cstring,cstring_literals"
// CHECK: @_unnamed_cfstring_.4 = private global %struct.__NSConstantString_tag { ptr ptrauth (ptr @__CFConstantStringClassReference, i32 2, i64 27361, ptr @_unnamed_cfstring_.4), i32 1992, ptr @.str.3, i64 15 }, section "__DATA,__cfstring"
