// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-calls -fptrauth-intrinsics -std=c++20 -emit-llvm %s  -o - | FileCheck %s

#define AQ __ptrauth(1,1,50)
#define IQ __ptrauth(1,0,50)

// CHECK: %[[STRUCT_SA:.*]] = type { ptr, ptr }
// CHECK: %[[STRUCT_SI:.*]] = type { ptr }

// CHECK: @_ZN14test_constexpr1gE = global i32 0, align 4
// CHECK: @_ZN14test_constexpr1gE.ptrauth = private constant { ptr, i32, i64, i64 } { ptr @_ZN14test_constexpr1gE, i32 1, i64 0, i64 50 }, section "llvm.ptrauth", align 8
// CHECK: @__const._ZN14test_constexpr17testNoAddressDiscEv.d = private unnamed_addr constant { ptr, i64, i64, i64, i64 } { ptr @_ZN14test_constexpr1gE.ptrauth, i64 1, i64 1, i64 1, i64 1 }, align 8

struct SA {
  int * AQ m0; // Signed using address discrimination.
  int * AQ m1; // Signed using address discrimination.
};

struct SI {
  int * IQ m; // No address discrimination.
};

struct __attribute__((trivial_abi)) TrivialSA {
  int * AQ m0; // Signed using address discrimination.
  int * AQ m1; // Signed using address discrimination.
};

// Check that TrivialSA is passed indirectly despite being annotated with
// 'trivial_abi'.

// CHECK: define void @_Z18testParamTrivialSA9TrivialSA(ptr noundef %{{.*}})

void testParamTrivialSA(TrivialSA a) {
}

// CHECK: define void @_Z19testCopyConstructor2SA(ptr
// CHECK: call noundef ptr @_ZN2SAC1ERKS_(

// CHECK: define linkonce_odr noundef ptr @_ZN2SAC1ERKS_(
// CHECK: call noundef ptr @_ZN2SAC2ERKS_(

void testCopyConstructor(SA a) {
  SA t = a;
}

// CHECK: define void @_Z19testMoveConstructor2SA(ptr
// CHECK: call noundef ptr @_ZN2SAC1EOS_(

// CHECK: define linkonce_odr noundef ptr @_ZN2SAC1EOS_(
// CHECK: call noundef ptr @_ZN2SAC2EOS_(

void testMoveConstructor(SA a) {
  SA t = static_cast<SA &&>(a);
}

// CHECK: define void @_Z18testCopyAssignment2SA(ptr
// CHECK: call noundef nonnull align 8 dereferenceable(16) ptr @_ZN2SAaSERKS_(

// CHECK: define linkonce_odr noundef nonnull align 8 dereferenceable(16) ptr @_ZN2SAaSERKS_(ptr noundef nonnull align 8 dereferenceable(16) %[[THIS:.*]], ptr noundef nonnull align 8 dereferenceable(16) %0)
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: store ptr %[[V0:.*]], ptr %[[_ADDR]], align 8
// CHECK: %[[THISI:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[M0:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[THISI]], i32 0, i32 0
// CHECK: %[[V1:.*]] = load ptr, ptr %[[_ADDR]], align 8
// CHECK: %[[M02:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[V1]], i32 0, i32 0
// CHECK: %[[V2:.*]] = load ptr, ptr %[[M02]], align 8
// CHECK: %[[V3:.*]] = ptrtoint ptr %[[M02]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V3]], i64 50)
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[M0]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V5]], i64 50)
// CHECK: %[[V8:.*]] = ptrtoint ptr %[[V2]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V8]], i32 1, i64 %[[V4]], i32 1, i64 %[[V6]])

void testCopyAssignment(SA a) {
  SA t;
  t = a;
}

// CHECK: define void @_Z18testMoveAssignment2SA(ptr
// CHECK: call noundef nonnull align 8 dereferenceable(16) ptr @_ZN2SAaSEOS_(

