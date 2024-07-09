// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-calls -fptrauth-returns -fptrauth-intrinsics -fobjc-arc -fobjc-runtime-has-weak -fblocks -emit-llvm -no-enable-noundef-analysis -o - %s | FileCheck %s

#define ATTR0 __attribute__((ptrauth_struct(1, 100)))
#define ATTR1 __attribute__((ptrauth_struct(1, 101)))
#define ATTR2 __attribute__((ptrauth_struct(1, 102)))
#define ATTR3 __attribute__((ptrauth_struct(1, 103)))
#define ATTR4 __attribute__((ptrauth_struct(1, 104)))

// CHECK: %[[STRUCT_S0:.*]] = type { i32, i32 }
// CHECK: %[[STRUCT_S1:.*]] = type { i32, ptr, ptr, %[[STRUCT_S0]] }
// CHECK: %[[STRUCT_S4:.*]] = type { i32, i32, ptr, <4 x float> }

// CHECK: @g0 = global %[[STRUCT_S0]] zeroinitializer, align 4
// CHECK: @[[G0_PTRAUTH:.*]] = private constant { ptr, i32, i64, i64 } { ptr @g0, i32 1, i64 0, i64 100 }, section "llvm.ptrauth", align 8
// CHECK: @gp0 = global ptr @[[G0_PTRAUTH]], align 8
// CHECK: @[[PTRAUTH:.*]] = private constant { ptr, i32, i64, i64 } { ptr getelementptr (i8, ptr @g1, i64 24), i32 1, i64 0, i64 100 }, section "llvm.ptrauth", align 8
// CHECK: @gp1 = global ptr @[[PTRAUTH]], align 8
// CHECK: @ga2 = global [4 x i32] zeroinitializer, align 4
// CHECK: @gf0 = global ptr @ga2, align 8
// CHECK: @[[CONST_TEST_COMPOUND_LITERAL0_T0:.*]] = private unnamed_addr constant %[[STRUCT_S0]] { i32 1, i32 2 }, align 4

typedef long intptr_t;

struct ATTR0 S0 {
  int a0, a1;
};

typedef struct S0 S0;

struct ATTR1 S1 {
  int a;
  id b;
  S0 *f0;
  S0 f1;
};

typedef struct S1 S1;

struct ATTR2 S2;
typedef struct S2 S2;

struct ATTR3 S3 {
  int f0;
  __attribute__((annotate("abc"))) S0 f1;
};

typedef struct S3 S3;

typedef __attribute__((ext_vector_type(4))) float float4;

struct ATTR4 S4 {
  int f0, f1;
  __weak id f2;
  float4 extv0;
};

typedef struct S4 S4;

S0 getS0(void);
S0 *func0(S0 *);
S0 func1(S0);
S4 func2(S4);

S0 g0;
S0 *gp0 = &g0;
S1 g1;
S0 *gp1 = &g1.f1;

S0 ga0[10][10][10];
S0 ga1[2];

int ga2[4] = {0};
void (*gf0)(void) = (void (*)(void))ga2;

// CHECK-LABEL: define void @test_member_access0(
// CHECK-NOT: @llvm.ptrauth
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %{{.*}}, i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V3]], i64 4
// CHECK: load i32, ptr %[[RESIGNEDGEP]], align 4

void test_member_access0(S0 *s) {
  int t = s->a1;
}

// CHECK-LABEL: define void @test_member_access1(
// CHECK-NOT: @llvm.ptrauth
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %{{.*}}, i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V3]], i64 4
// CHECK: load i32, ptr %[[RESIGNEDGEP]], align 4

void test_member_access1(S0 *s) {
  int t = (*s).a1;
}

// CHECK-LABEL: define void @test_member_access2(
// CHECK-NOT: @llvm.ptrauth

void test_member_access2(S0 s) {
  int t = s.a1;
}

// CHECK-LABEL: define void @test_member_initialization(
// CHECK: %[[T:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1:.*]] = alloca %[[STRUCT_S1]], align 8
// CHECK: %[[S0:.*]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[T1]], i32 0, i32 2
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[T]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V0]], i32 1, i64 100)
// CHECK: %[[V2:.*]] = inttoptr i64 %[[V1]] to ptr
// CHECK: store ptr %[[V2]], ptr %[[S0]], align 8

