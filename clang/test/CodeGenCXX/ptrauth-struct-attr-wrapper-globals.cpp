// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-calls -fptrauth-returns -fptrauth-intrinsics -emit-llvm -std=c++17 -O1 -disable-llvm-passes -no-enable-noundef-analysis -fexceptions -fcxx-exceptions -o - %s | FileCheck %s

// CHECK: %[[STRUCT_S0:.*]] = type { i32, i32, [4 x i32] }
// CHECK: %[[STRUCT_S5:.*]] = type { %[[STRUCT_S2:.*]] }
// CHECK: %[[STRUCT_S2]] = type { i32, %[[STRUCT_S0]] }
// CHECK: %[[STRUCT_S6:.*]] = type { i8 }
// CHECK: %[[STRUCT_S7:.*]] = type { i8 }
// CHECK: %[[STRUCT_S8:.*]] = type { i8 }
// CHECK: %[[STRUCT_S9:.*]] = type { i8 }
// CHECK: %[[STRUCT_S12:.*]] = type { i8 }
// CHECK: %[[STRUCT_S14:.*]] = type { i8 }
// CHECK: %[[STRUCT_S13:.*]] = type { i8 }
// CHECK: %[[STRUCT_S1:.*]] = type { i32, i32, [4 x i32] }
// CHECK: %[[STRUCT_S3:.*]] = type { [2 x i32], %[[STRUCT_S0]] }
// CHECK: %[[CLASS_ANON:.*]] = type { ptr, ptr }
// CHECK: %[[CLASS_ANON_0:.*]] = type { %[[STRUCT_S0]], i32 }

#include <ptrauth.h>

typedef __SIZE_TYPE__ size_t;
void* operator new(size_t count, void* ptr);

#define ATTR_NONE __attribute__((ptrauth_struct(ptrauth_key_none, 100)))
#define ATTR0 __attribute__((ptrauth_struct(1, 100)))
#define ATTR1 __attribute__((ptrauth_struct(1, 101)))
#define ATTR2 __attribute__((ptrauth_struct(1, 102)))
#define ATTR4 __attribute__((ptrauth_struct(1, 104)))

struct ATTR0 S0 {
  int f0, f1, f2[4];
  S0() {}
  S0(const S0 &);
  S0 &operator=(const S0 &);
  ~S0();
  void nonvirtual0() { f1 = 1; }
  int lambda0(int i) {
    return [&](){ return f1 + i; }();
  }
  int lambda1(int i) {
    return [*this, i](){ return f1 + i; }();
  }
};

struct ATTR1 S1 {
  int f0, f1, f2[4];
  void nonvirtual1();
  S1();
  ~S1() noexcept(false);
};

struct ATTR2 S2 {
  int f0;
  void nonvirtual2();
  S0 f1;
  S2(S0);
};

struct S3 {
  int f0[2];
  S0 f1;
};

struct ATTR4 S4 {
  int f0, f1;
};

struct ATTR2 S5 : S2 {
  using S2::S2;
};

struct ATTR_NONE S6 {
};

template <int k>
struct __attribute__((ptrauth_struct(k, 100))) S7 {
};

template <int d>
struct __attribute__((ptrauth_struct(1, d))) S8 {
};

template <int k, int d>
struct __attribute__((ptrauth_struct((k + 2) % 4, (d + 1)))) S9 {
};

struct ATTR0 S11 : S1, S2 {
};

struct __attribute__((ptrauth_struct(__builtin_ptrauth_struct_key(S0) + 1, __builtin_ptrauth_struct_disc(S1) + 1))) S12 {
};

template <class Derived>
struct __attribute__((ptrauth_struct(__builtin_ptrauth_struct_key(Derived) + 1, __builtin_ptrauth_struct_disc(Derived) + 1))) S13 {
};

struct ATTR4 S14 : S13<S14> {
};

// CHECK: @gs0.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs0, i32 1, i64 0, i64 100 },
// CHECK: @gs2 = constant ptr @gs0.ptrauth, align 8
// CHECK: @gs3 = constant ptr @gs0.ptrauth, align 8
// CHECK: @gs6 = global %[[STRUCT_S6]] zeroinitializer, align 1
// CHECK: @gs7 = global ptr @gs6, align 8
// CHECK: @gs8 = global %[[STRUCT_S7]] zeroinitializer, align 1
// CHECK: @gs8.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs8, i32 1, i64 0, i64 100 }, section "llvm.ptrauth", align 8
// CHECK: @gs9 = global ptr @gs8.ptrauth, align 8
// CHECK: @gs10 = global %[[STRUCT_S8]] zeroinitializer, align 1
// CHECK: @gs10.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs10, i32 1, i64 0, i64 200 }, section "llvm.ptrauth", align 8
// CHECK: @gs11 = global ptr @gs10.ptrauth, align 8
// CHECK: @gs12 = global %[[STRUCT_S9]] zeroinitializer, align 1
// CHECK: @gs12.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs12, i32 0, i64 0, i64 202 }, section "llvm.ptrauth", align 8
// CHECK: @gs13 = global ptr @gs12.ptrauth, align 8
// CHECK: @gs16 = global %[[STRUCT_S12]] zeroinitializer, align 1
// CHECK: @gs16.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs16, i32 2, i64 0, i64 102 }, section "llvm.ptrauth", align 8
// CHECK: @gs17 = global ptr @gs16.ptrauth, align 8
// CHECK: @gs18 = global %[[STRUCT_S14]] zeroinitializer, align 1
// CHECK: @gs18.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs18, i32 1, i64 0, i64 104 }, section "llvm.ptrauth", align 8
// CHECK: @gs19 = global ptr @gs18.ptrauth, align 8
// CHECK: @gs20 = global %[[STRUCT_S13]] zeroinitializer, align 1
// CHECK: @gs20.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @gs20, i32 2, i64 0, i64 105 }, section "llvm.ptrauth", align 8
// CHECK: @gs21 = global ptr @gs20.ptrauth, align 8

