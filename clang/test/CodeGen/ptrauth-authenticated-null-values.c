// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-calls -fptrauth-intrinsics -emit-llvm %s -O0 -o - | FileCheck %s

typedef void *__ptrauth(2, 0, 0, "authenticates-null-values") authenticated_null;
typedef void *__ptrauth(2, 1, 0, "authenticates-null-values") authenticated_null_addr_disc;
typedef void *__ptrauth(2, 0, 0) unauthenticated_null;
typedef void *__ptrauth(2, 1, 0) unauthenticated_null_addr_disc;

int test_global;

// CHECK: define void @f0(ptr noundef [[AUTH1_ARG:%.*]], ptr noundef [[AUTH2_ARG:%.*]])
void f0(authenticated_null *auth1, authenticated_null *auth2) {
  *auth1 = *auth2;
  // CHECK: [[AUTH1_ARG]].addr = alloca ptr
  // CHECK: [[AUTH2_ARG]].addr = alloca ptr
  // CHECK: store ptr [[AUTH1_ARG]], ptr [[AUTH1_ARG]].addr
  // CHECK: store ptr [[AUTH2_ARG]], ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH1_ADDR:%.*]] = load ptr, ptr [[AUTH1_ARG]].addr
  // CHECK: [[AUTH2_ADDR:%.*]] = load ptr, ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH2_VALUE:%.*]] = load ptr, ptr [[AUTH2_ADDR]]
  // CHECK: store ptr [[AUTH2_VALUE]], ptr [[AUTH1_ADDR]]
}

// CHECK: define void @f1(ptr noundef [[AUTH1_ARG:%.*]], ptr noundef [[AUTH2_ARG:%.*]])
void f1(unauthenticated_null *auth1, authenticated_null *auth2) {
  *auth1 = *auth2;
  // CHECK: [[ENTRY:.*]]:
  // CHECK: [[AUTH1_ARG]].addr = alloca ptr
  // CHECK: [[AUTH2_ARG]].addr = alloca ptr
  // CHECK: store ptr [[AUTH1_ARG]], ptr [[AUTH1_ARG]].addr
  // CHECK: store ptr [[AUTH2_ARG]], ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH1_ADDR:%.*]] = load ptr, ptr [[AUTH1_ARG]].addr
  // CHECK: [[AUTH2_ADDR:%.*]] = load ptr, ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH2:%.*]] = load ptr, ptr [[AUTH2_ADDR]]
  // CHECK: [[CAST_VALUE:%.*]] = ptrtoint ptr %2 to i64
  // CHECK: [[AUTHED_VALUE:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[CAST_VALUE]], i32 2, i64 0)
  // CHECK: [[TRUE_VALUE:%.*]] = inttoptr i64 [[AUTHED_VALUE]] to ptr
  // CHECK: [[COMPARISON:%.*]] = icmp ne ptr [[TRUE_VALUE]], null
  // CHECK: br i1 [[COMPARISON]], label %resign.nonnull, label %resign.cont
  // CHECK: resign.nonnull:
  // CHECK: %7 = ptrtoint ptr %2 to i64
  // CHECK: %8 = call i64 @llvm.ptrauth.resign(i64 %7, i32 2, i64 0, i32 2, i64 0)
  // CHECK: %9 = inttoptr i64 %8 to ptr
  // CHECK: br label %resign.cont
  // CHECK: resign.cont:
  // CHECK: [[RESULT:%.*]] = phi ptr [ null, %entry ], [ %9, %resign.nonnull ]
  // CHECK: store ptr [[RESULT]], ptr [[AUTH1_ADDR]]
}