// CHECK: define linkonce_odr noundef nonnull align 8 dereferenceable(16) ptr @_ZN2SAaSEOS_(ptr noundef nonnull align 8 dereferenceable(16) %[[THIS:.*]], ptr noundef nonnull align 8 dereferenceable(16) %0)
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: store ptr %[[V0:.*]], ptr %[[_ADDR]], align 8
// CHECK: %[[THISI:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: %[[M0:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[THISI]], i32 0, i32 0
// CHECK: %[[V1:.*]] = load ptr, ptr %[[_ADDR]], align 8
// CHECK: %[[M02:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[V1]], i32 0, i32 0
// CHECK: %[[V2:.*]] = load ptr, ptr %[[M02]], align 8
// CHECK: %[[V3:.*]] = ptrtoint ptr %[[M02]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V3]], i64 50)
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[M0]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V5]], i64 50)
// CHECK: %[[V8:.*]] = ptrtoint ptr %[[V2]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V8]], i32 1, i64 %[[V4]], i32 1, i64 %[[V6]])

void testMoveAssignment(SA a) {
  SA t;
  t = static_cast<SA &&>(a);
}

// CHECK: define void @_Z19testCopyConstructor2SI(i
// CHECK: call void @llvm.memcpy.p0.p0.i64(

void testCopyConstructor(SI a) {
  SI t = a;
}

// CHECK: define void @_Z19testMoveConstructor2SI(
// CHECK: call void @llvm.memcpy.p0.p0.i64(

void testMoveConstructor(SI a) {
  SI t = static_cast<SI &&>(a);
}

// CHECK: define void @_Z18testCopyAssignment2SI(
// CHECK: call void @llvm.memcpy.p0.p0.i64(

void testCopyAssignment(SI a) {
  SI t;
  t = a;
}

// CHECK: define void @_Z18testMoveAssignment2SI(
// CHECK: call void @llvm.memcpy.p0.p0.i64(

void testMoveAssignment(SI a) {
  SI t;
  t = static_cast<SI &&>(a);
}

namespace test_constexpr {
  int g;

  struct BaseAddrDisc {
    int * AQ ptr;
    long long a, b, c, d;
    constexpr BaseAddrDisc() { ptr = &g; a = b = c = d = 1;}
  };

  struct BaseNoAddrDisc {
    int * IQ ptr;
    long long a, b, c, d;
    constexpr BaseNoAddrDisc() { ptr = &g; a = b = c = d = 1;}
  };

  template <class Base>
  struct Derived : Base {};

// clang cannot initialize 'd' using memcpy because the base class 'Base' has an
// address-discriminated member.

// CHECK-LABEL: define void @_ZN14test_constexpr15testAddressDiscEv()
// CHECK: %[[D:.*]] = alloca %{{.*}}, align 8
// CHECK-NEXT: call noundef ptr @_ZN14test_constexpr7DerivedINS_12BaseAddrDiscEEC1Ev(ptr noundef nonnull align 8 dereferenceable(40) %[[D]])
// CHECK-NEXT: ret void

  void testAddressDisc() {
    constexpr Derived<BaseAddrDisc> d;
  }

// CHECK: define void @_ZN14test_constexpr17testNoAddressDiscEv()
// CHECK: %[[D:.*]] = alloca %{{.*}}, align 8
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %[[D]], ptr align 8 @__const._ZN14test_constexpr17testNoAddressDiscEv.d, i64 40, i1 false)
// CHECK: ret void

  void testNoAddressDisc() {
    constexpr Derived<BaseNoAddrDisc> d;
  }
}
// CHECK: define linkonce_odr noundef ptr @_ZN2SAC2ERKS_(ptr noundef nonnull align 8 dereferenceable(16) %[[THIS:.*]], ptr noundef nonnull align 8 dereferenceable(16) %0)
// CHECK: %[[RETVAL:.*]] = alloca ptr, align 8
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: store ptr %[[V0:.*]], ptr %[[_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: store ptr %[[THIS1]], ptr %[[RETVAL]], align 8
// CHECK: %[[M0:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[THIS1]], i32 0, i32 0
// CHECK: %[[V1:.*]] = load ptr, ptr %[[_ADDR]], align 8
// CHECK: %[[M02:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[V1]], i32 0, i32 0
// CHECK: %[[V2:.*]] = load ptr, ptr %[[M02]], align 8
// CHECK: %[[V3:.*]] = ptrtoint ptr %[[M02]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V3]], i64 50)
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[M0]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V5]], i64 50)
// CHECK: %[[V8:.*]] = ptrtoint ptr %[[V2]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V8]], i32 1, i64 %[[V4]], i32 1, i64 %[[V6]])