// CHECK: define internal void @__cxx_global_var_init()
// CHECK: %[[V0:.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @gs0 to i64), i32 1, i64 100)
// CHECK: %[[V1:.*]] = inttoptr i64 %[[V0]] to ptr
// CHECK: call ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V1]])
// CHECK: call i32 @__cxa_atexit(ptr @_ZN2S0D1Ev.ptrauth, ptr @gs0.ptrauth, ptr @__dso_handle)
// CHECK: ret void
// CHECK: }

// CHECK: define linkonce_odr ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[THIS:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: call ptr @_ZN2S0C2Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[THIS1]])
// CHECK: ret ptr %[[THIS1]]

S0 gs0;

// CHECK: define linkonce_odr ptr @_ZN2S5CI12S2E2S0(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS:.*]], ptr %[[V0:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: call ptr @_ZN2S5CI22S2E2S0(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS1]], ptr %[[V0]])
// CHECK: ret ptr %[[THIS1]]

S5 gs1(gs0);

S0 &gs2 = gs0;
S0 &gs3 = gs2;

S6 gs6, *gs7 = &gs6;

S7<1> gs8, *gs9 = &gs8;
S8<200> gs10, *gs11 = &gs10;
S9<2, 201> gs12, *gs13 = &gs12;
S12 gs16, *gs17 = &gs16;
S14 gs18, *gs19 = &gs18;
S13<S14> gs20, *gs21 = &gs20;

// CHECK: define void @_Z16test_nonvirtual0P2S0(ptr %[[S:.*]])
// CHECK: %[[S_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[S_ADDR]], align 8
// CHECK: call void @_ZN2S011nonvirtual0Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V0]])

// CHECK: define linkonce_odr void @_ZN2S011nonvirtual0Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[THIS:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[V0:.*]] = ptrtoint ptr %[[THIS1]] to i64
// CHECK: %[[V1:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V0]], i32 1, i64 100)
// CHECK: %[[V2:.*]] = inttoptr i64 %[[V1]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V2]], i64 4
// CHECK: store i32 1, ptr %[[RESIGNEDGEP]], align 4

void test_nonvirtual0(S0 *s) {
  s->nonvirtual0();
}

// CHECK: define ptr @_Z9test_new0v()
// CHECK: entry:
// CHECK: %[[EXN_SLOT:.*]] = alloca ptr
// CHECK: %[[EHSELECTOR_SLOT:.*]] = alloca i32
// CHECK: %[[CALL:.*]] = call noalias nonnull ptr @_Znwm(i64 24)
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[CALL]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 101)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[CALL1:.*]] = invoke ptr @_ZN2S1C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V3]])

// CHECK: ret ptr %[[CALL]]

// CHECK: landingpad { ptr, i32 }
// CHECK: call void @_ZdlPvm(ptr %[[CALL]], i64 24)

S1 *test_new0() {
  return new S1();
}

// CHECK: define ptr @_Z9test_new1Pv(ptr %[[P:.*]])
// CHECK: %[[P_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[P]], ptr %[[P_ADDR]], align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[P_ADDR]], align 8
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V1]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V2]], i32 1, i64 101)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: %[[CALL:.*]] = call ptr @_ZN2S1C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V4]])
// CHECK: ret ptr %[[V1]]

S1 *test_new1(void *p) {
  return new (p) S1;
}

// CHECK: define ptr @_Z9test_new2v()
// CHECK: %[[CALL:.*]] = call noalias nonnull ptr @_Znam(i64 112)
// CHECK: %[[V2:.*]] = getelementptr inbounds i8, ptr %[[CALL]], i64 16
// CHECK: %[[ARRAYCTOR_END:.*]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[V2]], i64 4

// CHECK: %[[ARRAYCTOR_CUR:.*]] = phi ptr [ %[[V2]], %{{.*}} ], [ %[[ARRAYCTOR_NEXT:.*]], %{{.*}} ]
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[ARRAYCTOR_CUR]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V4]], i32 1, i64 101)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: %[[CALL1:.*]] = invoke ptr @_ZN2S1C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V6]])

// CHECK: %[[ARRAYCTOR_NEXT]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[ARRAYCTOR_CUR]], i64 1
// CHECK: %[[ARRAYCTOR_DONE:.*]] = icmp eq ptr %[[ARRAYCTOR_NEXT]], %[[ARRAYCTOR_END]]

