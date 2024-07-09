// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-calls -fptrauth-intrinsics -emit-llvm %s  -o - | FileCheck %s --check-prefixes=CHECK,NOFPDIV
// RUN: %clang_cc1 -fptrauth-function-pointer-type-discrimination -triple arm64-apple-ios -fptrauth-calls -fptrauth-intrinsics -emit-llvm %s  -o - | FileCheck %s --check-prefixes=CHECK,FPDIV

#include <ptrauth.h>

#define FNPTRKEY 0
#define FNPTRDISC ptrauth_function_pointer_type_discriminator(void(*)(void))

void (*fnptr)(void);
long discriminator;

// NOFPDIV: @fnptr_discriminator = global i64 [[FPDISC:0]], align 8
// FPDIV: @fnptr_discriminator = global i64 [[FPDISC:18983]], align 8
long fnptr_discriminator = FNPTRDISC;


// CHECK-LABEL: define void @test_sign_unauthenticated_peephole()
void test_sign_unauthenticated_peephole() {
  // CHECK:      [[T0:%.*]] = load ptr, ptr @fnptr,
  // CHECK-NEXT: call void [[T0]](){{$}}
  // CHECK-NEXT: ret void
  __builtin_ptrauth_sign_unauthenticated(fnptr, FNPTRKEY, FNPTRDISC)();
}

// This peephole doesn't kick in because it's incorrect when ABI pointer
// authentication is enabled.
// CHECK-LABEL: define void @test_auth_peephole()
void test_auth_peephole() {
  // CHECK:      [[T0:%.*]] = load ptr, ptr @fnptr,
  // CHECK-NEXT: [[T1:%.*]] = load i64, ptr @discriminator,
  // CHECK-NEXT: [[T2:%.*]] = ptrtoint ptr [[T0]] to i64
  // CHECK-NEXT: [[T3:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[T2]], i32 0, i64 [[T1]])
  // CHECK-NEXT: [[T4:%.*]] = inttoptr  i64 [[T3]] to ptr
  // CHECK-NEXT: call void [[T4]]() [ "ptrauth"(i32 0, i64 [[FPDISC]]) ]
  // CHECK-NEXT: ret void
  __builtin_ptrauth_auth(fnptr, 0, discriminator)();
}

// CHECK-LABEL: define void @test_auth_and_resign_peephole()
void test_auth_and_resign_peephole() {
  // CHECK:      [[T0:%.*]] = load ptr, ptr @fnptr,
  // CHECK-NEXT: [[T1:%.*]] = load i64, ptr @discriminator,
  // CHECK-NEXT: call void [[T0]]() [ "ptrauth"(i32 2, i64 [[T1]]) ]
  // CHECK-NEXT: ret void
  __builtin_ptrauth_auth_and_resign(fnptr, 2, discriminator, FNPTRKEY, FNPTRDISC)();
}