// CHECK: define linkonce_odr noundef ptr @_ZN2SAC2EOS_(ptr noundef nonnull align 8 dereferenceable(16) %[[THIS:.*]], ptr noundef nonnull align 8 dereferenceable(16) %0)
// CHECK: %[[RETVAL:.*]] = alloca ptr, align 8
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK: %[[_ADDR:.*]] = alloca ptr, align 8
// CHECK: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK: store ptr %[[V0:.*]], ptr %[[_ADDR]], align 8
// CHECK: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK: store ptr %[[THIS1]], ptr %[[RETVAL]], align 8
// CHECK: %[[M0:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[THIS1]], i32 0, i32 0
// CHECK: %[[V1:.*]] = load ptr, ptr %[[_ADDR]], align 8
// CHECK: %[[M02:.*]] = getelementptr inbounds nuw %[[STRUCT_SA]], ptr %[[V1]], i32 0, i32 0
// CHECK: %[[V2:.*]] = load ptr, ptr %[[M02]], align 8
// CHECK: %[[V3:.*]] = ptrtoint ptr %[[M02]] to i64
// CHECK: %[[V4:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V3]], i64 50)
// CHECK: %[[V5:.*]] = ptrtoint ptr %[[M0]] to i64
// CHECK: %[[V6:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V5]], i64 50)
// CHECK: %[[V8:.*]] = ptrtoint ptr %[[V2]] to i64
// CHECK: %[[V9:.*]] = call i64 @llvm.ptrauth.resign(i64 %[[V8]], i32 1, i64 %[[V4]], i32 1, i64 %[[V6]])

// CHECK: define linkonce_odr noundef ptr @_ZN14test_constexpr7DerivedINS_12BaseAddrDiscEEC2Ev(ptr noundef nonnull align 8 dereferenceable(40) %[[THIS:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK-NEXT: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK-NEXT: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK-NEXT: call noundef ptr @_ZN14test_constexpr12BaseAddrDiscC2Ev(ptr noundef nonnull align 8 dereferenceable(40) %[[THIS1]])
// CHECK-NEXT: ret ptr %[[THIS1]]

// CHECK: define linkonce_odr noundef ptr @_ZN14test_constexpr12BaseAddrDiscC2Ev(ptr noundef nonnull align 8 dereferenceable(40) %[[THIS:.*]])
// CHECK: %[[THIS_ADDR:.*]] = alloca ptr, align 8
// CHECK-NEXT: store ptr %[[THIS]], ptr %[[THIS_ADDR]], align 8
// CHECK-NEXT: %[[THIS1:.*]] = load ptr, ptr %[[THIS_ADDR]], align 8
// CHECK-NEXT: %[[PTR:.*]] = getelementptr inbounds %{{.*}}, ptr %[[THIS1]], i32 0, i32 0
// CHECK-NEXT: %[[V0:.*]] = ptrtoint ptr %[[PTR]] to i64
// CHECK-NEXT: %[[V1:.*]] = call i64 @llvm.ptrauth.blend(i64 %[[V0]], i64 50)
// CHECK-NEXT: %[[V2:.*]] = call i64 @llvm.ptrauth.sign(i64 ptrtoint (ptr @_ZN14test_constexpr1gE to i64), i32 1, i64 %[[V1]])
// CHECK-NEXT: %[[V3:.*]] = inttoptr i64 %[[V2]] to ptr
// CHECK-NEXT: store ptr %[[V3]], ptr %[[PTR]], align 8
// CHECK-NEXT: %[[D:.*]] = getelementptr inbounds %{{.*}}, ptr %[[THIS1]], i32 0, i32 4
// CHECK-NEXT: store i64 1, ptr %[[D]], align 8
// CHECK-NEXT: %[[C:.*]] = getelementptr inbounds %{{.*}}, ptr %[[THIS1]], i32 0, i32 3
// CHECK-NEXT: store i64 1, ptr %[[C]], align 8
// CHECK-NEXT: %[[B:.*]] = getelementptr inbounds %{{.*}}, ptr %[[THIS1]], i32 0, i32 2
// CHECK-NEXT: store i64 1, ptr %[[B]], align 8
// CHECK-NEXT: %[[A:.*]] = getelementptr inbounds %{{.*}}, ptr %[[THIS1]], i32 0, i32 1
// CHECK-NEXT: store i64 1, ptr %[[A]], align 8
// CHECK-NEXT: ret ptr %[[THIS1]]