// CHECK: ret ptr %[[V2]]

// CHECK: landingpad { ptr, i32 }

// CHECK: %[[ARRAYDESTROY_ELEMENTPAST:.*]] = phi ptr [ %[[ARRAYCTOR_CUR]], %{{.*}} ], [ %[[ARRAYDESTROY_ELEMENT:.*]], %{{.*}} ]
// CHECK: %[[ARRAYDESTROY_ELEMENT:.*]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[ARRAYDESTROY_ELEMENTPAST]], i64 -1
// CHECK: %[[V10:.*]] = ptrtoint ptr %[[ARRAYDESTROY_ELEMENT]] to i64
// CHECK: %[[V11:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V10]], i32 1, i64 101)
// CHECK: %[[V12:.*]] = inttoptr i64 %[[V11]] to ptr
// CHECK: invoke ptr @_ZN2S1D1Ev(ptr  nonnull align {{[0-9]+}} dereferenceable(24) %[[V12]])

// CHECK: icmp eq ptr %[[ARRAYDESTROY_ELEMENT]], %[[V2]]

// CHECK: call void @_ZdaPvm(ptr %[[CALL]], i64 112)

S1 *test_new2() {
  return new S1[4];
}

// CHECK: define void @_Z12test_delete0P2S1(ptr %{{.*}})
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0]] = load ptr, ptr %[[A_ADDR]], align 8

// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 101)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: invoke ptr @_ZN2S1D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V0]])

// CHECK: call void @_ZdlPvm(ptr %[[V3]], i64 24)

// CHECK: landingpad { ptr, i32 }
// CHECK: call void @_ZdlPvm(ptr %[[V3]], i64 24)

void test_delete0(S1 *a) {
  delete a;
}

// CHECK: define void @_Z12test_delete1P2S1(ptr %{{.*}})
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]], align 8

// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 101)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V4]], i64 -16
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V5]], i32 1, i64 101)
// CHECK: %[[V7:.*]] = inttoptr i64 %[[V6]] to ptr
// CHECK: %[[RESIGNEDGEP1:.*]] = getelementptr i8, ptr %[[V7]], i64 -8
// CHECK: %[[V9:.*]] = load i64, ptr %[[RESIGNEDGEP1]], align 4
// CHECK: %[[V10:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V11:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V10]], i32 1, i64 101)
// CHECK: %[[V12:.*]] = inttoptr i64 %[[V11]] to ptr
// CHECK: %[[DELETE_END:.*]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[V12]], i64 %[[V9]]
// CHECK: icmp eq ptr %[[V12]], %[[DELETE_END]]

// CHECK: %[[ARRAYDESTROY_ELEMENTPAST:.*]] = phi ptr [ %[[DELETE_END]], %{{.*}} ], [ %[[ARRAYDESTROY_ELEMENT]], %{{.*}} ]
// CHECK: %[[ARRAYDESTROY_ELEMENT:.*]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[ARRAYDESTROY_ELEMENTPAST]], i64 -1
// CHECK: %[[V13:.*]] = ptrtoint ptr %[[ARRAYDESTROY_ELEMENT]] to i64
// CHECK: %[[V14:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V13]], i32 1, i64 101)
// CHECK: %[[V15:.*]] = inttoptr i64 %[[V14]] to ptr
// CHECK: invoke ptr @_ZN2S1D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V15]])

// CHECK: icmp eq ptr %[[ARRAYDESTROY_ELEMENT]], %[[V12]]

// CHECK: call void @_ZdaPvm(ptr %[[RESIGNEDGEP]], i64 %{{.*}})

// CHECK: landingpad { ptr, i32 }

// CHECK: %[[ARRAYDESTROY_ELEMENTPAST4:.*]] = phi ptr [ %[[ARRAYDESTROY_ELEMENT]], %{{.*}} ], [ %[[ARRAYDESTROY_ELEMENT5:.*]], %{{.*}} ]
// CHECK: %[[ARRAYDESTROY_ELEMENT5:.*]] = getelementptr inbounds %[[STRUCT_S1]], ptr %[[ARRAYDESTROY_ELEMENTPAST4]], i64 -1
// CHECK: %[[V19:.*]] = ptrtoint ptr %[[ARRAYDESTROY_ELEMENT5]] to i64
// CHECK: %[[V20:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V19]], i32 1, i64 101)
// CHECK: %[[V21:.*]] = inttoptr i64 %[[V20]] to ptr
// CHECK: %[[CALL7:.*]] = invoke ptr @_ZN2S1D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V21]])

// CHECK: icmp eq ptr %[[ARRAYDESTROY_ELEMENT5]], %[[V12]]

// CHECK: call void @_ZdaPvm(ptr %[[RESIGNEDGEP]], i64 %{{.*}})

void test_delete1(S1 *a) {
  delete [] a;
}

// CHECK: define void @_Z16test_assignment0P2S0S0_(ptr %{{.*}}, ptr %{{.*}})
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[B_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[B_ADDR]], align 8
// CHECK: %[[V1:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: call nonnull align 4 dereferenceable(24) ptr @_ZN2S0aSERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V1]], ptr nonnull align 4 dereferenceable(24) %[[V0]])

