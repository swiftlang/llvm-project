// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -I %S/Inputs -Wno-objc-root-class -fptrauth-intrinsics -fptrauth-calls -triple arm64-apple-ios -fobjc-runtime=ios-12.2 -emit-llvm -fblocks -fobjc-arc -fobjc-runtime-has-weak -O2 -disable-llvm-passes -o - %s -fptrauth-objc-isa-mode=strip | FileCheck --check-prefix=CHECK-STRIP %s
// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -I %S/Inputs -Wno-objc-root-class -fptrauth-intrinsics -fptrauth-calls -triple arm64-apple-ios -fobjc-runtime=ios-12.2 -emit-llvm -fblocks -fobjc-arc -fobjc-runtime-has-weak -O2 -disable-llvm-passes -o - %s -fptrauth-objc-isa-mode=sign-and-strip | FileCheck --check-prefix=CHECK-SIGN-AND-STRIP %s
// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -I %S/Inputs -Wno-objc-root-class -fptrauth-intrinsics -fptrauth-calls -triple arm64-apple-ios -fobjc-runtime=ios-12.2 -emit-llvm -fblocks -fobjc-arc -fobjc-runtime-has-weak -O2 -disable-llvm-passes -o - %s -fptrauth-objc-isa-mode=sign-and-auth | FileCheck --check-prefix=CHECK-SIGN-AND-AUTH %s
// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -I %S/Inputs -Wno-objc-root-class -fptrauth-intrinsics -fptrauth-calls -triple arm64-apple-ios -fobjc-runtime=ios-12.2 -emit-llvm -fblocks -fobjc-arc -fobjc-runtime-has-weak -O2 -disable-llvm-passes -o - %s | FileCheck --check-prefix=CHECK-DISABLED %s

#include <ptrauth.h>
_Static_assert(!__has_feature(ptrauth_objc_isa_masking), "wat");
#if __has_feature(ptrauth_qualifier_authentication_mode)

@class NSString;
NSString *aString = @"foo";

// CHECK-SIGN-AND-AUTH: @.str = private unnamed_addr constant [4 x i8] c"foo\00", section "__TEXT,__cstring,cstring_literals"
// CHECK-SIGN-AND-AUTH: @_unnamed_cfstring_ = private global %struct.__NSConstantString_tag { ptr ptrauth (ptr @__CFConstantStringClassReference, i32 2, i64 27361, ptr @_unnamed_cfstring_), i32 1992, ptr @.str, i64 3 }, section "__DATA,__cfstring"
// CHECK-SIGN-AND-AUTH: @aString = global ptr @_unnamed_cfstring_

// CHECK-SIGN-AND-STRIP: @.str = private unnamed_addr constant [4 x i8] c"foo\00", section "__TEXT,__cstring,cstring_literals"
// CHECK-SIGN-AND-STRIP: @_unnamed_cfstring_ =  private global %struct.__NSConstantString_tag { ptr ptrauth (ptr @__CFConstantStringClassReference, i32 2, i64 27361, ptr @_unnamed_cfstring_), i32 1992, ptr @.str, i64 3 }, section "__DATA,__cfstring"
// CHECK-SIGN-AND-STRIP: @aString = global ptr @_unnamed_cfstring_

// CHECK-STRIP: @.str = private unnamed_addr constant [4 x i8] c"foo\00", section "__TEXT,__cstring,cstring_literals", align 1
// CHECK-STRIP: @_unnamed_cfstring_ = private global %struct.__NSConstantString_tag { ptr @__CFConstantStringClassReference, i32 1992, ptr @.str, i64 3 }, section "__DATA,__cfstring"
// CHECK-STRIP: @aString = global ptr @_unnamed_cfstring_

// CHECK-DISABLED: @.str = private unnamed_addr constant [4 x i8] c"foo\00", section "__TEXT,__cstring,cstring_literals", align 1
// CHECK-DISABLED: @_unnamed_cfstring_ = private global %struct.__NSConstantString_tag { ptr @__CFConstantStringClassReference, i32 1992, ptr @.str, i64 3 }, section "__DATA,__cfstring"
// CHECK-DISABLED: @aString = global ptr @_unnamed_cfstring_

#if __has_feature(ptrauth_objc_isa_signs)
int ptrauth_objc_isa_signs_global = 0; // Verifying compilation path
// CHECK-SIGN-AND-AUTH: @ptrauth_objc_isa_signs_global = global i32 0, align 4
// CHECK-SIGN-AND-STRIP: @ptrauth_objc_isa_signs_global = global i32 0, align 4
#if __has_feature(ptrauth_objc_isa_authenticates)
int ptrauth_objc_isa_signs_and_auths_global = 0; // Verifying compilation path
// CHECK-SIGN-AND-AUTH: @ptrauth_objc_isa_signs_and_auths_global = global i32 0, align 4
#elif __has_feature(ptrauth_objc_isa_strips)
int ptrauth_objc_isa_signs_and_strips_global = 0; // Verifying compilation path
                                                  // CHECK-SIGN-AND-STRIP: @ptrauth_objc_isa_signs_and_strips_global = global i32 0, align 4
#else
_Static_assert(false, "none of the tests should hit this path");
#endif
#else
_Static_assert(!__has_feature(ptrauth_objc_isa_authenticates), "Strip and auth is an invalid mode");
#if __has_feature(ptrauth_objc_isa_strips)
int ptrauth_objc_isa_strips_global = 0; // Verifying compilation path
                                        // CHECK-STRIP: @ptrauth_objc_isa_strips_global = global i32 0, align 4
#else
// Make sure that the isa features don't lie when objc isa signing is completely disabled
int ptrauth_objc_isa_disabled_global = 0; // Verifying compilation path
                                          // CHECK-DISABLED: @ptrauth_objc_isa_disabled_global = global i32 0, align 4
#endif
#endif

typedef struct {
  void *__ptrauth_objc_isa_pointer ptr_isa;
  __UINT64_TYPE__ __ptrauth_objc_isa_uintptr int_isa;
} TestStruct;

void testModes(TestStruct *instruct, TestStruct *outstruct);
void testModes(TestStruct *instruct, TestStruct *outstruct) {
  instruct->ptr_isa = *(void **)outstruct->ptr_isa;
  instruct->int_isa = *(__UINT64_TYPE__ *)outstruct->int_isa;
  // CHECK-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.strip
  // CHECK-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.strip
  // CHECK-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.strip
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.sign
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.strip
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-STRIP: [[TMP:%.*]] = call i64 @llvm.ptrauth.sign
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.auth
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.sign
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.auth
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.blend
  // CHECK-SIGN-AND-AUTH: [[TMP:%.*]] = call i64 @llvm.ptrauth.sign
}

#endif