void test_member_initialization() {
  S0 t;
  S1 t1 = {1, 0, &t};
}

// CHECK-LABEL: define i32 @test_array0(
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V3]], i64 84
// CHECK: load i32, ptr %[[RESIGNEDGEP]], align 4

int test_array0(S0 *a) {
  return a[10].a1;
}

// CHECK-LABEL: define i32 @test_array1(
// CHECK: %[[V1:.*]] = load i32, ptr %{{.*}}, align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V1]], 2
// CHECK: %[[IDXPROM:.*]] = sext i32 %[[ADD]] to i64
// CHECK: %[[ARRAYIDX_IDX:.*]] = mul nsw i64 %[[IDXPROM]], 8
// CHECK: %[[ADD:.*]] = add i64 %[[ARRAYIDX_IDX]], 4
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 100)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V4]], i64 %[[ADD]]
// CHECK: load i32, ptr %[[RESIGNEDGEP]], align 4

int test_array1(S0 *a, int i) {
  return a[i + 2].a1;
}

// CHECK-LABEL: define i32 @test_array2(
// CHECK: %[[V0:.*]] = load i32, ptr %{{.*}}, align 4
// CHECK: %[[IDXPROM:.*]] = sext i32 %[[V0]] to i64
// CHECK: %[[ARRAYIDX:.*]] = getelementptr inbounds [10 x [10 x [10 x %[[STRUCT_S0]]]]], ptr @ga0, i64 0, i64 %[[IDXPROM]]
// CHECK: %[[V1:.*]] = load i32, ptr %{{.*}}, align 4
// CHECK: %[[IDXPROM1:.*]] = sext i32 %[[V1]] to i64
// CHECK: %[[ARRAYIDX2:.*]] = getelementptr inbounds [10 x [10 x %[[STRUCT_S0]]]], ptr %[[ARRAYIDX]], i64 0, i64 %[[IDXPROM1]]
// CHECK: %[[V2:.*]] = load i32, ptr %{{.*}}, align 4
// CHECK: %[[IDXPROM3:.*]] = sext i32 %[[V2]] to i64
// CHECK: %[[ARRAYIDX4:.*]] = getelementptr inbounds [10 x %[[STRUCT_S0]]], ptr %[[ARRAYIDX2]], i64 0, i64 %[[IDXPROM3]]
// CHECK: %[[A1:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[ARRAYIDX4]], i32 0, i32 1
// CHECK: load i32, ptr %[[A1]], align 4

int test_array2(int i, int j, int k) {
  return ga0[i][j][k].a1;
}

// CHECK: define ptr @test_array3(
// CHECK: %[[V0:.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @ga1 to i64), i32 1, i64 100)
// CHECK: %[[V1:.*]] = inttoptr i64 %[[V0]] to ptr
// CHECK: ret ptr %[[V1]]

S0 *test_array3(void) {
  return ga1;
}

// CHECK-LABEL: define i32 @test_pointer_arithmetic0(
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %{{.*}}, i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[ADD_PTR:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[V3]], i64 10
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[ADD_PTR]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V4]], i32 1, i64 100)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: %[[V7:.*]] = ptrtoint ptr %[[V6]] to i64
// CHECK: %[[V8:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V7]], i32 1, i64 100)
// CHECK: %[[V9:.*]] = inttoptr i64 %[[V8]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V9]], i64 4
// CHECK: %[[V12:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4

int test_pointer_arithmetic0(S0 *a) {
  return (a + 10)->a1;
}

// CHECK: define ptr @test_pointer_arithmetic1(
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[INCDEC_PTR:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[V3]], i32 1
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[INCDEC_PTR]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V4]], i32 1, i64 100)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: store ptr %[[V6]], ptr %[[A_ADDR]], align 8
// CHECK: ret ptr %[[V6]]

S0 *test_pointer_arithmetic1(S0 *a) {
  return ++a;
}

// CHECK: define ptr @test_pointer_arithmetic2(
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[INCDEC_PTR:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[V3]], i32 1
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[INCDEC_PTR]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V4]], i32 1, i64 100)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: store ptr %[[V6]], ptr %[[A_ADDR]], align 8
// CHECK: ret ptr %[[V0]]

S0 *test_pointer_arithmetic2(S0 *a) {
  return a++;
}