void test_assignment0(S0 *a, S0 *b) {
  *a = *b;
}

// CHECK: define void @_Z16test_assignment1v()
// CHECK: %[[T0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: call ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V3]])
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V5]], i32 1, i64 100)
// CHECK: %[[V7:.*]] = inttoptr i64 %[[V6]] to ptr
// CHECK: invoke ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V7]])

// CHECK: %[[V8:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V8]], i32 1, i64 100)
// CHECK: %[[V10:.*]] = inttoptr i64 %[[V9]] to ptr
// CHECK: %[[V11:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V12:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V11]], i32 1, i64 100)
// CHECK: %[[V13:.*]] = inttoptr i64 %[[V12]] to ptr
// CHECK: invoke nonnull align 4 dereferenceable(24) ptr @_ZN2S0aSERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V13]], ptr nonnull align 4 dereferenceable(24) %[[V10]])

// CHECK: %[[V14:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V15:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V14]], i32 1, i64 100)
// CHECK: %[[V16:.*]] = inttoptr i64 %[[V15]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V16]])
// CHECK: %[[V18:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V19:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V18]], i32 1, i64 100)
// CHECK: %[[V20:.*]] = inttoptr i64 %[[V19]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V20]])
// CHECK: ret void

// CHECK: landingpad { ptr, i32 }
// CHECK: landingpad { ptr, i32 }
// CHECK: %[[V28:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V29:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V28]], i32 1, i64 100)
// CHECK: %[[V30:.*]] = inttoptr i64 %[[V29]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V30]])

// CHECK: %[[V32:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V33:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V32]], i32 1, i64 100)
// CHECK: %[[V34:.*]] = inttoptr i64 %[[V33]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V34]])

void test_assignment1() {
  S0 t0, t1;
  t0 = t1;
}

// CHECK: define void @_Z16test_assignment2P2S4S0_(
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[B_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[V0:.*]] = load ptr, ptr %[[B_ADDR]], align 8
// CHECK: %[[V1:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[V2]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V4]], i32 1, i64 104)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: %[[V7:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V8:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V7]], i32 1, i64 104)
// CHECK: %[[V9:.*]] = inttoptr i64 %[[V8]] to ptr
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 4 %[[V6]], ptr align 4 %[[V9]], i64 8, i1 false)

void test_assignment2(S4 *a, S4 *b) {
  *a = *b;
}

// CHECK: define void @_Z28test_constructor_destructor0v()
// CHECK: %[[T0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: call ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V3]])
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V5]], i32 1, i64 100)
// CHECK: %[[V7:.*]] = inttoptr i64 %[[V6]] to ptr
// CHECK: %[[V8:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V8]], i32 1, i64 100)
// CHECK: %[[V10:.*]] = inttoptr i64 %[[V9]] to ptr
// CHECK: invoke ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V7]], ptr nonnull align 4 dereferenceable(24) %[[V10]])

// CHECK: %[[V11:.*]] = ptrtoint ptr %[[T1]] to i64
// CHECK: %[[V12:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V11]], i32 1, i64 100)
// CHECK: %[[V13:.*]] = inttoptr i64 %[[V12]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V13]])
// CHECK: %[[V15:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V16:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V15]], i32 1, i64 100)
// CHECK: %[[V17:.*]] = inttoptr i64 %[[V16]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24)  %[[V17]])
// CHECK: ret void

// CHECK: landingpad { ptr, i32 }
// CHECK: %[[V23:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V24:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V23]], i32 1, i64 100)
// CHECK: %[[V25:.*]] = inttoptr i64 %[[V24]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24)  %[[V25]])

void test_constructor_destructor0() {
  S0 t0, t1 = t0;
}

// CHECK: define void @_Z19test_member_access1P2S0i(ptr %{{.*}}, i32 %{{.*}})
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[I_ADDR:.*]] = alloca i32, align 4
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: %[[V1:.*]] = load i32, ptr %[[I_ADDR]], align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V1]], 2
// CHECK: %[[IDXPROM:.*]] = sext i32 %[[ADD]] to i64
// CHECK: %[[ARRAYIDX_OFFS:.*]] = mul nsw i64 %[[IDXPROM]], 4
// CHECK: %[[ADD1:.*]] = add i64 8, %[[ARRAYIDX_OFFS]]
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 100)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V4]], i64 %[[ADD1]]
// CHECK: store i32 123, ptr %[[RESIGNEDGEP]], align 4

void test_member_access1(S0 *a, int i) {
  a->f2[i + 2] = 123;
}

