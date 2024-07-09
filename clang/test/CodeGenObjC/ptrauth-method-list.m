// RUN: %clang_cc1 -mllvm -ptrauth-emit-wrapper-globals=0 -fptrauth-calls -fobjc-arc -fblocks -fobjc-runtime=ios-7 -triple arm64-apple-ios -emit-llvm -o - %s | FileCheck %s

// CHECK: @"_OBJC_$_CLASS_METHODS_C" = internal global { i32, i32, [2 x %struct._objc_method] } { i32 24, i32 2, [2 x %struct._objc_method] [
// CHECK-SAME: %struct._objc_method { ptr @OBJC_METH_VAR_NAME_, ptr @OBJC_METH_VAR_TYPE_, ptr ptrauth (ptr @"\01+[C pm1]", i32 0, i64 0, ptr getelementptr inbounds ({ i32, i32, [2 x %struct._objc_method] }, ptr @"_OBJC_$_CLASS_METHODS_C", i32 0, i32 2, i32 0, i32 2)) },
// CHECK-SAME: %struct._objc_method { ptr @OBJC_METH_VAR_NAME_.1, ptr @OBJC_METH_VAR_TYPE_, ptr ptrauth (ptr @"\01+[C m1]", i32 0, i64 0, ptr getelementptr inbounds ({ i32, i32, [2 x %struct._objc_method] }, ptr @"_OBJC_$_CLASS_METHODS_C", i32 0, i32 2, i32 1, i32 2)) }
// CHECK-SAME: ] }, section "__DATA, __objc_const",
// CHECK: @"_OBJC_$_INSTANCE_METHODS_C" = internal global { i32, i32, [2 x %struct._objc_method] } { i32 24, i32 2, [2 x %struct._objc_method] [
// CHECK-SAME: %struct._objc_method { ptr @OBJC_METH_VAR_NAME_.3, ptr @OBJC_METH_VAR_TYPE_, ptr ptrauth (ptr @"\01-[C pm0]", i32 0, i64 0, ptr getelementptr inbounds ({ i32, i32, [2 x %struct._objc_method] }, ptr @"_OBJC_$_INSTANCE_METHODS_C", i32 0, i32 2, i32 0, i32 2)) },
// CHECK-SAME: %struct._objc_method { ptr @OBJC_METH_VAR_NAME_.4, ptr @OBJC_METH_VAR_TYPE_, ptr ptrauth (ptr @"\01-[C m0]", i32 0, i64 0, ptr getelementptr inbounds ({ i32, i32, [2 x %struct._objc_method] }, ptr @"_OBJC_$_INSTANCE_METHODS_C", i32 0, i32 2, i32 1, i32 2)) }
// CHECK-SAME: ] }, section "__DATA, __objc_const",

@protocol P
- (void) pm0;
+ (void) pm1;
@end

@interface C<P>
- (void) m0;
+ (void) m1;
@end

@implementation C
- (void) pm0 {}
+ (void) pm1 {}
- (void) m0 {}
+ (void) m1 {}
@end

void test_method_list(C *c) {
  [c m0];
  [C m1];
}