// CHECK: define void @f2(ptr noundef [[AUTH1_ARG:%.*]], ptr noundef [[AUTH2_ARG:%.*]])
void f2(authenticated_null *auth1, unauthenticated_null *auth2) {
  *auth1 = *auth2;
  // CHECK: [[ENTRY:.*]]:
  // CHECK: [[AUTH1_ARG]].addr = alloca ptr
  // CHECK: [[AUTH2_ARG]].addr = alloca ptr
  // CHECK: store ptr [[AUTH1_ARG]], ptr [[AUTH1_ARG]].addr
  // CHECK: store ptr [[AUTH2_ARG]], ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH1_ADDR:%.*]] = load ptr, ptr [[AUTH1_ARG]].addr
  // CHECK: [[AUTH2_ADDR:%.*]] = load ptr, ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH2:%.*]] = load ptr, ptr [[AUTH2_ADDR]]
  // CHECK: [[COMPARE:%.*]] = icmp ne ptr [[AUTH2]], null
  // CHECK: br i1 [[COMPARE]], label %[[NON_NULL:resign.*]], label %[[NULL:resign.*]]
  // CHECK: [[NULL]]:
  // CHECK: [[SIGNED_ZERO:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[SIGNED_NULL:%.*]] = inttoptr i64 [[SIGNED_ZERO]] to ptr
  // CHECK: br label %[[CONT:resign.*]]
  // CHECK: [[NON_NULL]]:
  // CHECK: [[AUTH2_CAST:%.*]] = ptrtoint ptr [[AUTH2]] to i64
  // CHECK: [[AUTH2_AUTHED:%.*]] = call i64 @llvm.ptrauth.resign(i64 [[AUTH2_CAST]], i32 2, i64 0, i32 2, i64 0)
  // CHECK: [[AUTH2:%.*]] = inttoptr i64 [[AUTH2_AUTHED]] to ptr
  // CHECK: br label %[[CONT]]

  // CHECK: [[CONT]]:
  // CHECK: [[RESULT:%.*]] = phi ptr [ [[SIGNED_NULL]], %[[NULL]] ], [ [[AUTH2]], %[[NON_NULL]] ]
  // CHECK: store ptr [[RESULT]], ptr [[AUTH1_ADDR]]
}

// CHECK: define void @f3(ptr noundef [[AUTH1:%.*]], ptr noundef [[I:%.*]])
void f3(authenticated_null *auth1, void *i) {
  *auth1 = i;
  // CHECK: [[AUTH1_ADDR:%.*]] = alloca ptr
  // CHECK: [[I_ADDR:%.*]] = alloca ptr
  // CHECK: store ptr [[AUTH1]], ptr [[AUTH1_ADDR]]
  // CHECK: store ptr [[I]], ptr [[I_ADDR]]
  // CHECK: [[AUTH1:%.*]] = load ptr, ptr [[AUTH1_ADDR]]
  // CHECK: [[I:%.*]] = load ptr, ptr [[I_ADDR]]
  // CHECK: [[CAST_I:%.*]] = ptrtoint ptr [[I]] to i64
  // CHECK: [[SIGNED_CAST_I:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[CAST_I]], i32 2, i64 0)
  // CHECK: [[SIGNED_I:%.*]] = inttoptr i64 [[SIGNED_CAST_I]] to ptr
  // CHECK: store ptr [[SIGNED_I]], ptr [[AUTH1]]
}

// CHECK: define void @f4(ptr noundef [[AUTH1:%.*]])
void f4(authenticated_null *auth1) {
  *auth1 = 0;
  // CHECK: [[AUTH1_ADDR:%.*]] = alloca ptr
  // CHECK: store ptr [[AUTH1]], ptr [[AUTH1_ADDR]]
  // CHECK: [[AUTH1:%.*]] = load ptr, ptr [[AUTH1_ADDR]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[RESULT:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[RESULT]], ptr [[AUTH1]]
}

// CHECK: define void @f5(ptr noundef [[AUTH1:%.*]])
void f5(authenticated_null *auth1) {
  *auth1 = &test_global;
  // CHECK: [[AUTH1_ADDR:%.*]] = alloca ptr
  // CHECK: store ptr [[AUTH1]], ptr [[AUTH1_ADDR]]
  // CHECK: [[AUTH1:%.*]] = load ptr, ptr [[AUTH1_ADDR]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @test_global to i64), i32 2, i64 0)
  // CHECK: [[RESULT:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[RESULT]], ptr [[AUTH1]]
}