// CHECK-LABEL: define void @test_dereference0(
// CHECK: %[[A0_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[A1_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A0_ADDR]], align 8
// CHECK: %[[V1:.*]] = load ptr, ptr %[[A1_ADDR]], align 8
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V4]], i32 1, i64 100)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: %[[V7:.*]] = ptrtoint ptr %[[V1]] to i64
// CHECK: %[[V8:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V7]], i32 1, i64 100)
// CHECK: %[[V9:.*]] = inttoptr i64 %[[V8]] to ptr
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 4 %[[V6]], ptr align 4 %[[V9]], i64 8, i1 false)

void test_dereference0(S0 *a0, S0 *a1) {
  *a0 = *a1;
}

// CHECK-LABEL: define void @test_dereference1(
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T:.*]] = alloca %[[STRUCT_S1]], align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8
// CHECK: %[[V3:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V3]], i32 1, i64 101)
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr
// CHECK: call void @__copy_constructor_8_8_t0w4_s8_t16w16(ptr %[[T]], ptr %[[V5]])

void test_dereference1(S1 *s) {
  S1 t = *s;
}

// CHECK-LABEL: define void @test_address_of0(
// CHECK: %[[T:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[T]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V0]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: store ptr %[[V3]], ptr %{{.*}}, align 8

void test_address_of0(void) {
  S0 t = getS0();
  S0 *p = &t;
}

// CHECK-LABEL: define void @test_address_of1(
// CHECK: %[[V0:.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @g0 to i64), i32 1, i64 100)
// CHECK: %[[V1:.*]] = inttoptr i64 %[[V0]] to ptr
// CHECK: store ptr %[[V1]], ptr %{{.*}}, align 8

void test_address_of1(void) {
  S0 *p = &g0;
}

// CHECK-LABEL: define void @test_address_of2(
// CHECK: %[[T:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[CALL:.*]] = call i64 @getS0()
// CHECK: store i64 %[[CALL]], ptr %[[T]], align 4
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 4 %[[T1]], ptr align 4 %[[T]], i64 8, i1 false)

void test_address_of2(void) {
  S0 t = getS0();
  S0 t1 = *&t;
}

// CHECK-LABEL: define void @test_conversion0(
// CHECK: %[[P_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T:.*]] = alloca ptr, align 8
// CHECK: %[[T2:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[P_ADDR]], align 8

// CHECK: %[[V3:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V3]], i32 1, i64 100)
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr

// CHECK: %[[V6:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V5]], %{{.*}} ]
// CHECK: store ptr %[[V6]], ptr %[[T]], align 8
// CHECK: %[[V7:.*]] = load ptr, ptr %[[T]], align 8

// CHECK: %[[V10:.*]] = ptrtoint ptr %[[V8]] to i64
// CHECK: %[[V11:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V10]], i32 1, i64 100)
// CHECK: %[[V12:.*]] = inttoptr i64 %[[V11]] to ptr

// CHECK: %[[V13:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V12]], %{{.*}} ]
// CHECK: store ptr %[[V13]], ptr %[[T2]], align 8

void test_conversion0(void *p) {
  S0 *t = (S0 *)p;
  void *t2 = t;
}

// CHECK-LABEL: define void @test_conversion1(
// CHECK: %[[P_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[P_ADDR]], align 8

// CHECK: %[[V3:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V3]], i32 1, i64 100, i32 1, i64 101)
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr

// CHECK: %[[V6:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V5]], %{{.*}} ]
// CHECK: store ptr %[[V6]], ptr %[[T]], align 8

void test_conversion1(S0 *p) {
  S1 *t = (S1 *)p;
}

// CHECK-LABEL: define void @test_conversion2(
// CHECK: %[[P_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[P_ADDR]], align 8
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[T]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V1]], i64 1000)

// CHECK: %[[V4:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V4]], i32 1, i64 100, i32 1, i64 %[[V2]])
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr

// CHECK: %[[V7:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V6]], %{{.*}} ]
// CHECK: store ptr %[[V7]], ptr %[[T]], align 8
// CHECK: %[[V8:.*]] = load ptr, ptr %[[T]], align 8
// CHECK: %[[V9:.*]] = ptrtoint ptr %[[T]] to i64
// CHECK: %[[V10:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V9]], i64 1000)