// CHECK: define nonnull align 4 dereferenceable(24) ptr @_Z15test_reference0R2S0(ptr nonnull align 4 dereferenceable(24) %[[A:.*]])
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[T0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[T1:.*]] = alloca ptr, align 8
// CHECK: %[[R0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: alloca ptr, align 8
// CHECK: %[[R1:.*]] = alloca ptr, align 8
// CHECK: %[[V11:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V21:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V11]], i32 1, i64 100)
// CHECK: %[[V31:.*]] = inttoptr i64 %[[V21]] to ptr
// CHECK: %[[V41:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: call ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V31]], ptr nonnull align 4 dereferenceable(24) %[[V41]])
// CHECK: %[[V6:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: store ptr %[[V6]], ptr %[[T1]], align 8
// CHECK: %[[V8:.*]] = ptrtoint ptr %[[R0]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V8]], i32 1, i64 100)
// CHECK: %[[V10:.*]] = inttoptr i64 %[[V9]] to ptr
// CHECK: %[[V11:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V12:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V11]], i32 1, i64 100)
// CHECK: %[[V13:.*]] = inttoptr i64 %[[V12]] to ptr
// CHECK: %[[CALL1:.*]] = invoke nonnull align 4 dereferenceable(24) ptr @_Z5func0R2S0(ptr nonnull align 4 dereferenceable(24) %[[V13]])

// CHECK: invoke ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V10]], ptr nonnull align 4 dereferenceable(24) %[[CALL1]])

// CHECK: %[[V15:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V16:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V15]], i32 1, i64 100)
// CHECK: %[[V17:.*]] = inttoptr i64 %[[V16]] to ptr
// CHECK: %[[CALL6:.*]] = invoke nonnull align 4 dereferenceable(24) ptr @_Z5func0R2S0(ptr nonnull align 4 dereferenceable(24) %[[V17]])

// CHECK: store ptr %[[CALL6]], ptr %[[R1]], align 8
// CHECK: %[[V18:.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @gs0 to i64), i32 1, i64 100)
// CHECK: %[[V19:.*]] = inttoptr i64 %[[V18]] to ptr
// CHECK: %[[V21:.*]] = ptrtoint ptr %[[R0]] to i64
// CHECK: %[[V22:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V21]], i32 1, i64 100)
// CHECK: %[[V23:.*]] = inttoptr i64 %[[V22]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V23]])
// CHECK: %[[V26:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V27:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V26]], i32 1, i64 100)
// CHECK: %[[V28:.*]] = inttoptr i64 %[[V27]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V28]])
// CHECK: ret ptr %[[V19]]

// CHECK: landingpad { ptr, i32 }

// CHECK: landingpad { ptr, i32 }
// CHECK: %[[V37:.*]] = ptrtoint ptr %[[R0]] to i64
// CHECK: %[[V38:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V37]], i32 1, i64 100)
// CHECK: %[[V39:.*]] = inttoptr i64 %[[V38]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V39]])

// CHECK: %[[V42:.*]] = ptrtoint ptr %[[T0]] to i64
// CHECK: %[[V43:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V42]], i32 1, i64 100)
// CHECK: %[[V44:.*]] = inttoptr i64 %[[V43]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V44]])

S0 &func0(S0 &);

S0 &test_reference0(S0 &a) {
  S0 t0 = a;
  S0 &t1 = a;
  S0 r0 = func0(t0);
  S0 &r1 = func0(t0);
  return gs0;
}

// CHECK: define void @_Z17test_conditional0bR2S0S0_(ptr dead_on_unwind noalias writable sret(%struct.S0) align 4 %[[AGG_RESULT:.*]], i1 %{{.*}}, ptr nonnull align 4 dereferenceable(24) %{{.*}}, ptr nonnull align 4 dereferenceable(24) %{{.*}})
// CHECK: alloca ptr, align 8
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[B_ADDR:.*]] = alloca ptr, align 8

// CHECK: %[[V5:.*]] = load ptr, ptr %[[A_ADDR]], align 8

// CHECK: %[[V6:.*]] = load ptr, ptr %[[B_ADDR]], align 8

// CHECK: %[[V7:.*]] = phi ptr [ %[[V5]], %{{.*}} ], [ %[[V6]], %{{.*}} ]
// CHECK: call ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[AGG_RESULT]], ptr nonnull align 4 dereferenceable(24) %[[V7]])
// CHECK: ret void

S0 test_conditional0(bool c, S0 &a, S0 &b) {
  return c ? a : b;
}

// CHECK: define i32 @_Z17test_conditional1b(
// CHECK: %[[A:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[B:.*]] = alloca %[[STRUCT_S0]], align 4

// CHECK: %[[V9:.*]] = phi ptr [ %[[A]], %{{.*}} ], [ %[[B]], %{{.*}} ]
// CHECK: %[[F1:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[V9]], i32 0, i32 1
// CHECK: %[[V10:.*]] = load i32, ptr %[[F1]], align 4
// CHECK: ret i32 %[[V10]]

int test_conditional1(bool c) {
  S0 a, b;
  return (c ? a : b).f1;
}

// CHECK: define void @_Z17test_conditional2bR2S2R2S3(ptr dead_on_unwind noalias writable sret(%struct.S0) align 4 %[[AGG_RESULT]], i1 %{{.*}}, ptr nonnull align 4 dereferenceable(28) %{{.*}}, ptr nonnull align 4 dereferenceable(32) %{{.*}})
// CHECK: alloca ptr, align 8
// CHECK: %[[A_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[B_ADDR:.*]] = alloca ptr, align 8

