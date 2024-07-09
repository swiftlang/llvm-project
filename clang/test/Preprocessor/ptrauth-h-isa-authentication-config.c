// RUN: %clang_cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=strip -fptrauth-objc-isa-masking -triple arm64-apple-ios -E -O0 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-STRIP %s
// RUN: %clang_cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-strip -fptrauth-objc-isa-masking -triple arm64-apple-ios -E -O0 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-SIGN-AND-STRIP %s
// RUN: %clang_cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-auth -fptrauth-objc-isa-masking -triple arm64-apple-ios -E -O0 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-SIGN-AND-AUTH %s
// RUN: %clang_cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=strip -triple arm64-apple-ios -E -O0 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-STRIP-NO-MASK %s
// RUN: %clang_cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-strip -triple arm64-apple-ios -E -O0 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-SIGN-AND-STRIP-NO-MASK %s
// RUN: %clang_cc1 -nostdsysteminc -fptrauth-intrinsics -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-auth -triple arm64-apple-ios -E -O0 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-SIGN-AND-AUTH-NO-MASK %s

#include <ptrauth.h>
#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)

const char *test = TO_STRING(__ptrauth_objc_isa_pointer);

// CHECK-STRIP: const char *test = "__ptrauth(ptrauth_key_objc_isa_pointer, 1, 0x6AE1 , \"strip\" \",isa-pointer\")";
// CHECK-SIGN-AND-STRIP: const char *test = "__ptrauth(ptrauth_key_objc_isa_pointer, 1, 0x6AE1 , \"sign-and-strip\" \",isa-pointer\")";
// CHECK-SIGN-AND-AUTH: const char *test = "__ptrauth(ptrauth_key_objc_isa_pointer, 1, 0x6AE1 , \"sign-and-auth\" \",isa-pointer\")";
// CHECK-STRIP-NO-MASK: const char *test = "__ptrauth(ptrauth_key_objc_isa_pointer, 1, 0x6AE1 , \"strip\" )";
// CHECK-SIGN-AND-STRIP-NO-MASK: const char *test = "__ptrauth(ptrauth_key_objc_isa_pointer, 1, 0x6AE1 , \"sign-and-strip\" )";
// CHECK-SIGN-AND-AUTH-NO-MASK: const char *test = "__ptrauth(ptrauth_key_objc_isa_pointer, 1, 0x6AE1 , \"sign-and-auth\" )";