// CHECK: %[[V12:.*]] = ptrtoint ptr %[[V8]] to i64
// CHECK: %[[V13:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V12]], i32 1, i64 %[[V10]], i32 1, i64 100)
// CHECK: %[[V14:.*]] = inttoptr i64 %[[V13]] to ptr

// CHECK: %[[V15:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V14]], %{{.*}} ]
// CHECK: store ptr %[[V15]], ptr %{{.*}}, align 8

void test_conversion2(S0 *p) {
  S0 *__ptrauth(1, 1, 1000) t = p;
  S0 *t2 = t;
}

// CHECK-LABEL: define void @test_conversion3(
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8

// CHECK: %[[V3:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V3]], i32 1, i64 102, i32 1, i64 100)
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr

// CHECK: %[[V6:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V5]], %{{.*}} ]
// CHECK: store ptr %[[V6]], ptr %[[T]], align 8

void test_conversion3(S2 *s) {
  S0 *t = (S0 *)s;
}

// CHECK-LABEL: define void @test_conversion4(
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8

// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 100)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr

// CHECK: %[[V5:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V4]], %{{.*}} ]
// CHECK: %[[V6:.*]] = ptrtoint ptr %[[V5]] to i64
// CHECK: store i64 %[[V6]], ptr %{{.*}}, align 8

void test_conversion4(S0 *s) {
  intptr_t i = (intptr_t)s;
}

// CHECK-LABEL: define void @test_conversion5(
// CHECK: %[[I_ADDR:.*]] = alloca i64, align 8
// CHECK: %[[V0:.*]] = load i64, ptr %[[I_ADDR]], align 8
// CHECK: %[[V1:.*]] = inttoptr i64 %[[V0]] to ptr

// CHECK: %[[V3:.*]] = ptrtoint ptr %[[V1]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V3]], i32 1, i64 100)
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr

// CHECK: %[[V6:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V5]], %{{.*}} ]
// CHECK: store ptr %[[V6]], ptr %{{.*}}, align 8

void test_conversion5(intptr_t i) {
  S0 *s = (S0 *)i;
}

// CHECK: define i32 @test_comparison0(ptr %[[S:.*]])
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @g0 to i64), i32 1, i64 100)
// CHECK: %[[V2:.*]] = inttoptr i64 %[[V1]] to ptr
// CHECK: icmp eq ptr %[[V0]], %[[V2]]

int test_comparison0(S0 *s) {
  if (s == &g0)
    return 1;
  return 2;
}

// CHECK-LABEL: define void @test_call0()
// CHECK: %[[T:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[P:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[T]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V0]], i32 1, i64 100)
// CHECK: %[[V2:.*]] = inttoptr i64 %[[V1]] to ptr
// CHECK: store ptr %[[V2]], ptr %[[P]], align 8
// CHECK: %[[V3:.*]] = load ptr, ptr %[[P]], align 8
// CHECK: call ptr @func0(ptr %[[V3]])

void test_call0(void) {
  S0 t;
  S0 *p = &t;
  func0(p);
}

// CHECK-LABEL: define void @test_call1()
// CHECK: %[[T0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1:.*]] = alloca ptr, align 8
// CHECK: %[[T2:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V0]], i32 1, i64 100)
// CHECK: %[[V2:.*]] = inttoptr i64 %[[V1]] to ptr
// CHECK: %[[V3:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V3]], i64 1000)
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[V2]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V5]], i32 1, i64 100, i32 1, i64 %[[V4]])
// CHECK: %[[V7:.*]] = inttoptr i64 %[[V6]] to ptr
// CHECK: store ptr %[[V7]], ptr %[[T1]], align 8
// CHECK: %[[V8:.*]] = load ptr, ptr %[[T1]], align 8
// CHECK: %[[V9:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V10:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V9]], i64 1000)

// CHECK: %[[V12:.*]] = ptrtoint ptr %[[V8]] to i64
// CHECK: %[[V13:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V12]], i32 1, i64 %[[V10]], i32 1, i64 100)
// CHECK: %[[V14:.*]] = inttoptr i64 %[[V13]] to ptr

// CHECK: %[[V15:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V14]], %{{.*}} ]
// CHECK-NEXT: %[[CALL:.*]] = call ptr @func0(ptr %[[V15]])
// CHECK-NEXT: store ptr %[[CALL]], ptr %[[T2]], align 8
// CHECK-NEXT: ret void