// CHECK: %[[V5:.*]] = load ptr, ptr %[[A_ADDR]], align 8
// CHECK: %[[V6:.*]] = ptrtoint ptr %[[V5]] to i64
// CHECK: %[[V7:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V6]], i32 1, i64 102)
// CHECK: %[[V8:.*]] = inttoptr i64 %[[V7]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V8]], i64 4
// CHECK: %[[V10:.*]] = ptrtoint ptr %[[RESIGNEDGEP]] to i64
// CHECK: %[[V11:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V10]], i32 1, i64 100)
// CHECK: %[[V12:.*]] = inttoptr i64 %[[V11]] to ptr

// CHECK: %[[V14:.*]] = load ptr, ptr %[[B_ADDR]], align 8
// CHECK: %[[F1:.*]] = getelementptr inbounds %[[STRUCT_S3]], ptr %[[V14]], i32 0, i32 1
// CHECK: %[[V15:.*]] = ptrtoint ptr %[[F1]] to i64
// CHECK: %[[V16:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V15]], i32 1, i64 100)
// CHECK: %[[V17:.*]] = inttoptr i64 %[[V16]] to ptr

// CHECK: %[[V18:.*]] = phi ptr [ %[[V12]], %{{.*}} ], [ %[[V17]], %{{.*}} ]
// CHECK: call ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[AGG_RESULT]], ptr nonnull align 4 dereferenceable(24) %[[V18]])
// CHECK: ret void

S0 test_conditional2(bool c, S2 &a, S3 &b) {
  return c ? a.f1 : b.f1;
}

// CHECK: define void @_Z17test_inheritance0P3S11(ptr %[[A:.*]])
// CHECK: %[[A_ADDR:.*]] = alloca ptr,
// CHECK: store ptr %[[A]], ptr %[[A_ADDR]],
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]],
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V2]], i32 1, i64 100, i32 1, i64 101)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: call void @_ZN2S111nonvirtual1Ev(ptr nonnull align 4 {{.*}} %[[V4]])

void test_inheritance0(S11 *a) {
  a->nonvirtual1();
}

// CHECK: define void @_Z17test_inheritance1P3S11(ptr %[[A:.*]])
// CHECK: %[[A_ADDR:.*]] = alloca ptr,
// CHECK: store ptr %[[A]], ptr %[[A_ADDR]],
// CHECK: %[[V0:.*]] = load ptr, ptr %[[A_ADDR]],
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[ADD_PTR:.*]] = getelementptr inbounds i8, ptr %[[V4]], i64 24
// CHECK: %[[V6:.*]] = ptrtoint ptr %[[ADD_PTR]] to i64
// CHECK: %[[V7:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V6]], i32 1, i64 102)
// CHECK: %[[V8:.*]] = inttoptr i64 %[[V7]] to ptr
// CHECK: call void @_ZN2S211nonvirtual2Ev(ptr nonnull align 4 {{.*}} %[[V8]])

void test_inheritance1(S11 *a) {
  a->nonvirtual2();
}

// CHECK: define void @_Z15test_exception0v()
// CHECK: alloca ptr, align 8
// CHECK: %[[S0:.*]] = alloca ptr, align 8
// CHECK: %[[I:.*]] = alloca i32, align 4
// CHECK: %[[EXCEPTION:.*]] = call ptr @__cxa_allocate_exception(i64 24)
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[EXCEPTION]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: invoke ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V3]])

// CHECK: invoke void @__cxa_throw(ptr %[[EXCEPTION]], ptr @_ZTI2S0, ptr @_ZN2S0D1Ev.ptrauth)

// CHECK: landingpad { ptr, i32 }
// CHECK: call void @__cxa_free_exception(ptr %[[EXCEPTION]])

// CHECK: %[[V12:.*]] = call ptr @__cxa_begin_catch(
// CHECK: %[[V13:.*]] = ptrtoint ptr %[[V12]] to i64
// CHECK: %[[V14:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V13]], i32 1, i64 100)
// CHECK: %[[V15:.*]] = inttoptr i64 %[[V14]] to ptr
// CHECK: store ptr %[[V15]], ptr %[[S0]], align 8
// CHECK: %[[V17:.*]] = load ptr, ptr %[[S0]], align 8
// CHECK: %[[V18:.*]] = ptrtoint ptr %[[V17]] to i64
// CHECK: %[[V19:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V18]], i32 1, i64 100)
// CHECK: %[[V20:.*]] = inttoptr i64 %[[V19]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V20]], i64 4
// CHECK: %[[V23:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4
// CHECK: store i32 %[[V23]], ptr %[[I]], align 4

void test_exception0() {
  try {
    throw S0();
  } catch (const S0 &s0) {
    int i = s0.f1;
  }
}

// CHECK: define void @_Z15test_exception1v()
// CHECK: %[[S0:.*]] = alloca %[[STRUCT_S0]], align 4
// CHECK: %[[I:.*]] = alloca i32, align 4
// CHECK: %[[EXCEPTION:.*]] = call ptr @__cxa_allocate_exception(i64 24)
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[EXCEPTION]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: invoke ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24)  %[[V3]])

