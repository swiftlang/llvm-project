// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -fptrauth-calls -triple arm64e-apple-ios %s -emit-llvm -o - | FileCheck %s

@implementation X
-(void)meth {}
@end

// By default, we should be signing the method list pointer and not emitting
// relative method lists.

// CHECK: @"_OBJC_$_INSTANCE_METHODS_X" = internal global { i32, i32, [1 x %struct._objc_method] } { i32 24, i32 1, [1 x %struct._objc_method] [%struct._objc_method { ptr @OBJC_METH_VAR_NAME_, ptr @OBJC_METH_VAR_TYPE_, ptr ptrauth (ptr @"\01-[X meth]", i32 0, i64 0, ptr getelementptr inbounds ({ i32, i32, [1 x %struct._objc_method] }, ptr @"_OBJC_$_INSTANCE_METHODS_X", i32 0, i32 2, i32 0, i32 2)) }] }, section "__DATA, __objc_const", align 8

// CHECK: @"_OBJC_CLASS_RO_$_X" = internal global %struct._class_ro_t { i32 2, i32 0, i32 0, ptr null, ptr @OBJC_CLASS_NAME_, ptr ptrauth (ptr @"_OBJC_$_INSTANCE_METHODS_X", i32 2, i64 49936, ptr getelementptr inbounds (%struct._class_ro_t, ptr @"_OBJC_CLASS_RO_$_X", i32 0, i32 5)), ptr null, ptr null, ptr null, ptr null }, section "__DATA, __objc_const", align 8
