// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -ffeature-availability=feature1:on -ffeature-availability=feature2:off -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -emit-llvm -o - -DUSE_DOMAIN -Werror=unused-variable %s | FileCheck --check-prefixes=CHECK,DOMAIN %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -emit-llvm -o - -DUSE_DOMAIN -DALWAYS_ENABLED -Werror=unused-variable %s | FileCheck --check-prefixes=CHECK,DOMAIN %s

// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -ffeature-availability=feature1:on -ffeature-availability=feature2:off -emit-pch -o %t %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -ffeature-availability=feature1:on -ffeature-availability=feature2:off -include-pch %t -emit-llvm -o - %s | FileCheck %s

// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -emit-pch -o %t -DUSE_DOMAIN %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -include-pch %t -emit-llvm -o - -DUSE_DOMAIN %s | FileCheck --check-prefixes=CHECK,DOMAIN %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -emit-pch -o %t -DUSE_DOMAIN -DALWAYS_ENABLED %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fblocks -include-pch %t -emit-llvm -o - -DUSE_DOMAIN -DALWAYS_ENABLED %s | FileCheck --check-prefixes=CHECK,DOMAIN %s

// CHECK: %[[STRUCT_S0:.*]] = type { i32 }
// CHECK: @g0 = external global i32, align 4
// CHECK-NOT: @g1
// CHECK-NOT: @g2

#ifndef HEADER
#define HEADER

#include <availability_domain.h>

#define AVAIL 0
#define UNAVAIL 1

#ifdef USE_DOMAIN
// DOMAIN: @g3 = extern_weak global i32, align 4

#ifdef ALWAYS_ENABLED
CLANG_ALWAYS_ENABLED_AVAILABILITY_DOMAIN(feature1);
#else
CLANG_ENABLED_AVAILABILITY_DOMAIN(feature1);
#endif
CLANG_DISABLED_AVAILABILITY_DOMAIN(feature2);
#endif

__attribute__((availability(domain:feature1, AVAIL))) int func0(void);
__attribute__((availability(domain:feature2, AVAIL))) int func1(void);
int func2(void);

__attribute__((availability(domain:feature1, AVAIL))) extern int g0;
__attribute__((availability(domain:feature2, AVAIL))) int g1 = 100;
__attribute__((availability(domain:feature2, AVAIL))) int g2;

struct __attribute__((availability(domain:feature1, AVAIL))) S0 {
  int d0;
};

// E0_B and E0_C can never be instantiated at runtime, so a switch on enum E0
// never takes their values.
enum E0 {
  E0_A,
  E0_B __attribute__((availability(domain:feature1, UNAVAIL))),
  E0_C __attribute__((availability(domain:feature2, AVAIL))),
  E0_D,
};

// CHECK-LABEL: define void @test0()
// CHECK-NOT: br
// CHECK: call i32 @func0()
// CHECK: store i32 123, ptr @g0, align 4
// CHECK-NOT: func1()
// CHECK-NOT: func2()
void test0(void) {
  if (__builtin_available(domain:feature1)) {
    func0();
    g0 = 123;
  }

  if (__builtin_available(domain:feature2)) {
    func1();
    g1 = 123;
  }

  if (__builtin_available(domain:feature1))
    if (__builtin_available(domain:feature2)) {
      func2();
    }
}

// CHECK-LABEL: define void @test1()
__attribute__((availability(domain:feature1, AVAIL)))
void test1(void) {
}

// CHECK-NOT: @test2(
__attribute__((availability(domain:feature2, AVAIL)))
void test2(void) {
}

// CHECK-LABEL: define void @test3(
// CHECK: %[[D0:.*]] = getelementptr inbounds nuw %[[STRUCT_S0]], ptr %{{.*}}, i32 0, i32 0
// CHECK: store i32 134, ptr %[[D0]], align 4
__attribute__((availability(domain:feature1, AVAIL)))
void test3(struct S0 *s0) {
  s0->d0 = 134;
}