void test_call1(void) {
  S0 t0;
  S0 *__ptrauth(1, 1, 1000) t1 = &t0;
  S0 *t2 = func0(t1);
}

// CHECK-LABEL: define void @test_call2()
// CHECK: %[[P:.*]] = alloca ptr, align 8
// CHECK: %[[CALL:.*]] = call ptr @func0(ptr null)
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[P]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V0:.*]], i64 1000)

// CHECK: %[[V3:.*]] = ptrtoint ptr %[[CALL]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V3]], i32 1, i64 100, i32 1, i64 %[[V1]])
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr

// CHECK: %[[V6:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V5]], %{{.*}} ]
// CHECK: store ptr %[[V6]], ptr %[[P]], align 8

void test_call2(void) {
  S0 *__ptrauth(1, 1, 1000) p = func0(0);
}

// CHECK-LABEL: define i64 @test_call3(
// CHECK-NOT: @llvm.ptrauth

S0 test_call3(S0 a) {
  S0 t = a;
  S0 t1 = func1(t);
  S0 t2 = t1;
  return t2;
}

// NOTE: The aggregate temporary created to pass 't' to 'func2' isn't
//       destructed. This is a pre-existing bug.

// FIXME: Shouldn't pass raw pointers to non-trivial C struct special functions.

// CHECK: define void @test_call4(ptr dead_on_unwind noalias writable sret(%struct.S4) align 16 %[[AGG_RESULT:.*]], ptr %[[A:.*]])
// CHECK: %[[T:.*]] = alloca %[[STRUCT_S4]], align 16
// CHECK: %[[T1:.*]] = alloca %[[STRUCT_S4]], align 16
// CHECK: %[[AGG_TMP:.*]] = alloca %[[STRUCT_S4]], align 16
// CHECK: %[[NRVO:.*]] = alloca i1, align 1
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[A]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V0]], i32 1, i64 104)
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[A]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 104)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: call void @__copy_constructor_16_16_t0w8_w8_t16w16(ptr %[[T]], ptr %[[V4]])
// CHECK: call void @__copy_constructor_16_16_t0w8_w8_t16w16(ptr %[[AGG_TMP]], ptr %[[T]])
// CHECK: %[[V7:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V8:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V7]], i32 1, i64 104)
// CHECK: %[[V9:.*]] = inttoptr i64 %[[V8]] to ptr
// CHECK: %[[V10:.*]] = ptrtoint ptr %[[AGG_TMP]] to i64
// CHECK: %[[V11:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V10]], i32 1, i64 104)
// CHECK: %[[V12:.*]] = inttoptr i64 %[[V11]] to ptr
// CHECK: call void @func2(ptr dead_on_unwind writable sret(%struct.S4) align 16 %[[V9]], ptr %[[V12]])
// CHECK: %[[V12:.*]] = ptrtoint ptr %[[AGG_RESULT]] to i64
// CHECK: %[[V13:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V12]], i32 1, i64 104)
// CHECK: %[[V14:.*]] = inttoptr i64 %[[V13]] to ptr
// CHECK: store i1 false, ptr %[[NRVO]], align 1
// CHECK: %[[V15:.*]] = ptrtoint ptr %[[AGG_RESULT]] to i64
// CHECK: %[[V16:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V15]], i32 1, i64 104)
// CHECK: %[[V17:.*]] = inttoptr i64 %[[V16]] to ptr
// CHECK: call void @__copy_constructor_16_16_t0w8_w8_t16w16(ptr %[[V17]], ptr %[[T1]])

// CHECK: call void @__destructor_16_w8(ptr %[[T1]])
// CHECK: call void @__destructor_16_w8(ptr %[[T]])
// CHECK: %[[V25:.*]] = ptrtoint ptr %[[A]] to i64
// CHECK: %[[V26:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V25]], i32 1, i64 104)
// CHECK: %[[V27:.*]] = inttoptr i64 %[[V26]] to ptr
// CHECK: call void @__destructor_16_w8(ptr %[[V27]])

S4 test_call4(S4 a) {
  S4 t = a;
  S4 t1 = func2(t);
  S4 t2 = t1;
  return t2;
}

// CHECK: define ptr @test_return0(ptr %[[P:.*]])
// CHECK: %[[P_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[P_ADDR]], align 8
// CHECK: %[[V2:.*]] = icmp ne ptr %[[V0]], null