// CHECK: %[[V12:.*]] = call ptr @__cxa_get_exception_ptr(
// CHECK: %[[V14:.*]] = ptrtoint ptr %[[S0]] to i64
// CHECK: %[[V15:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V14]], i32 1, i64 100)
// CHECK: %[[V16:.*]] = inttoptr i64 %[[V15]] to ptr
// CHECK: %[[V17:.*]] = ptrtoint ptr %[[V12]] to i64
// CHECK: %[[V18:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V17]], i32 1, i64 100)
// CHECK: %[[V19:.*]] = inttoptr i64 %[[V18]] to ptr
// CHECK: invoke ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V16]], ptr nonnull align 4 dereferenceable(24) %[[V19]])

// CHECK: %[[V20:.*]] = call ptr @__cxa_begin_catch(
// CHECK: %[[F1:.*]] = getelementptr inbounds %[[STRUCT_S0]], ptr %[[S0]], i32 0, i32 1
// CHECK: %[[V22:.*]] = load i32, ptr %[[F1]], align 4
// CHECK: store i32 %[[V22]], ptr %[[I]], align 4
// CHECK: %[[V24:.*]] = ptrtoint ptr %[[S0]] to i64
// CHECK: %[[V25:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V24]], i32 1, i64 100)
// CHECK: %[[V26:.*]] = inttoptr i64 %[[V25]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V26]])

void test_exception1() {
  try {
    throw S0();
  } catch (S0 s0) {
    int i = s0.f1;
  }
}

// CHECK: define void @_Z15test_exception2v()
// CHECK: alloca ptr, align 8
// CHECK: %[[S0:.*]] = alloca ptr, align 8
// CHECK: %[[EXN_BYREF_TMP:.*]] = alloca ptr, align 8
// CHECK: %[[I:.*]] = alloca i32, align 4
// CHECK: %[[EXCEPTION:.*]] = call ptr @__cxa_allocate_exception(i64 24)
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[EXCEPTION]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: invoke ptr @_ZN2S0C1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V3]])

// CHECK: %[[V12:.*]] = call ptr @__cxa_begin_catch(
// CHECK: %[[V13:.*]] = ptrtoint ptr %[[V12]] to i64
// CHECK: %[[V14:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V13]], i32 1, i64 100)
// CHECK: %[[V15:.*]] = inttoptr i64 %[[V14]] to ptr
// CHECK: store ptr %[[V15]], ptr %[[EXN_BYREF_TMP]], align 8
// CHECK: store ptr %[[EXN_BYREF_TMP]], ptr %[[S0]], align 8
// CHECK: %[[V18:.*]] = load ptr, ptr %[[S0]], align 8
// CHECK: %[[V19:.*]] = load ptr, ptr %[[V18]], align 8
// CHECK: %[[V20:.*]] = ptrtoint ptr %[[V19]] to i64
// CHECK: %[[V21:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V20]], i32 1, i64 100)
// CHECK: %[[V22:.*]] = inttoptr i64 %[[V21]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V23]], i64 4
// CHECK: %[[V25:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4
// CHECK: store i32 %[[V25]], ptr %[[I]], align 4

void test_exception2() {
  try {
    throw S0();
  } catch (S0 *&s0) {
    int i = s0->f1;
  }
}

// CHECK: define linkonce_odr i32 @_ZN2S07lambda0Ei(
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[I_ADDR:.*]] = alloca i32, align 4
// CHECK: %[[REF_TMP:.*]] = alloca %[[CLASS_ANON]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[V1:.*]] = getelementptr inbounds %[[CLASS_ANON]], ptr %[[REF_TMP]], i32 0, i32 0
// CHECK: store ptr %[[THIS1]], ptr %[[V1]], align 8
// CHECK: %[[V2:.*]] = getelementptr inbounds %[[CLASS_ANON]], ptr %[[REF_TMP]], i32 0, i32 1
// CHECK: store ptr %[[I_ADDR]], ptr %[[V2]], align 8
// CHECK: call i32 @_ZZN2S07lambda0EiENKUlvE_clEv(ptr nonnull align {{[0-9]+}} dereferenceable(16) %[[REF_TMP]])

void test_lambda0(S0 *a) {
  a->lambda0(1);
}

// CHECK: define linkonce_odr i32 @_ZN2S07lambda1Ei(
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[I_ADDR:.*]] = alloca i32, align 4
// CHECK: %[[REF_TMP:.*]] = alloca %[[CLASS_ANON_0]], align 4
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[V1:.*]] = getelementptr inbounds %[[CLASS_ANON_0]], ptr %[[REF_TMP]], i32 0, i32 0
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V1]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V2]], i32 1, i64 100)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: call ptr @_ZN2S0C1ERKS_(ptr nonnull align {{[0-9]+}} dereferenceable(24)  %[[V4]], ptr nonnull align 4 dereferenceable(24) %[[THIS1]])
// CHECK: %[[V5:.*]] = getelementptr inbounds %[[CLASS_ANON_0]], ptr %[[REF_TMP]], i32 0, i32 1
// CHECK: %[[V6:.*]] = load i32, ptr %[[I_ADDR]], align 4
// CHECK: store i32 %[[V6]], ptr %[[V5]], align 4
// CHECK: %[[CALL2:.*]] = invoke i32 @_ZZN2S07lambda1EiENKUlvE_clEv(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[REF_TMP]])