// CHECK: define i32 @f6(ptr noundef [[AUTH1:%.*]])
int f6(authenticated_null *auth1) {
  return !!*auth1;
  // CHECK: [[AUTH1_ADDR:%.*]] = alloca ptr
  // CHECK: store ptr [[AUTH1]], ptr [[AUTH1_ADDR]]
  // CHECK: [[AUTH1:%.*]] = load ptr, ptr [[AUTH1_ADDR]]
  // CHECK: [[AUTH1_V:%.*]] = load ptr, ptr [[AUTH1]]
  // CHECK: [[CAST_AUTH1:%.*]] = ptrtoint ptr [[AUTH1_V]] to i64
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[CAST_AUTH1]], i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[AUTHED]] to ptr
  // CHECK: [[TOBOOL:%.*]] = icmp ne ptr [[CAST]], null
  // CHECK: [[LNOT:%.*]] = xor i1 [[TOBOOL]], true
  // CHECK: [[LNOT1:%.*]] = xor i1 [[LNOT]], true
  // CHECK: [[LNOT_EXT:%.*]] = zext i1 [[LNOT1]] to i32
  // CHECK: ret i32 [[LNOT_EXT]]
}

// CHECK: define void @f7(ptr noundef [[AUTH1_ARG:%.*]], ptr noundef [[AUTH2_ARG:%.*]])
void f7(authenticated_null_addr_disc *auth1, authenticated_null_addr_disc *auth2) {
  *auth1 = *auth2;
  // CHECK: [[AUTH1_ARG]].addr = alloca ptr
  // CHECK: [[AUTH2_ARG]].addr = alloca ptr
  // CHECK: store ptr [[AUTH1_ARG]], ptr [[AUTH1_ARG]].addr
  // CHECK: store ptr [[AUTH2_ARG]], ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH1_ADDR:%.*]] = load ptr, ptr [[AUTH1_ARG]].addr
  // CHECK: [[AUTH2_ADDR:%.*]] = load ptr, ptr [[AUTH2_ARG]].addr
  // CHECK: [[AUTH2_VALUE:%.*]] = load ptr, ptr [[AUTH2_ADDR]]
  // CHECK: [[CAST_AUTH2_ADDR:%.*]] = ptrtoint ptr [[AUTH2_ADDR]] to i64
  // CHECK: [[CAST_AUTH1_ADDR:%.*]] = ptrtoint ptr [[AUTH1_ADDR]] to i64
  // CHECK: [[CAST_AUTH2:%.*]] = ptrtoint ptr [[AUTH2_VALUE]] to i64
  // CHECK: [[RESIGNED:%.*]] = call i64 @llvm.ptrauth.resign(i64 [[CAST_AUTH2]], i32 2, i64 [[CAST_AUTH2_ADDR]], i32 2, i64 [[CAST_AUTH1_ADDR]])
  // CHECK: [[CAST_RESIGNED_VALUE:%.*]] = inttoptr i64 [[RESIGNED]] to ptr
  // CHECK: store ptr [[CAST_RESIGNED_VALUE]], ptr [[AUTH1_ADDR]]
}

struct S0 {
  int i;
  authenticated_null p;
};

void f8() {
  struct S0 t = {.i = 1, .p = 0};
  // CHECK: define void @f8()
  // CHECK: [[T:%.*]] = alloca %struct.S0
  // CHECK: [[I:%.*]] = getelementptr inbounds %struct.S0, ptr [[T]], i32 0, i32 0
  // CHECK: store i32 1, ptr [[I]]
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S0, ptr [[T]], i32 0, i32 1
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
}

void f9() {
  struct S0 t = {};
  // CHECK: define void @f9()
  // CHECK: [[T:%.*]] = alloca %struct.S0
  // CHECK: [[I:%.*]] = getelementptr inbounds %struct.S0, ptr [[T]], i32 0, i32 0
  // CHECK: store i32 0, ptr [[I]]
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S0, ptr [[T]], i32 0, i32 1
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
}

