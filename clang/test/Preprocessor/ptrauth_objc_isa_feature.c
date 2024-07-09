// RUN: %clang_cc1 %s -E -triple=arm64-- -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-strip | FileCheck %s --check-prefixes=ENABLED
// RUN: %clang_cc1 %s -E -triple=arm64-- -fptrauth-calls -fptrauth-objc-isa-mode=sign-and-auth  | FileCheck %s --check-prefixes=ENABLED
// RUN: %clang_cc1 %s -E -triple=arm64-- -fptrauth-calls | FileCheck %s --check-prefixes=DISABLED

#if __has_feature(ptrauth_objc_isa)
// ENABLED: has_ptrauth_objc_isa
void has_ptrauth_objc_isa() {}
#else
// DISABLED: no_ptrauth_objc_isa
void no_ptrauth_objc_isa() {}
#endif
