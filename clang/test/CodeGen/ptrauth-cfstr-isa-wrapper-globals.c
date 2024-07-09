// RUN: %clang_cc1 -cc1 -internal-isystem /Users/oliver/llvm-internal/debug/lib/clang/11.0.0/include -nostdsysteminc -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-strip -triple arm64-apple-ios -emit-llvm -O2 -disable-llvm-passes -o - %s | FileCheck %s
// RUN: %clang_cc1 -cc1 -internal-isystem /Users/oliver/llvm-internal/debug/lib/clang/11.0.0/include -nostdsysteminc -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-auth  -triple arm64-apple-ios -emit-llvm -O2 -disable-llvm-passes -o - %s | FileCheck %s

#define CFSTR __builtin___CFStringMakeConstantString

void f() {
  CFSTR("Hello, World!");
}

const void *G = CFSTR("yo joe");

void h() {
  static const void *h = CFSTR("Goodbye, World!");
}

// CHECK: @__CFConstantStringClassReference = external global [0 x i32]
// CHECK: @__CFConstantStringClassReference.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @__CFConstantStringClassReference, i32 2, i64 ptrtoint (ptr @_unnamed_cfstring_ to i64), i64 27361 }, section "llvm.ptrauth", align 8 
// CHECK: @.str = private unnamed_addr constant [14 x i8] c"Hello, World!\00", section "__TEXT,__cstring,cstring_literals", align 1
// CHECK: @_unnamed_cfstring_ = private global %struct.__NSConstantString_tag { ptr @__CFConstantStringClassReference.ptrauth, i32 1992, ptr @.str, i64 13 }, section "__DATA,__cfstring"
// CHECK: @__CFConstantStringClassReference.ptrauth.1 = private constant { ptr, i32, i64, i64 } { ptr @__CFConstantStringClassReference, i32 2, i64 ptrtoint (ptr @_unnamed_cfstring_.3 to i64), i64 27361 }, section "llvm.ptrauth"
// CHECK: @.str.2 = private unnamed_addr constant [7 x i8] c"yo joe\00", section "__TEXT,__cstring,cstring_literals", align 1
// CHECK: @_unnamed_cfstring_.3 = private global %struct.__NSConstantString_tag { ptr @__CFConstantStringClassReference.ptrauth.1, i32 1992, ptr @.str.2, i64 6 }, section "__DATA,__cfstring"
// CHECK: @G = global ptr @_unnamed_cfstring_.3
// CHECK: @h.h = internal global ptr @_unnamed_cfstring_.6
// CHECK: @__CFConstantStringClassReference.ptrauth.4 = private constant { ptr, i32, i64, i64 } { ptr @__CFConstantStringClassReference, i32 2, i64 ptrtoint (ptr @_unnamed_cfstring_.6 to i64), i64 27361 }, section "llvm.ptrauth"
// CHECK: @.str.5 = private unnamed_addr constant [16 x i8] c"Goodbye, World!\00", section "__TEXT,__cstring,cstring_literals"
// CHECK: @_unnamed_cfstring_.6 = private global %struct.__NSConstantString_tag { ptr @__CFConstantStringClassReference.ptrauth.4, i32 1992, ptr @.str.5, i64 15 }, section "__DATA,__cfstring"
