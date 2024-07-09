// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-calls -fptrauth-intrinsics -emit-llvm -no-enable-noundef-analysis %s -O0 -o - | FileCheck %s

// This is largely a duplicate of CodeGen/ptrauth-authenticated-null-values.c as
// there are C++ specific branches in some struct init and copy implementations
// so we want to be sure that the behaviour is still correct in C++ mode.

typedef void *__ptrauth(2, 0, 0, "authenticates-null-values") authenticated_null;
typedef void *__ptrauth(2, 1, 0, "authenticates-null-values") authenticated_null_addr_disc;
typedef void *__ptrauth(2, 0, 0) unauthenticated_null;

int test_global;

// CHECK: define void @f0(ptr [[AUTH1_ARG:%.*]], ptr [[AUTH2_ARG:%.*]])
extern "C" void f0(authenticated_null *auth1, authenticated_null *auth2) {
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

// CHECK: define void @f1(ptr [[AUTH1_ARG:%.*]], ptr [[AUTH2_ARG:%.*]])
extern "C" void f1(unauthenticated_null *auth1, authenticated_null *auth2) {
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

// CHECK: define void @f2(ptr [[AUTH1_ARG:%.*]], ptr [[AUTH2_ARG:%.*]])
extern "C" void f2(authenticated_null *auth1, unauthenticated_null *auth2) {
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

// CHECK: define void @f3(ptr [[AUTH1:%.*]], ptr [[I:%.*]])
extern "C" void f3(authenticated_null *auth1, void *i) {
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

// CHECK: define void @f4(ptr [[AUTH1:%.*]])
extern "C" void f4(authenticated_null *auth1) {
  *auth1 = 0;
  // CHECK: [[AUTH1_ADDR:%.*]] = alloca ptr
  // CHECK: store ptr [[AUTH1]], ptr [[AUTH1_ADDR]]
  // CHECK: [[AUTH1:%.*]] = load ptr, ptr [[AUTH1_ADDR]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[RESULT:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[RESULT]], ptr [[AUTH1]]
}

// CHECK: define void @f5(ptr [[AUTH1:%.*]])
extern "C" void f5(authenticated_null *auth1) {
  *auth1 = &test_global;
  // CHECK: [[AUTH1_ADDR:%.*]] = alloca ptr
  // CHECK: store ptr [[AUTH1]], ptr [[AUTH1_ADDR]]
  // CHECK: [[AUTH1:%.*]] = load ptr, ptr [[AUTH1_ADDR]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @test_global to i64), i32 2, i64 0)
  // CHECK: [[RESULT:%.*]] = inttoptr i64 [[SIGNED]] to ptr
  // CHECK: store ptr [[RESULT]], ptr [[AUTH1]]
}

// CHECK: define i32 @f6(ptr [[AUTH1:%.*]])
extern "C" int f6(authenticated_null *auth1) {
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

// CHECK: define void @f7(ptr [[AUTH1_ARG:%.*]], ptr [[AUTH2_ARG:%.*]])
extern "C" void f7(authenticated_null_addr_disc *auth1, authenticated_null_addr_disc *auth2) {
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

extern "C" void f8() {
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

extern "C" void f9() {
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

extern "C" void f10() {
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

extern "C" void f11() {
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

extern "C" void f12() {
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

extern "C" void f13() {
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

extern "C" void f14() {
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

extern "C" void f15() {
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

extern "C" void f16() {
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

extern "C" void f17(struct S2 a, struct S2 b) {
  a = b;
  // CHECK-LABEL: define void @f17
  // CHECK: %call = call nonnull align 8 dereferenceable(24) ptr @_ZN2S2aSERKS_(ptr
}

// CHECK-LABEL: define linkonce_odr nonnull align 8 dereferenceable(24) ptr @_ZN2S2aSERKS_
// CHECK: %call = call nonnull align 8 dereferenceable(16) ptr @_ZN2S1aSERKS_(ptr

struct Subclass : S1 {
  int z;
};

extern "C" void f18() {
  Subclass t = Subclass();
  // CHECK-LABEL: define void @f18
  // CHECK: %t = alloca %struct.Subclass
  // CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %t, ptr align 8 @0, i64 24, i1 false)
  // CHECK: [[A:%.*]] = getelementptr inbounds %struct.Subclass, ptr %t, i32 0, i32 0
  // CHECK: [[B:%.*]] = getelementptr inbounds %struct.S1, ptr [[A]], i32 0, i32 0
  // CHECK: [[C:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: [[D:%.*]] = inttoptr i64 [[C]] to ptr
  // CHECK: store ptr [[D]], ptr [[B]]
  // CHECK: [[E:%.*]] = getelementptr inbounds %struct.S1, ptr [[A]], i32 0, i32 1
  // CHECK: [[F:%.*]] = ptrtoint ptr [[E]] to i64
  // CHECK: [[G:%.*]] = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 [[F]])
  // CHECK: [[H:%.*]] = inttoptr i64 [[G]] to ptr
  // CHECK: store ptr [[H]], ptr [[E]]
}

extern "C" void f19() {
  Subclass t = {};
  // CHECK-LABEL: define void @f19
  // CHECK: %t = alloca %struct.Subclass
  // CHECK: %0 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: %1 = inttoptr i64 %0 to ptr
  // CHECK: store ptr %1, ptr %p
  // CHECK: %q = getelementptr inbounds %struct.S1, ptr %t, i32 0, i32 1
  // CHECK: %2 = ptrtoint ptr %q to i64
  // CHECK: %3 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %2)
  // CHECK: %4 = inttoptr i64 %3 to ptr
  // CHECK: store ptr %4, ptr %q, align 8
  // CHECK: %z = getelementptr inbounds %struct.Subclass, ptr %t, i32 0, i32 1
  // CHECK: store i32 0, ptr %z, align 8
}

extern "C" void f21(Subclass *s1, Subclass *s2) {
  *s1 = *s2;
  // CHECK-LABEL: define void @f21
  // CHECK: %call = call nonnull align 8 dereferenceable(20) ptr @_ZN8SubclassaSERKS_
}

struct S3 {
  int *__ptrauth(2, 0, 0, "authenticates-null-values") f0;
};

extern "C" void f22() {
  struct S3 s;
  // CHECK-LABEL: define void @f22()
  // CHECK: [[S:%.*]] = alloca %struct.S3
  // CHECK: ret void
}

struct S4 : virtual S3 {
  authenticated_null_addr_disc new_field;
};

extern "C" void f23() {
  struct S4 s = {};
  // CHECK-LABEL: define void @f23()
  // CHECK: %s = alloca %struct.S4
  // CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %s, ptr align 8 @1, i64 24, i1 false)
  // CHECK: %0 = getelementptr inbounds %struct.S4, ptr %s, i32 0, i32 2
  // CHECK: %1 = getelementptr inbounds %struct.S3, ptr %0, i32 0, i32 0
  // CHECK: %2 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
  // CHECK: %3 = inttoptr i64 %2 to ptr
  // CHECK: store ptr %3, ptr %1
  // CHECK: %4 = getelementptr inbounds %struct.S4, ptr %s, i32 0, i32 1
  // CHECK: %5 = ptrtoint ptr %4 to i64
  // CHECK: %6 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %5)
  // CHECK: %7 = inttoptr i64 %6 to ptr
  // CHECK: store ptr %7, ptr %4
  // CHECK: %call = call ptr @_ZN2S4C1Ev
  // CHECK: foobar:
  asm("foobar:");
}

struct S5 : S1 {
  S5() : S1() {}
};

extern "C" void f24() {
  struct S5 s = {};
}

struct S6 {
  int i;
  authenticated_null p;
  S6(){};
};

extern "C" void f25() {
  struct S6 s = {};
}

struct S7 {
  void* __ptrauth(1,1,1) field1;
  void* __ptrauth(1,1,1) field2;
};

extern "C" void f26() {
  int i = 0;
  struct S7 s = { .field2 = &i};
  // CHECK: %field1 = getelementptr inbounds %struct.S7, ptr %s, i32 0, i32 0
  // CHECK: store ptr null, ptr %field1
  // CHECK: %field2 = getelementptr inbounds %struct.S7, ptr %s, i32 0, i32 1
  // CHECK: %0 = ptrtoint ptr %field2 to i64
  // CHECK: %1 = call i64 @llvm.ptrauth.blend(i64 %0, i64 1)
  // CHECK: %3 = call i64 @llvm.ptrauth.sign(i64 %2, i32 1, i64 %1)
  // CHECK: %4 = inttoptr i64 %3 to ptr
  // CHECK: store ptr %4, ptr %field2
}

struct AStruct;
const AStruct &foo();
class AClass { 
  public: 
  AClass() {} 
  private: 
  virtual void f(); 
};
void AClass::f() {
  const struct {
    unsigned long a;
    const AStruct &b;
    unsigned long c;
    unsigned long d;
    unsigned long e;
  } test []= {{ 0, foo(), 0, 0, 0 }};

}
AClass global;


// struct S1 copy constructor
// CHECK-LABEL: define linkonce_odr nonnull align 8 dereferenceable(16) ptr @_ZN2S1aSERKS_
// CHECK: %this.addr = alloca ptr
// CHECK: [[ADDR:%.*]] = alloca ptr
// CHECK: store ptr %this, ptr %this.addr
// CHECK: store ptr %0, ptr [[ADDR]]
// CHECK: %this1 = load ptr, ptr %this.addr
// CHECK: %p = getelementptr inbounds %struct.S1, ptr %this1, i32 0, i32 0
// CHECK: [[S1PTR:%.*]] = load ptr, ptr [[ADDR]]
// CHECK: %p2 = getelementptr inbounds %struct.S1, ptr [[S1PTR]], i32 0, i32 0
// CHECK: [[P2:%.*]] = load ptr, ptr %p2
// CHECK: store ptr [[P2]], ptr %p
// CHECK: %q = getelementptr inbounds %struct.S1, ptr %this1, i32 0, i32 1
// CHECK: [[S1PTR:%.*]] = load ptr, ptr [[ADDR]]
// CHECK: %q3 = getelementptr inbounds %struct.S1, ptr [[S1PTR]], i32 0, i32 1
// CHECK: [[Q3_ADDR:%.*]] = load ptr, ptr %q3
// CHECK: [[Q3:%.*]] = ptrtoint ptr %q3 to i64
// CHECK: [[Q:%.*]] = ptrtoint ptr %q to i64
// CHECK: [[DISC:%.*]] = ptrtoint ptr [[Q3_ADDR]] to i64
// CHECK: [[RESIGNED:%.*]] = call i64 @llvm.ptrauth.resign(i64 [[DISC]], i32 2, i64 [[Q3]], i32 2, i64 [[Q]])
// CHECK: [[CAST:%.*]] = inttoptr i64 [[RESIGNED]] to ptr
// CHECK: store ptr [[CAST]], ptr %q

// CHECK-LABEL: define linkonce_odr ptr @_ZN2S5C2Ev
// CHECK: %this.addr = alloca ptr
// CHECK: store ptr %this, ptr %this.addr
// CHECK: %this1 = load ptr, ptr %this.addr
// CHECK: %0 = getelementptr inbounds i8, ptr %this1, i64 0
// CHECK: call void @llvm.memset.p0.i64(ptr align 8 %0, i8 0, i64 16, i1 false)
// CHECK: %1 = getelementptr inbounds %struct.S1, ptr %this1, i32 0, i32 0
// CHECK: %2 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 0)
// CHECK: %3 = inttoptr i64 %2 to ptr
// CHECK: store ptr %3, ptr %1
// CHECK: %4 = getelementptr inbounds %struct.S1, ptr %this1, i32 0, i32 1
// CHECK: %5 = ptrtoint ptr %4 to i64
// CHECK: %6 = call i64 @llvm.ptrauth.sign(i64 0, i32 2, i64 %5)
// CHECK: %7 = inttoptr i64 %6 to ptr
// CHECK: store ptr %7, ptr %4

// CHECK-LABEL: define linkonce_odr ptr @_ZN2S6C2Ev
// CHECK: %this.addr = alloca ptr
// CHECK: store ptr %this, ptr %this.addr
// CHECK: %this1 = load ptr, ptr %this.addr
// CHECK: ret ptr %this1