// CHECK: %[[V3:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V3]], i32 1, i64 100)
// CHECK: %[[V5:.*]] = inttoptr i64 %[[V4]] to ptr

// CHECK: %[[V6:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V5]], %{{.*}} ]
// CHECK: ret ptr %[[V6]]

S0 *test_return0(void *p) {
  return (S0 *)p;
}

// CHECK-LABEL: define void @test_assignment0(
// CHECK-NOT: call {{.*}}ptrauth

void test_assignment0(S0 *s) {
  S0 *t = s;
}

// CHECK-LABEL: define void @test_compound_literal0(
// CHECK: %[[T0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1]] = alloca i32, align 4
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 4 %[[T0]], ptr align 4 @[[CONST_TEST_COMPOUND_LITERAL0_T0]], i64 8, i1 false)
// CHECK: %[[A0:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[T0]], i32 0, i32 0
// CHECK: %[[V1:.*]] = load i32, ptr %[[A0]], align 4
// CHECK: %[[A1:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[T0]], i32 0, i32 1
// CHECK: %[[V2:.*]] = load i32, ptr %[[A1]], align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V1]], %[[V2]]

void test_compound_literal0() {
  S0 t0 = (S0){1, 2};
  int t1 = t0.a0 + t0.a1;
}

// CHECK-LABEL: define void @test_compound_literal1(
// CHECK: %[[T0:.*]] = alloca ptr, align 8
// CHECK: %[[_COMPOUNDLITERAL:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[_COMPOUNDLITERAL]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V0]], i32 1, i64 100)
// CHECK: %[[V2:.*]] = inttoptr i64 %[[V1]] to ptr
// CHECK: store ptr %[[V2]], ptr %[[T0]], align 8
// CHECK: %[[V3:.*]] = load ptr, ptr %[[T0]], align 8
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[V3]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V4]], i32 1, i64 100)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V6]], i64 0
// CHECK: %[[V9:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4
// CHECK: %[[V10:.*]] = load ptr, ptr %[[T0]], align 8
// CHECK: %[[V11:.*]] = ptrtoint ptr %[[V10]] to i64
// CHECK: %[[V12:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V11]], i32 1, i64 100)
// CHECK: %[[V13:.*]] = inttoptr i64 %[[V12]] to ptr
// CHECK: %[[RESIGNEDGEP1:.*]] = getelementptr i8, ptr %[[V13]], i64 4
// CHECK: %[[V16:.*]] = load i32, ptr %[[RESIGNEDGEP1]], align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V9]], %[[V16]]

void test_compound_literal1() {
  S0 *t0 = &(S0){1, 2};
  int t1 = t0->a0 + t0->a1;
}

// CHECK: define void @test_block0(ptr %[[S:.*]])
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[BLOCK:.*]] = alloca <{ ptr, i32, i32, ptr, ptr, ptr, %[[STRUCT_S0]] }>, align 8
// CHECK: %[[BLOCK_CAPTURED:.*]] = getelementptr inbounds <{ ptr, i32, i32, ptr, ptr, ptr, %[[STRUCT_S0]] }>, ptr %[[BLOCK]], i32 0, i32 5
// CHECK: %[[V4:.*]] = load ptr, ptr %[[S_ADDR]], align 8
// CHECK: store ptr %[[V4]], ptr %[[BLOCK_CAPTURED]], align 8
// CHECK: %[[BLOCK_CAPTURED1:.*]] = getelementptr inbounds <{ ptr, i32, i32, ptr, ptr, ptr, %[[STRUCT_S0]] }>, ptr %[[BLOCK]], i32 0, i32 6
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %[[BLOCK_CAPTURED1]], ptr align 4 %[[T0]], i64 8, i1 false)

