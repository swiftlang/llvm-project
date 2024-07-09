// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -triple arm64e-apple-ios13.0 -fptrauth-calls -fptrauth-intrinsics -O1 %s -emit-llvm -o - | FileCheck %s

#include <ptrauth.h>

void *test_nop_cast(void (*fptr)(int)) {
  // CHECK: define noundef ptr @test_nop_cast(ptr noundef readnone returned [[FPTR:%.*]])
  // CHECK: ret ptr [[FPTR]]
  return ptrauth_nop_cast(void *, fptr);
}

typedef void (*VoidFn)(void);

VoidFn test_nop_cast_functype(void (*fptr)(int, int)) {
  // CHECK: define noundef ptr @test_nop_cast_functype(ptr noundef readnone returned [[FPTR:%.*]])
  // CHECK: ret ptr [[FPTR]]
  return ptrauth_nop_cast(void (*)(void), fptr);
}

void *test_nop_cast_direct() {
  // CHECK: define nonnull ptr @test_nop_cast_direct()
  // CHECK: ret ptr ptrauth (ptr @test_nop_cast_direct, i32 0)
  return ptrauth_nop_cast(void *, test_nop_cast_direct);
}