void f10() {
  struct S0 t = {.i = 12};
  // CHECK: define void @f10()
  // CHECK: [[T:%.*]] = alloca %struct.S0
  // CHECK: [[I:%.*]] = getelementptr inbounds %struct.S0, ptr [[T]], i32 0, i32 0
  // CHECK: store i32 12, ptr [[I]]
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S0, ptr [[T]], i32 0, i32 1
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
}

struct S1 {
  authenticated_null p;
  authenticated_null_addr_disc q;
};

void f11() {
  struct S1 t = {.p = (void *)1};
  // CHECK-LABEL: define void @f11()
  // CHECK: [[T:%.*]] = alloca %struct.S1
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S1, ptr [[T]], i32 0, i32 0
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 1, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
  // CHECK: [[Q:%.*]] = getelementptr inbounds %struct.S1, ptr [[T]], i32 0, i32 1
  // CHECK: [[ADDR_DISC:%.*]] = ptrtoint ptr [[Q]] to i64
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 [[ADDR_DISC]])
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[Q]]
}

void f12() {
  struct S1 t = {.p = (void *)1, .q = (void *)0};
  // CHECK-LABEL: define void @f12()
  // CHECK: [[T:%.*]] = alloca %struct.S1
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S1, ptr [[T]], i32 0, i32 0
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 1, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
  // CHECK: [[Q:%.*]] = getelementptr inbounds %struct.S1, ptr [[T]], i32 0, i32 1
  // CHECK: [[ADDR_DISC:%.*]] = ptrtoint ptr [[Q]] to i64
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 [[ADDR_DISC]])
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[Q]]
}
void f13() {
  struct S1 t = {.q = (void *)1};
  // CHECK: define void @f13()
  // CHECK: [[T:%.*]] = alloca %struct.S1
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S1, ptr [[T]], i32 0, i32 0
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
  // CHECK: [[Q:%.*]] = getelementptr inbounds %struct.S1, ptr [[T]], i32 0, i32 1
  // CHECK: [[ADDR_DISC:%.*]] = ptrtoint ptr [[Q]] to i64
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 1, i32 2, i64 [[ADDR_DISC]])
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[Q]]
}

struct S2 {
  int i;
  struct S1 s1;
};

void f14() {
  struct S2 t = {};
  // CHECK-LABEL: define void @f14
  // CHECK: [[T:%.*]] = alloca %struct.S2
  // CHECK: [[I:%.*]] = getelementptr inbounds %struct.S2, ptr [[T]], i32 0, i32 0
  // CHECK: store i32 0, ptr [[I]]
  // CHECK: [[S1:%.*]] = getelementptr inbounds %struct.S2, ptr [[T]], i32 0, i32 1
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S1, ptr [[S1]], i32 0, i32 0
  // CHECK: [[SIGNED_INT:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[SIGNED:%.*]] = inttoptr i64 [[SIGNED_INT]] to ptr
  // CHECK: store ptr [[SIGNED]], ptr [[P]]
  // CHECK: [[Q:%.*]] = getelementptr inbounds %struct.S1, ptr [[S1]], i32 0, i32 1
  // CHECK: [[ADDR_DISC:%.*]] = ptrtoint ptr [[Q]] to i64
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 [[ADDR_DISC]])
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[Q]]
}

void f15() {
  struct S2 t = {.s1 = {}};
  // CHECK-LABEL: define void @f15
  // CHECK: [[T:%.*]] = alloca %struct.S2
  // CHECK: [[I:%.*]] = getelementptr inbounds %struct.S2, ptr [[T]], i32 0, i32 0
  // CHECK: store i32 0, ptr [[I]]
  // CHECK: [[S1:%.*]] = getelementptr inbounds %struct.S2, ptr [[T]], i32 0, i32 1
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S1, ptr [[S1]], i32 0, i32 0
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
  // CHECK: [[Q:%.*]] = getelementptr inbounds %struct.S1, ptr [[S1]], i32 0, i32 1
  // CHECK: [[ADDR_DISC:%.*]] = ptrtoint ptr [[Q]] to i64
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 [[ADDR_DISC]])
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[Q]]
}