// CHECK: define internal i32 @__test_block0_block_invoke(ptr %[[_BLOCK_DESCRIPTOR:.*]])
// CHECK: %[[BLOCK_CAPTURE_ADDR:.*]] = getelementptr inbounds <{ ptr, i32, i32, ptr, ptr, ptr, %[[STRUCT_S0]] }>, ptr %[[_BLOCK_DESCRIPTOR]], i32 0, i32 5
// CHECK: %[[V0:.*]] = load ptr, ptr %[[BLOCK_CAPTURE_ADDR]], align 8
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V3]], i64 4
// CHECK: %[[V6:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4
// CHECK: %[[BLOCK_CAPTURE_ADDR1:.*]] = getelementptr inbounds <{ ptr, i32, i32, ptr, ptr, ptr, %[[STRUCT_S0]] }>, ptr %[[_BLOCK_DESCRIPTOR]], i32 0, i32 6
// CHECK: %[[A1:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[BLOCK_CAPTURE_ADDR1]], i32 0, i32 1
// CHECK: %[[V7:.*]] = load i32, ptr %[[A1]], align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V6]], %[[V7]]
// CHECK: ret i32 %[[ADD]]

void test_block0(S0 *s) {
  S0 t0 = {1, 2};
  int t1 = ^{
    return s->a1 + t0.a1;
  }();
}

// CHECK: define ptr @test_atomic0(
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[P:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: store ptr %[[V0]], ptr %[[P]], align 8
// CHECK: %[[ATOMIC_LOAD:.*]] = load atomic ptr, ptr %[[P]] seq_cst, align 8

// CHECK: %[[V1:.*]] = phi ptr [ %[[ATOMIC_LOAD]], %{{.*}} ], [ %[[V16:.*]], %{{.*}} ]
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V1]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 100)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: %[[INCDEC_PTR:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[V4]], i32 1
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[INCDEC_PTR]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V5]], i32 1, i64 100)
// CHECK: %[[V7:.*]] = inttoptr i64 %[[V6]] to ptr
// CHECK: %[[V8:.*]] = cmpxchg ptr %[[P]], ptr %[[V1]], ptr %[[V7]] seq_cst seq_cst
// CHECK: %[[V9:.*]] = extractvalue { ptr, i1 } %[[V8]], 0
// CHECK: %[[V10:.*]] = extractvalue { ptr, i1 } %[[V8]], 1

// CHECK: ret ptr %[[V7]]

S0 *test_atomic0(S0 *a) {
  S0 * _Atomic p = a;
  return ++p;
}

// CHECK: define ptr @test_atomic1(
// CHECK: alloca ptr, align 8
// CHECK: %[[P:.*]] = alloca ptr, align 8
// CHECK: %[[ATOMIC_LOAD:.*]] = load atomic ptr, ptr %[[P]] seq_cst, align 8

// CHECK: ret ptr %[[ATOMIC_LOAD]]

S0 *test_atomic1(S0 *a) {
  S0 * _Atomic p = a;
  return p++;
}

// CHECK-LABEL: define i32 @test_address_space0(
// CHECK: %[[V1:.*]] = ptrtoint ptr addrspace(1) %{{.*}} to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr addrspace(1)
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr addrspace(1) %[[V3]], i64 4
// CHECK: load i32, ptr addrspace(1) %[[RESIGNEDGEP]], align 4

int test_address_space0(__attribute__((address_space(1))) S0 *s) {
  return s->a1;
}

// CHECK: define i32 @test_attr_annotate(ptr %[[S:.*]])
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[S]], ptr %[[S_ADDR]], align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8
// CHECK: %[[V1:.*]] = icmp ne ptr %[[V0]], null
// CHECK: br i1 %[[V1]], label %[[RESIGN_NONNULL:.*]], label %[[RESIGN_CONT:.*]]

// CHECK: [[RESIGN_NONNULL]]:
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 103)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: br label %[[RESIGN_CONT]]

// CHECK: [[RESIGN_CONT]]:
// CHECK: %[[V5:.*]] = phi ptr [ null, %{{.*}} ], [ %[[V4]], %[[RESIGN_NONNULL]] ]
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V5]], i64 4
// CHECK: %[[V9:.*]] = call ptr @llvm.ptr.annotation.p0.p0(ptr %[[RESIGNEDGEP]],
// CHECK: %[[A1:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[V9]], i32 0, i32 1
// CHECK: load i32, ptr %[[A1]], align 4

int test_attr_annotate(S3 *s) {
  return s->f1.a1;
}

// CHECK-LABEL: define void @test_ext_vector0(
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 104)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V3]], i64 24
// CHECK: store float 3.000000e+00, ptr %[[RESIGNEDGEP]], align 8

void test_ext_vector0(S4 *s) {
  s->extv0.hi[0] = 3.0;
}