// CHECK: call ptr @_ZZN2S07lambda1EiENUlvE_D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[REF_TMP]])
// CHECK: ret i32 %[[CALL2]]

// CHECK: landingpad { ptr, i32 }
// CHECK: call ptr @_ZZN2S07lambda1EiENUlvE_D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[REF_TMP]])

void test_lambda1(S0 *a) {
  a->lambda1(1);
}

// CHECK-LABEL: define void @_ZN36test_builtin_ptrauth_struct_mangling4testEv()
// CHECK: call void @_ZN36test_builtin_ptrauth_struct_mangling7foo_keyI2S0EEvNS_1SIXu28__builtin_ptrauth_struct_keyT_EEEE(
// CHECK: call void @_ZN36test_builtin_ptrauth_struct_mangling8foo_discI2S0EEvNS_1SIXu29__builtin_ptrauth_struct_discT_EEEE(

namespace test_builtin_ptrauth_struct_mangling {
template <int i>
struct S {
};

template <class C>
void foo_key(S<__builtin_ptrauth_struct_key(C)> a) {
}
template <class C>
void foo_disc(S<__builtin_ptrauth_struct_disc(C)> a) {
}

void test() {
  foo_key<S0>(S<__builtin_ptrauth_struct_key(S0)>());
  foo_disc<S0>(S<__builtin_ptrauth_struct_disc(S0)>());
}
}

// CHECK: define linkonce_odr ptr @_ZN2S0C2Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[THIS:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: ret ptr %[[THIS1]]

// CHECK: define linkonce_odr ptr @_ZN2S5CI22S2E2S0(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS:.*]], ptr %[[V0:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: call ptr @_ZN2S2C2E2S0(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS1]], ptr %[[V0]])
// CHECK: ret ptr %[[THIS1]]

// CHECK: define linkonce_odr i32 @_ZZN2S07lambda0EiENKUlvE_clEv(
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[V0:.*]] = getelementptr inbounds %[[CLASS_ANON]], ptr %[[THIS1]], i32 0, i32 0
// CHECK: %[[V1:.*]] = load ptr, ptr %[[V0]], align 8
// CHECK: %[[V2:.*]] = ptrtoint ptr %[[V1]] to i64
// CHECK: %[[V3:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V2]], i32 1, i64 100)
// CHECK: %[[V4:.*]] = inttoptr i64 %[[V3]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V5]], i64 4
// CHECK: %[[V7:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4
// CHECK: %[[V8:.*]] = getelementptr inbounds %[[CLASS_ANON]], ptr %[[THIS1]], i32 0, i32 1
// CHECK: %[[V9:.*]] = load ptr, ptr %[[V8]], align 8
// CHECK: %[[V10:.*]] = load i32, ptr %[[V9]], align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V7]], %[[V10]]
// CHECK: ret i32 %[[ADD]]

// CHECK: define linkonce_odr i32 @_ZZN2S07lambda1EiENKUlvE_clEv(
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[V0:.*]] = getelementptr inbounds %[[CLASS_ANON_0]], ptr %[[THIS1]], i32 0, i32 0
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: %[[V4:.*]] = ptrtoint ptr %[[V3]] to i64
// CHECK: %[[V5:.*]] = call i64 @llvm.ptrauth.auth(i64 %[[V4]], i32 1, i64 100)
// CHECK: %[[V6:.*]] = inttoptr i64 %[[V5]] to ptr
// CHECK: %[[RESIGNEDGEP:.*]] = getelementptr i8, ptr %[[V6]], i64 4
// CHECK: %[[V9:.*]] = load i32, ptr %[[RESIGNEDGEP]], align 4
// CHECK: %[[V10:.*]] = getelementptr inbounds %[[CLASS_ANON_0]], ptr %[[THIS1]], i32 0, i32 1
// CHECK: %[[V11:.*]] = load i32, ptr %[[V10]], align 4
// CHECK: %[[ADD:.*]] = add nsw i32 %[[V9]], %[[V11]]
// CHECK: ret i32 %[[ADD]]

// CHECK: define linkonce_odr ptr @_ZZN2S07lambda1EiENUlvE_D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS]]) unnamed_addr
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: ptr @_ZZN2S07lambda1EiENUlvE_D2Ev(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS1]])

// CHECK: define linkonce_odr ptr @_ZZN2S07lambda1EiENUlvE_D2Ev(ptr nonnull align {{[0-9]+}} dereferenceable(28) %[[THIS]]) unnamed_addr
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[V0:.*]] = getelementptr inbounds %[[CLASS_ANON_0]], ptr %[[THIS1]], i32 0, i32 0
// CHECK: %[[V1:.*]] = ptrtoint ptr %[[V0]] to i64
// CHECK: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 %[[V1]], i32 1, i64 100)
// CHECK: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK: call ptr @_ZN2S0D1Ev(ptr nonnull align {{[0-9]+}} dereferenceable(24) %[[V3]])
