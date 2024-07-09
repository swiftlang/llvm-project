// RUN: %clang_cc1 -cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-auth -fptrauth-objc-isa-masking -triple arm64-apple-ios -emit-llvm -O0 -disable-llvm-passes -o - %s | FileCheck %s
#if __has_feature(ptrauth_qualifier_authentication_mode)
#define test_ptrauth(a...) __ptrauth(a)
#else
#define test_ptrauth(a...)
#endif
typedef struct {
  void *test_ptrauth(2, 0, 0, "isa-pointer") isa_0_0;
  void *test_ptrauth(2, 1, 0, "isa-pointer") isa_1_0;
  // Just using distinct discriminators
  void *test_ptrauth(2, 0, 13, "isa-pointer") isa_0_13;
  void *test_ptrauth(2, 1, 17, "isa-pointer") isa_1_17;
  void *test_ptrauth(2, 1, 17, "") non_isa;

} TestStruct1;

// CHECK: @objc_absolute_packed_isa_class_mask = external global i8

// CHECK: define void @testWrite(
void testWrite(TestStruct1 *t, void *p) {
  t->isa_0_0 = p;
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]], i32 2, i64 0)
  // CHECK: [[RETAGGED:%.*]] = or i64 [[SIGNED]], [[TAG_BITS]]

  t->isa_1_0 = p;
  // CHECK: resign.nonnull1:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]], i32 2
  // CHECK: [[RETAGGED:%.*]] = or i64 [[SIGNED]], [[TAG_BITS]]

  t->isa_0_13 = p;
  // CHECK: resign.nonnull3:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]], i32 2, i64 13)
  // CHECK: [[RETAGGED:%.*]] = or i64 [[SIGNED]], [[TAG_BITS]]

  t->isa_1_17 = p;
  // CHECK: resign.nonnull5:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]], i32 2
  // CHECK: [[RETAGGED:%.*]] = or i64 [[SIGNED]], [[TAG_BITS]]

  t->non_isa = p;
  // CHECK: resign.nonnull7:
  // CHECK: [[POINTER:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]], i32 2
}

// CHECK: define void @testRead
void testRead(TestStruct1 *t, void *p[5]) {
  p[0] = t->isa_0_0;
  // CHECK: resign.nonnull:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[POINTER]], i32 2, i64 0)
  // CHECK: [[RETAGGED:%.*]] = or i64 [[AUTHED]], [[TAG_BITS]]

  p[1] = t->isa_1_0;
  // CHECK: resign.nonnull1:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[POINTER]], i32 2,
  // CHECK: [[RETAGGED:%.*]] = or i64 [[AUTHED]], [[TAG_BITS]]

  p[2] = t->isa_0_13;
  // CHECK: resign.nonnull4:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[POINTER]], i32 2, i64 13)
  // CHECK: [[RETAGGED:%.*]] = or i64 [[AUTHED]], [[TAG_BITS]]

  p[3] = t->isa_1_17;
  // CHECK: resign.nonnull7:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[POINTER]]
  // CHECK: [[RETAGGED:%.*]] = or i64 [[AUTHED]], [[TAG_BITS]]

  p[4] = t->non_isa;
  // CHECK: resign.nonnull10:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[VALUE]]
}

// CHECK: define void @conditional_mask_write
void conditional_mask_write(TestStruct1 *t, void *p[2]) {
  if (p[0]) {
    t->isa_0_0 = p[0];
    // CHECK: [[VALUE:%.*]] = ptrtoint ptr
    // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
    // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
    // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]]
    // CHECK: [[RETAGGED:%.*]] = or i64 [[SIGNED]], [[TAG_BITS]]
  }
  t->isa_1_0 = p[1];
  // CHECK: resign.nonnull3:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[SIGNED:%.*]] = call i64 @llvm.ptrauth.sign(i64 [[POINTER]]
  // CHECK: [[RETAGGED:%.*]] = or i64 [[SIGNED]], [[TAG_BITS]]
}

void conditional_mask_read(TestStruct1 *t, void *p[2]) {
  if (p[0]) {
    p[0] = t->isa_0_0;
    // CHECK: [[VALUE:%.*]] = ptrtoint ptr
    // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
    // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
    // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[POINTER]]
    // CHECK: [[RETAGGED:%.*]] = or i64 [[AUTHED]], [[TAG_BITS]]
  }
  p[1] = t->isa_1_0;
  // CHECK: resign.nonnull2:
  // CHECK: [[VALUE:%.*]] = ptrtoint ptr
  // CHECK: [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
  // CHECK: [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
  // CHECK: [[AUTHED:%.*]] = call i64 @llvm.ptrauth.auth(i64 [[POINTER]]
  // CHECK: [[RETAGGED:%.*]] = or i64 [[AUTHED]], [[TAG_BITS]]
}

// CHECK-LABEL: define void @copy
void copy(TestStruct1 *x, TestStruct1 *y) {
  *x = *y;
}

// CHECK: define linkonce_odr hidden void @__copy_assignment
// CHECK: resign.nonnull:
// CHECK:  [[VALUE:%.*]] = ptrtoint ptr [[TMP:%.*]] to i64
// CHECK:  [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
// CHECK:  [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
// CHECK:  [[RESIGNED:%.*]] = call i64 @llvm.ptrauth.resign(i64 [[POINTER]], i32 2,
// CHECK:  [[RETAGGED:%.*]] = or i64 [[RESIGNED]], [[TAG_BITS]]
// CHECK:  [[ADDR1:%.*]] = ptrtoint ptr
// CHECK:  [[BLEND1:%.*]] = call i64 @llvm.ptrauth.blend(i64 [[ADDR1]], i64 17)
// CHECK:  [[ADDR2:%.*]] = ptrtoint ptr
// CHECK:  [[BLEND2:%.*]] = call i64 @llvm.ptrauth.blend(i64 [[ADDR2]], i64 17)
// CHECK:  [[VALUE:%.*]] = ptrtoint ptr
// CHECK:  [[POINTER:%.*]] = and i64 [[VALUE]], ptrtoint (ptr @objc_absolute_packed_isa_class_mask to i64)
// CHECK:  [[TAG_BITS:%.*]] = xor i64 [[VALUE]], [[POINTER]]
// CHECK:  [[RESIGNED:%.*]] = call i64 @llvm.ptrauth.resign(i64 [[POINTER]], i32 2, i64 [[BLEND1]], i32 2, i64 [[BLEND2]])
// CHECK:  [[RETAGGED:%.*]] = or i64 [[RESIGNED]], [[TAG_BITS]]
// CHECK:  [[ADDR1:%.*]] = ptrtoint ptr
// CHECK:  [[BLEND1:%.*]] = call i64 @llvm.ptrauth.blend(i64 [[ADDR1]], i64 17)
// CHECK:  [[ADDR2:%.*]] = ptrtoint ptr
// CHECK:  [[BLEND2:%.*]] = call i64 @llvm.ptrauth.blend(i64 [[ADDR2]], i64 17)
// CHECK:  [[POINTER:%.*]] = ptrtoint ptr
// CHECK:  [[RESIGNED:%.*]] = call i64 @llvm.ptrauth.resign(i64 [[POINTER]], i32 2, i64 [[BLEND1]], i32 2, i64 [[BLEND2]])
