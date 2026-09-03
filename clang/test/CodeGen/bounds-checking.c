// N.B. The clang driver defaults to -fsanitize-merge but clang_cc1 effectively
// defaults to -fno-sanitize-merge.
// RUN: %clang_cc1 -fsanitize=array-bounds -O -fsanitize-trap=array-bounds -emit-llvm -triple x86_64-apple-darwin10 -DNO_DYNAMIC %s -o - |     FileCheck %s --check-prefixes=CHECK,INCDEC
// RUN: %clang_cc1 -fsanitize=array-bounds -O                              -emit-llvm -triple x86_64-apple-darwin10 %s -o -              | not FileCheck %s
//
// RUN: %clang_cc1 -fsanitize=local-bounds    -fsanitize-trap=local-bounds -emit-llvm -triple x86_64-apple-darwin10              %s -o - |     FileCheck %s
//
// RUN: %clang_cc1 -fsanitize=local-bounds -fsanitize-trap=local-bounds                               -O3 -emit-llvm -triple x86_64-apple-darwin10 %s -o - |     FileCheck %s --check-prefixes=NOOPTLOCAL
// RUN: %clang_cc1 -fsanitize=local-bounds -fsanitize-trap=local-bounds -fno-sanitize-merge           -O3 -emit-llvm -triple x86_64-apple-darwin10 %s -o - |     FileCheck %s --check-prefixes=NOOPTLOCAL
// RUN: %clang_cc1 -fsanitize=local-bounds -fsanitize-trap=local-bounds -fsanitize-merge=local-bounds -O3 -emit-llvm -triple x86_64-apple-darwin10 %s -o - | not FileCheck %s --check-prefixes=NOOPTLOCAL
//
// RUN: %clang_cc1 -fsanitize=array-bounds -fsanitize-trap=array-bounds                               -O3 -emit-llvm -triple x86_64-apple-darwin10 %s -o - |     FileCheck %s --check-prefixes=NOOPTARRAY
// RUN: %clang_cc1 -fsanitize=array-bounds -fsanitize-trap=array-bounds -fno-sanitize-merge           -O3 -emit-llvm -triple x86_64-apple-darwin10 %s -o - |     FileCheck %s --check-prefixes=NOOPTARRAY
// RUN: %clang_cc1 -fsanitize=array-bounds -fsanitize-trap=array-bounds -fsanitize-merge=array-bounds -O3 -emit-llvm -triple x86_64-apple-darwin10 %s -o - | not FileCheck %s --check-prefixes=NOOPTARRAY
//
// REQUIRES: x86-registered-target

// CHECK-LABEL: @f1
double f1(int b, int i) {
  double a[b];
  // CHECK: call {{.*}} @llvm.{{(ubsan)?trap}}
  return a[i];
}

// CHECK-LABEL: @f2
void f2(void) {
  // everything is constant; no trap possible
  // CHECK-NOT: call {{.*}} @llvm.{{(ubsan)?trap}}
  int a[2];
  a[1] = 42;

#ifndef NO_DYNAMIC
  extern void *malloc(__typeof__(sizeof(0)));
  short *b = malloc(64);
  b[5] = *a + a[1] + 2;
#endif
}

// CHECK-LABEL: @f3
void f3(void) {
  int a[1];
  // CHECK: call {{.*}} @llvm.{{(ubsan)?trap}}
  a[2] = 1;
}

// CHECK-LABEL: @f4
__attribute__((no_sanitize("bounds")))
int f4(int i) {
  int b[64];
  // CHECK-NOT: call void @llvm.trap()
  // CHECK-NOT: trap:
  // CHECK-NOT: cont:
  return b[i];
}

// Union flexible-array members are a C99 extension. All array members with a
// constant size should be considered FAMs.

union U { int a[0]; int b[1]; int c[2]; };

// CHECK-LABEL: @f5
int f5(union U *u, int i) {
  // a is treated as a flexible array member.
  // CHECK-NOT: @llvm.ubsantrap
  return u->a[i];
}

// CHECK-LABEL: @f6
int f6(union U *u, int i) {
  // b is treated as a flexible array member.
  // CHECK-NOT: call {{.*}} @llvm.{{(ubsan)?trap}}
  return u->b[i];
}

// CHECK-LABEL: @f7
int f7(union U *u, int i) {
  // c is treated as a flexible array member.
  // CHECK-NOT: @llvm.ubsantrap
  return u->c[i];
}

char B[10];
char B2[10];
// CHECK-LABEL: @f8
// Check the label to prevent spuriously matching ubsantraps from other
// functions.
// NOOPTLOCAL-LABEL: @f8
// NOOPTARRAY-LABEL: @f8
void f8(int i, int k) {
  // NOOPTLOCAL: call void @llvm.ubsantrap(i8 3) #[[ATTR1:[0-9]+]]
  // NOOPTARRAY: call void @llvm.ubsantrap(i8 18) #[[ATTR2:[0-9]+]]
  B[i] = '\0';

  // NOOPTLOCAL: call void @llvm.ubsantrap(i8 5) #[[ATTR1:[0-9]+]]
  // NOOPTARRAY: call void @llvm.ubsantrap(i8 18) #[[ATTR2:[0-9]+]]
  B2[k] = '\0';
}

// See commit 9a954c6 that caused a SEGFAULT in this code.
struct S {
  __builtin_va_list ap;
} *s;
// CHECK-LABEL: @f9
struct S *f9(int i) {
  return &s[i];
}

// Pre/post inc/dec on an array element must use the same bounds check as a
// store: index < size, not index <= size. A prior bug allowed the write at
// index == size (one past the end) to slip through.
int Arr[5];
// INCDEC-LABEL: @f10_store
void f10_store(int i) {
  // INCDEC: icmp ult i32 %i, 5
  // INCDEC: call {{.*}} @llvm.{{(ubsan)?trap}}
  Arr[i] = 1;
}
// INCDEC-LABEL: @f10_postinc
void f10_postinc(int i) {
  // INCDEC: icmp ult i32 %i, 5
  // INCDEC: call {{.*}} @llvm.{{(ubsan)?trap}}
  Arr[i]++;
}
// INCDEC-LABEL: @f10_preinc
void f10_preinc(int i) {
  // INCDEC: icmp ult i32 %i, 5
  // INCDEC: call {{.*}} @llvm.{{(ubsan)?trap}}
  ++Arr[i];
}
// INCDEC-LABEL: @f10_postdec
void f10_postdec(int i) {
  // INCDEC: icmp ult i32 %i, 5
  // INCDEC: call {{.*}} @llvm.{{(ubsan)?trap}}
  Arr[i]--;
}
// INCDEC-LABEL: @f10_predec
void f10_predec(int i) {
  // INCDEC: icmp ult i32 %i, 5
  // INCDEC: call {{.*}} @llvm.{{(ubsan)?trap}}
  --Arr[i];
}

// NOOPTLOCAL: attributes #[[ATTR1]] = { nomerge noreturn nounwind }
// NOOPTARRAY: attributes #[[ATTR2]] = { nomerge noreturn nounwind }