void f16() {
  struct S2 t = {.i = 13};
  // CHECK-LABEL: define void @f16
  // CHECK: [[T:%.*]] = alloca %struct.S2
  // CHECK: [[I:%.*]] = getelementptr inbounds %struct.S2, ptr [[T]], i32 0, i32 0
  // CHECK: store i32 13, ptr [[I]]
  // CHECK: [[S1:%.*]] = getelementptr inbounds %struct.S2, ptr [[T]], i32 0, i32 1
  // CHECK: [[P:%.*]] = getelementptr inbounds %struct.S1, ptr [[S1]], i32 0, i32 0
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[P]]
  // CHECK: [[Q:%.*]] = getelementptr inbounds %struct.S1, ptr [[S1]], i32 0, i32 1
  // CHECK: [[ADDR_DISC:%.*]] = ptrtoint ptr [[Q]] to i64
  // CHECK: [[SIGN:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 [[ADDR_DISC]])
  // CHECK: [[CAST:%.*]] = inttoptr i64 [[SIGN]] to ptr
  // CHECK: store ptr [[CAST]], ptr [[Q]]
}

void f17(struct S2 a, struct S2 b) {
  a = b;
  // CHECK-LABEL: define void @f17
  // CHECK: call void @__copy_assignment_8_8_t0w4_S_t8w8_pa2_0_anv_16

  // CHECK-LABEL: define linkonce_odr hidden void @__copy_assignment_8_8_t0w4_S_t8w8_pa2_0_anv_16
  // CHECK: %dst.addr = alloca ptr
  // CHECK: %src.addr = alloca ptr
  // CHECK: store ptr %dst, ptr %dst.addr
  // CHECK: store ptr %src, ptr %src.addr
  // CHECK: %0 = load ptr, ptr %dst.addr
  // CHECK: %1 = load ptr, ptr %src.addr
  // CHECK: %2 = load i32, ptr %1
  // CHECK: store i32 %2, ptr %0
  // CHECK: %3 = getelementptr inbounds i8, ptr %0, i64 8
  // CHECK: %4 = getelementptr inbounds i8, ptr %1, i64 8
  // CHECK: call void @__copy_assignment_8_8_t0w8_pa2_0_anv_8

  // CHECK-LABEL: define linkonce_odr hidden void @__copy_assignment_8_8_t0w8_pa2_0_anv_8
  // CHECK: %dst.addr = alloca ptr
  // CHECK: %src.addr = alloca ptr
  // CHECK: store ptr %dst, ptr %dst.addr
  // CHECK: store ptr %src, ptr %src.addr
  // CHECK: %0 = load ptr, ptr %dst.addr
  // CHECK: %1 = load ptr, ptr %src.addr
  // CHECK: %2 = load i64, ptr %1
  // CHECK: store i64 %2, ptr %0
  // CHECK: %3 = getelementptr inbounds i8, ptr %0, i64 8
  // CHECK: %4 = getelementptr inbounds i8, ptr %1, i64 8
  // CHECK: %5 = load ptr, ptr %4
  // CHECK: %6 = ptrtoint ptr %4 to i64
  // CHECK: %7 = ptrtoint ptr %3 to i64
  // CHECK: %8 = ptrtoint ptr %5 to i64
  // CHECK: %9 = call i64 @llvm.ptrauth.resign(i64 %8, i32 2, i64 %6, i32 2, i64 %7)
  // CHECK: %10 = inttoptr i64 %9 to ptr
  // CHECK: store ptr %10, ptr %3
}

struct S1_nonull_auth {
  unauthenticated_null p;
  unauthenticated_null_addr_disc q;
};

