// RUN: %clang_cc1 -fptrauth-calls -fobjc-sign-class-ro -fobjc-arc -fobjc-runtime=ios-7 -triple arm64-apple-ios -emit-llvm -o - %s | FileCheck %s

// CHECK: @"OBJC_CLASS_$_Foo" = global %struct._class_t { %struct._class_t* @"OBJC_METACLASS_$_Foo", %struct._class_t* null, %struct._objc_cache* @_objc_empty_cache, i8* (i8*, i8*)** null, %struct._class_ro_t* bitcast ({ i8*, i32, i64, i64 }* @"_OBJC_CLASS_RO_$_Foo.ptrauth" to %struct._class_ro_t*) }, section "__DATA, __objc_data", align 8
// CHECK: @"OBJC_METACLASS_$_Foo" = global %struct._class_t { %struct._class_t* @"OBJC_METACLASS_$_Foo", %struct._class_t* @"OBJC_CLASS_$_Foo", %struct._objc_cache* @_objc_empty_cache, i8* (i8*, i8*)** null, %struct._class_ro_t* bitcast ({ i8*, i32, i64, i64 }* @"_OBJC_METACLASS_RO_$_Foo.ptrauth" to %struct._class_ro_t*) }, section "__DATA, __objc_data", align 8
// CHECK: @"_OBJC_CLASS_RO_$_Foo.ptrauth" = private constant { i8*, i32, i64, i64 } { i8* bitcast (%struct._class_ro_t* @"_OBJC_CLASS_RO_$_Foo" to i8*), i32 2, i64 ptrtoint (%struct._class_ro_t** getelementptr inbounds (%struct._class_t, %struct._class_t* @"OBJC_CLASS_$_Foo", i32 0, i32 4) to i64), i64 25080 }, section "llvm.ptrauth", align 8
// CHECK: @"_OBJC_METACLASS_RO_$_Foo.ptrauth" = private constant { i8*, i32, i64, i64 } { i8* bitcast (%struct._class_ro_t* @"_OBJC_METACLASS_RO_$_Foo" to i8*), i32 2, i64 ptrtoint (%struct._class_ro_t** getelementptr inbounds (%struct._class_t, %struct._class_t* @"OBJC_METACLASS_$_Foo", i32 0, i32 4) to i64), i64 25080 }, section "llvm.ptrauth", align 8

// CHECK: {i32, 1, !"Objective-C Enforce ClassRO Pointer Signing", i32 16}

@interface C
- (void) m0;
@end

@implementation C
- (void)m0 {}
@end

void test_sign_class_ro(C *c) {
  [c m0];
}
