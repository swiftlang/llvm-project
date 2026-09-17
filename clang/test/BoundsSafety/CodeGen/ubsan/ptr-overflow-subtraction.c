// RUN: %clang_cc1 -O0 -fbounds-safety -fsanitize=pointer-overflow -fsanitize-trap=pointer-overflow -emit-llvm %s -o - | FileCheck %s

#include <ptrcheck.h>

// Regression test for pointer subtraction generating a false-positive
// UBSan pointer overflow check when -fbounds-safety is enabled.
// The check must use the negated index so that (base - offset) ule base
// is checked, not (base + offset) ule base which is unconditionally false.
void ptr_sub(unsigned char * __bidi_indexable ptr, unsigned int offset) {
    unsigned char * __bidi_indexable ptr2 = ptr - offset;
    (void)ptr2;
}

// CHECK: %[[OFFSET:[a-z0-9]+]] = load i32
// CHECK: %[[NEG:[a-z0-9.]+]] = sub i32 0, %[[OFFSET]]
// CHECK: getelementptr{{.*}} %[[NEG]]
// CHECK: %[[EXT:[a-z0-9.]+]] = sext i32 %[[NEG]] to i64
// CHECK: call { i64, i1 } @llvm.smul.with.overflow.i64(i64 1, i64 %[[EXT]])