void f18(struct S1_nonull_auth a, struct S1_nonull_auth b) {
  a = b;
  // All we're doing here is making sure that we don't coalesce
  // copy functions for pointer authenticated types that only
  // differ in the handling of null values
  // CHECK-LABEL: define void @f18
  // CHECK: call void @__copy_assignment_8_8_t0w8_pa2_0_8
}

void f19() {
  authenticated_null_addr_disc addr[5] = {0};

  // CHECK-LABEL: define void @f19
  // CHECK: %addr = alloca [5 x ptr]
  // CHECK: %0 = ptrtoint ptr %addr to i64
  // CHECK: %1 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %0)
  // CHECK: %2 = inttoptr i64 %1 to ptr
  // CHECK: store ptr %2, ptr %addr
  // CHECK: %arrayinit.start = getelementptr inbounds ptr, ptr %addr, i64 1
  // CHECK: %arrayinit.end = getelementptr inbounds ptr, ptr %addr, i64 5
  // CHECK: br label %arrayinit.body
  // CHECK: arrayinit.body:
  // CHECK: %arrayinit.cur = phi ptr [ %arrayinit.start, %entry ], [ %arrayinit.next, %arrayinit.body ]
  // CHECK: %3 = ptrtoint ptr %arrayinit.cur to i64
  // CHECK: %4 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %3)
  // CHECK: %5 = inttoptr i64 %4 to ptr
  // CHECK: store ptr %5, ptr %arrayinit.cur
  // CHECK: %arrayinit.next = getelementptr inbounds ptr, ptr %arrayinit.cur, i64 1
  // CHECK: %arrayinit.done = icmp eq ptr %arrayinit.next, %arrayinit.end
  // CHECK: br i1 %arrayinit.done, label %arrayinit.end1, label %arrayinit.body
  // CHECK: arrayinit.end1:
}

void f21() {
  struct S2 addr[5] = {{}};
}

void f22() {
  struct S2 addr[5] = {0};
}

void f23() {
  struct S1 addr[5] = {};
  // CHECK-LABEL: define void @f23()
  // CHECK: %addr = alloca [5 x %struct.S1]
  // CHECK: arrayinit.end = getelementptr inbounds %struct.S1, ptr %addr, i64 5
  // CHECK: br label %arrayinit.body
  // CHECK: arrayinit.cur = phi ptr [ %addr, %entry ], [ %arrayinit.next, %arrayinit.body ]
  // CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %arrayinit.cur, ptr align 8 @6, i64 16, i1 false)
  // CHECK: 0 = getelementptr inbounds %struct.S1, ptr %arrayinit.cur, i32 0, i32 0
  // CHECK: 1 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: 2 = inttoptr i64 %1 to ptr
  // CHECK: store ptr %2, ptr %0
  // CHECK: 3 = getelementptr inbounds %struct.S1, ptr %arrayinit.cur, i32 0, i32 1
  // CHECK: 4 = ptrtoint ptr %3 to i64
  // CHECK: 5 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %4)
  // CHECK: 6 = inttoptr i64 %5 to ptr
  // CHECK: store ptr %6, ptr %3
  // CHECK: arrayinit.next = getelementptr inbounds %struct.S1, ptr %arrayinit.cur, i64 1
  // CHECK: arrayinit.done = icmp eq ptr %arrayinit.next, %arrayinit.end
  // CHECK: br i1 %arrayinit.done, label %arrayinit.end1, label %arrayinit.body
}

