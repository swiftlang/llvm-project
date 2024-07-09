// RUN: %clang_cc1 -triple arm64-apple-ios \
// RUN:   -fptrauth-calls -fptrauth-intrinsics -emit-llvm -fblocks \
// RUN:   %s -debug-info-kind=limited -o - | FileCheck %s

// Constant initializers for data pointers.
extern int external_int;

int *__ptrauth(1, 0, 1234) g1 = &external_int;
int *__ptrauth(1, 0, 1235, "isa-pointer") g2 = &external_int;
// CHECK: !DIDerivedType(tag: DW_TAG_LLVM_ptrauth_type,
// CHECK-SAME:           ptrAuthKey: 1,
// CHECK-SAME:           ptrAuthIsAddressDiscriminated: false,
// CHECK-SAME:           ptrAuthExtraDiscriminator: 1235,
// CHECK-SAME:           ptrAuthIsaPointer: true,
// CHECK-SAME:           ptrAuthAuthenticatesNullValues: false)

// CHECK: !DIDerivedType(tag: DW_TAG_LLVM_ptrauth_type,
// CHECK-SAME:           ptrAuthKey: 1,
// CHECK-SAME:           ptrAuthIsAddressDiscriminated: false,
// CHECK-SAME:           ptrAuthExtraDiscriminator: 1234,
// CHECK-SAME:           ptrAuthIsaPointer: false,
// CHECK-SAME:           ptrAuthAuthenticatesNullValues: false)

struct A {
  int value;
};
struct A *createA(void);

void f() {
  __block struct A *__ptrauth(1, 1, 1236) ptr = createA();
  ^{
    (void)ptr->value;
  }();
}
// CHECK: !DIDerivedType(tag: DW_TAG_LLVM_ptrauth_type,
// CHECK-SAME:           ptrAuthKey: 1,
// CHECK-SAME:           ptrAuthIsAddressDiscriminated: true,
// CHECK-SAME:           ptrAuthExtraDiscriminator: 1236,
// CHECK-SAME:           ptrAuthIsaPointer: false,
// CHECK-SAME:           ptrAuthAuthenticatesNullValues: false)

void f2() {
  __block struct A *__ptrauth(1, 1, 1237, "isa-pointer") ptr = createA();
  ^{
    (void)ptr->value;
  }();
}
// CHECK: !DIDerivedType(tag: DW_TAG_LLVM_ptrauth_type,
// CHECK-SAME:           ptrAuthKey: 1,
// CHECK-SAME:           ptrAuthIsAddressDiscriminated: true,
// CHECK-SAME:           ptrAuthExtraDiscriminator: 1237,
// CHECK-SAME:           ptrAuthIsaPointer: true,
// CHECK-SAME:           ptrAuthAuthenticatesNullValues: false)

void f3() {
  __block struct A *__ptrauth(1, 1, 1238, "isa-pointer, authenticates-null-values") ptr = createA();
  ^{
    (void)ptr->value;
  }();
}
// CHECK: !DIDerivedType(tag: DW_TAG_LLVM_ptrauth_type,
// CHECK-SAME:           ptrAuthKey: 1,
// CHECK-SAME:           ptrAuthIsAddressDiscriminated: true,
// CHECK-SAME:           ptrAuthExtraDiscriminator: 1238,
// CHECK-SAME:           ptrAuthIsaPointer: true,
// CHECK-SAME:           ptrAuthAuthenticatesNullValues: true)

void f4() {
  __block struct A *__ptrauth(1, 1, 1239, "authenticates-null-values") ptr = createA();
  ^{
    (void)ptr->value;
  }();
}
// CHECK: !DIDerivedType(tag: DW_TAG_LLVM_ptrauth_type,
// CHECK-SAME:           ptrAuthKey: 1,
// CHECK-SAME:           ptrAuthIsAddressDiscriminated: true,
// CHECK-SAME:           ptrAuthExtraDiscriminator: 1239,
// CHECK-SAME:           ptrAuthIsaPointer: false,
// CHECK-SAME:           ptrAuthAuthenticatesNullValues: true)