#ifdef USE_DOMAIN
// DOMAIN-LABEL: define void @test4()
// DOMAIN: %[[CALL:.*]] = call i32 @pred1()
// DOMAIN-NEXT: %[[TOBOOL:.*]] = icmp ne i32 %[[CALL]], 0
// DOMAIN-NEXT: br i1 %[[TOBOOL]], label %[[IF_THEN:.*]], label %[[IF_END:.*]]
//
// DOMAIN: [[IF_THEN]]:
// DOMAIN-NEXT: %[[CALL1:.*]] = call i32 @func3()
// DOMAIN-NEXT: store i32 1, ptr @g3, align 4
// DOMAIN-NEXT: br label %[[IF_END]]
//
// DOMAIN: [[IF_END]]:
// DOMAIN-NEXT: ret void

int pred1(void);
CLANG_DYNAMIC_AVAILABILITY_DOMAIN(feature3, pred1);
__attribute__((availability(domain:feature3, AVAIL))) int func3(void);
__attribute__((availability(domain:feature3, AVAIL))) extern int g3;

void test4(void) {
  if (__builtin_available(domain:feature3)) {
    func3();
    g3 = 1;
  }
}

// DOMAIN: declare extern_weak i32 @func3()

#endif

// CHECK-LABEL: define void @test5()
// CHECK: br label %[[L1:.*]]
// CHECK: [[L1]]:
// CHECK-NEXT: call i32 @func0()
// CHECK-NEXT: ret void

void test5(void) {
  if (__builtin_available(domain:feature1)) {
    goto L1;
L1:
    func0();
  } else {
    goto L2;
L2:
    func2();
  }
}

// The switch has no destination for E0_B or E0_C, so the blocks that hold their
// statements have no predecessors.
// CHECK-LABEL: define void @test6(
// CHECK: switch i32 %{{.*}}, label %[[EPILOG:.*]] [
// CHECK-NEXT: i32 0, label %[[BB_A:.*]]
// CHECK-NEXT: i32 3, label %{{.*}}
// CHECK-NEXT: ]
//
// CHECK: [[BB_A]]:
// CHECK-NEXT: call i32 @func2()
// CHECK-NEXT: br label %[[EPILOG]]

void test6(enum E0 e) {
  switch (e) {
  case E0_A:
    func2();
    break;
  case E0_B:
    func2();
    break;
  case E0_C:
    func2();
    break;
  case E0_D:
    func2();
    break;
  }
}

// E0_A still falls through into the statements of E0_B, and E0_D still shares a
// block with E0_C, so dropping the two values doesn't drop either block.
// CHECK-LABEL: define void @test7(
// CHECK: switch i32 %{{.*}}, label %[[EPILOG:.*]] [
// CHECK-NEXT: i32 0, label %[[BB_A:.*]]
// CHECK-NEXT: i32 3, label %[[BB_CD:.*]]
// CHECK-NEXT: ]
//
// CHECK: [[BB_A]]:
// CHECK-NEXT: call i32 @func2()
// CHECK-NEXT: br label %[[BB_B:.*]]
//
// CHECK: [[BB_B]]:
// CHECK-NEXT: call i32 @func2()
// CHECK-NEXT: br label %[[EPILOG]]
//
// CHECK: [[BB_CD]]:
// CHECK-NEXT: call i32 @func2()
// CHECK-NEXT: br label %[[EPILOG]]

void test7(enum E0 e) {
  switch (e) {
  case E0_A:
    func2();
  case E0_B:
    func2();
    break;
  case E0_C:
  case E0_D:
    func2();
    break;
  }
}

#ifdef USE_DOMAIN

// E1_A is guarded by a dynamic domain, so it can be instantiated at runtime and
// keeps its destination in the switch. E1_B can't, so it loses its destination.
enum E1 {
  E1_A __attribute__((availability(domain:feature3, AVAIL))),
  E1_B __attribute__((availability(domain:feature2, AVAIL))),
};

// DOMAIN-LABEL: define void @test8(
// DOMAIN: switch i32 %{{.*}}, label %{{.*}} [
// DOMAIN-NEXT: i32 0, label %{{.*}}
// DOMAIN-NEXT: ]

void test8(enum E1 e) {
  switch (e) {
  case E1_A:
    func2();
    break;
  case E1_B:
    func2();
    break;
  }
}

#endif

#endif /* HEADER */