void f24() {
  struct S2 addr[5] = {};
  // CHECK-LABEL: define void @f24()
  // CHECK: %addr = alloca [5 x %struct.S2]
  // CHECK: %arrayinit.end = getelementptr inbounds %struct.S2, ptr %addr, i64 5
  // CHECK: br label %arrayinit.body

  // CHECK: arrayinit.body:
  // CHECK: %arrayinit.cur = phi ptr [ %addr, %entry ], [ %arrayinit.next, %arrayinit.body ]
  // CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %arrayinit.cur, ptr align 8 @7, i64 24, i1 false)
  // CHECK: %0 = getelementptr inbounds %struct.S2, ptr %arrayinit.cur, i32 0, i32 1
  // CHECK: %1 = getelementptr inbounds %struct.S1, ptr %0, i32 0, i32 0
  // CHECK: %2 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: %3 = inttoptr i64 %2 to ptr
  // CHECK: store ptr %3, ptr %1
  // CHECK: %4 = getelementptr inbounds %struct.S1, ptr %0, i32 0, i32 1
  // CHECK: %5 = ptrtoint ptr %4 to i64
  // CHECK: %6 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %5)
  // CHECK: %7 = inttoptr i64 %6 to ptr
  // CHECK: store ptr %7, ptr %4
  // CHECK: %arrayinit.next = getelementptr inbounds %struct.S2, ptr %arrayinit.cur, i64 1
  // CHECK: %arrayinit.done = icmp eq ptr %arrayinit.next, %arrayinit.end
  // CHECK: br i1 %arrayinit.done, label %arrayinit.end1, label %arrayinit.body
  // CHECK: arrayinit.end1:
}

struct S3 {
  int *__ptrauth(2, 0, 0, "authenticates-null-values") f0;
};

void f25() {
  struct S3 s;
  // CHECK-LABEL: define void @f25()
  // CHECK: [[S:%.*]] = alloca %struct.S3
  // CHECK: ret void
  // CHECK: }
}

struct S4 {
  int i;
};
void f26() {
  struct S3 addr[2][3][4] = {};
  // CHECK-LABEL: define void @f26()
  // CHECK: arrayinit.body:
  // CHECK: %arrayinit.cur = phi ptr [ %addr, %entry ], [ %arrayinit.next, %array_authenticated_null_init.end8 ]
  // CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %arrayinit.cur, ptr align 8 @8, i64 96, i1 false)
  // CHECK: %array_authenticated_null_init.start = getelementptr inbounds [3 x [4 x %struct.S3]], ptr %arrayinit.cur, i64 0, i64 0
  // CHECK: %array_authenticated_null_init.end = getelementptr inbounds [4 x %struct.S3], ptr %array_authenticated_null_init.start, i64 3
  // CHECK: br label %array_authenticated_null_init.body
  // CHECK: array_authenticated_null_init.body:
  // CHECK: %array_authenticated_null_init.cur = phi ptr [ %array_authenticated_null_init.start, %arrayinit.body ], [ %array_authenticated_null_init.next6, %array_authenticated_null_init.end5 ]
  // CHECK: %array_authenticated_null_init.start1 = getelementptr inbounds [4 x %struct.S3], ptr %array_authenticated_null_init.cur, i64 0, i64 0
  // CHECK: %array_authenticated_null_init.end2 = getelementptr inbounds %struct.S3, ptr %array_authenticated_null_init.start1, i64 4
  // CHECK: br label %array_authenticated_null_init.body3
  // CHECK: array_authenticated_null_init.body3:
  // CHECK: %array_authenticated_null_init.cur4 = phi ptr [ %array_authenticated_null_init.start1, %array_authenticated_null_init.body ], [ %array_authenticated_null_init.next, %array_authenticated_null_init.body3 ]
  // CHECK: %0 = getelementptr inbounds %struct.S3, ptr %array_authenticated_null_init.cur4, i32 0, i32 0
  // CHECK: %1 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: %2 = inttoptr i64 %1 to ptr
  // CHECK: store ptr %2, ptr %0
  // CHECK: %array_authenticated_null_init.next = getelementptr inbounds %struct.S3, ptr %array_authenticated_null_init.cur4, i64 1
  // CHECK: %array_authenticated_null_init.done = icmp eq ptr %array_authenticated_null_init.next, %array_authenticated_null_init.end2
  // CHECK: br i1 %array_authenticated_null_init.done, label %array_authenticated_null_init.end5, label %array_authenticated_null_init.body3
}
