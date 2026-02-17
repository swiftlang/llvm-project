// RUN: %clang_cc1 -O0 -triple x86_64 -fbounds-safety -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -O0 -triple x86_64 -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -emit-llvm %s -o - | FileCheck %s

#include <ptrcheck.h>
struct Item {
  void *__counted_by(len) buf;
  unsigned len;
};

// TEST 1: Basic Access
// CHECK-LABEL: @TestAccess(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[S_ADDR:%.*]] = alloca ptr, align 8
// CHECK-NEXT:    [[I_ADDR:%.*]] = alloca i32, align 4
// CHECK-NEXT:    [[AGG_TEMP:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable", align 8
// CHECK-NEXT:    [[AGG_TEMP1:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    store ptr [[S:%.*]], ptr [[S_ADDR]], align 8
// CHECK-NEXT:    store i32 [[I:%.*]], ptr [[I_ADDR]], align 4
// CHECK-NEXT:    [[TMP0:%.*]] = load ptr, ptr [[S_ADDR]], align 8
// CHECK-NEXT:    [[LEN:%.*]] = getelementptr inbounds nuw %struct.Item, ptr [[TMP0]], i32 0, i32 1
// CHECK-NEXT:    [[TMP1:%.*]] = load i32, ptr [[LEN]], align 8
// CHECK-NEXT:    [[BUF:%.*]] = getelementptr inbounds nuw %struct.Item, ptr [[TMP0]], i32 0, i32 0
// CHECK-NEXT:    [[TMP2:%.*]] = load ptr, ptr [[BUF]], align 8
// CHECK-NEXT:    [[IDX_EXT:%.*]] = zext i32 [[TMP1]] to i64
// CHECK-NEXT:    [[ADD_PTR:%.*]] = getelementptr inbounds nuw i8, ptr [[TMP2]], i64 [[IDX_EXT]]
// CHECK-NEXT:    [[TMP3:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP1]], i32 0, i32 0
// CHECK-NEXT:    store ptr [[TMP2]], ptr [[TMP3]], align 8
// CHECK-NEXT:    [[TMP4:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP1]], i32 0, i32 1
// CHECK-NEXT:    store ptr [[ADD_PTR]], ptr [[TMP4]], align 8
// CHECK-NEXT:    [[TMP5:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP1]], i32 0, i32 2
// CHECK-NEXT:    store ptr [[TMP2]], ptr [[TMP5]], align 8
// CHECK-NEXT:    [[WIDE_PTR_PTR_ADDR:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP1]], i32 0, i32 0
// CHECK-NEXT:    [[WIDE_PTR_PTR:%.*]] = load ptr, ptr [[WIDE_PTR_PTR_ADDR]], align 8
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP1]], i32 0, i32 1
// CHECK-NEXT:    [[WIDE_PTR_UB:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR]], align 8
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP1]], i32 0, i32 2
// CHECK-NEXT:    [[WIDE_PTR_LB:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR]], align 8
// CHECK-NEXT:    [[TMP6:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP]], i32 0, i32 0
// CHECK-NEXT:    store ptr [[WIDE_PTR_PTR]], ptr [[TMP6]], align 8
// CHECK-NEXT:    [[TMP7:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP]], i32 0, i32 1
// CHECK-NEXT:    store ptr [[WIDE_PTR_UB]], ptr [[TMP7]], align 8
// CHECK-NEXT:    [[TMP8:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP]], i32 0, i32 2
// CHECK-NEXT:    store ptr [[WIDE_PTR_LB]], ptr [[TMP8]], align 8
// CHECK-NEXT:    [[WIDE_PTR_PTR_ADDR2:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP]], i32 0, i32 0
// CHECK-NEXT:    [[WIDE_PTR_PTR3:%.*]] = load ptr, ptr [[WIDE_PTR_PTR_ADDR2]], align 8
// CHECK-NEXT:    [[TMP9:%.*]] = load i32, ptr [[I_ADDR]], align 4
// CHECK-NEXT:    [[IDXPROM:%.*]] = sext i32 [[TMP9]] to i64
// CHECK-NEXT:    [[ARRAYIDX:%.*]] = getelementptr i8, ptr [[WIDE_PTR_PTR3]], i64 [[IDXPROM]]
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR4:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP]], i32 0, i32 1
// CHECK-NEXT:    [[WIDE_PTR_UB5:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR4]], align 8
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR6:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP]], i32 0, i32 2
// CHECK-NEXT:    [[WIDE_PTR_LB7:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR6]], align 8
// CHECK-NEXT:    [[TMP10:%.*]] = icmp ult ptr [[ARRAYIDX]], [[WIDE_PTR_UB5]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    br i1 [[TMP10]], label [[CONT:%.*]], label [[TRAP:%.*]], {{!prof ![0-9]+}}, {{!annotation ![0-9]+}}
// CHECK:       trap:
// CHECK-NEXT:    call void @llvm.ubsantrap(i8 25) #[[ATTR3:[0-9]+]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    unreachable, {{!annotation ![0-9]+}}
// CHECK:       cont:
// CHECK-NEXT:    [[TMP11:%.*]] = icmp uge ptr [[ARRAYIDX]], [[WIDE_PTR_LB7]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    br i1 [[TMP11]], label [[CONT9:%.*]], label [[TRAP8:%.*]], {{!prof ![0-9]+}}, {{!annotation ![0-9]+}}
// CHECK:       trap8:
// CHECK-NEXT:    call void @llvm.ubsantrap(i8 25) #[[ATTR3]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    unreachable, {{!annotation ![0-9]+}}
// CHECK:       cont9:
// CHECK-NEXT:    [[TMP12:%.*]] = load i8, ptr [[ARRAYIDX]], align 1
// CHECK-NEXT:    ret i8 [[TMP12]]
//
char TestAccess(struct Item *s, int i) {
  return ((char *)s->buf)[i];
}

// TEST 2: Increment/Decrement
// CHECK-LABEL: @TestInc(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[S_ADDR:%.*]] = alloca ptr, align 8
// CHECK-NEXT:    [[AGG_TEMP:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[TMP:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[AGG_TEMP2:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[AGG_TEMP3:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[AGG_TEMP6:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[AGG_TEMP9:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[AGG_TEMP17:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    [[AGG_TEMP20:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable", align 8
// CHECK-NEXT:    [[AGG_TEMP21:%.*]] = alloca %"__bounds_safety::wide_ptr.bidi_indexable.0", align 8
// CHECK-NEXT:    store ptr [[S:%.*]], ptr [[S_ADDR]], align 8
// CHECK-NEXT:    [[TMP0:%.*]] = load ptr, ptr [[S_ADDR]], align 8
// CHECK-NEXT:    [[BUF:%.*]] = getelementptr inbounds nuw %struct.Item, ptr [[TMP0]], i32 0, i32 0
// CHECK-NEXT:    [[LEN:%.*]] = getelementptr inbounds nuw %struct.Item, ptr [[TMP0]], i32 0, i32 1
// CHECK-NEXT:    [[TMP1:%.*]] = load i32, ptr [[LEN]], align 8
// CHECK-NEXT:    [[TMP2:%.*]] = load ptr, ptr [[BUF]], align 8
// CHECK-NEXT:    [[IDX_EXT:%.*]] = zext i32 [[TMP1]] to i64
// CHECK-NEXT:    [[ADD_PTR:%.*]] = getelementptr inbounds nuw i8, ptr [[TMP2]], i64 [[IDX_EXT]]
// CHECK-NEXT:    [[TMP3:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[TMP]], i32 0, i32 0
// CHECK-NEXT:    store ptr [[TMP2]], ptr [[TMP3]], align 8
// CHECK-NEXT:    [[TMP4:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[TMP]], i32 0, i32 1
// CHECK-NEXT:    store ptr [[ADD_PTR]], ptr [[TMP4]], align 8
// CHECK-NEXT:    [[TMP5:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[TMP]], i32 0, i32 2
// CHECK-NEXT:    store ptr [[TMP2]], ptr [[TMP5]], align 8
// CHECK-NEXT:    [[TMP6:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[TMP]], i32 0, i32 0
// CHECK-NEXT:    [[TMP7:%.*]] = load ptr, ptr [[TMP6]], align 8
// CHECK-NEXT:    [[BOUND_PTR_ARITH:%.*]] = getelementptr i8, ptr [[TMP7]], i64 1
// CHECK-NEXT:    [[TMP8:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP]], i32 0, i32 0
// CHECK-NEXT:    store ptr [[BOUND_PTR_ARITH]], ptr [[TMP8]], align 8
// CHECK-NEXT:    [[TMP9:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[TMP]], i32 0, i32 1
// CHECK-NEXT:    [[TMP10:%.*]] = load ptr, ptr [[TMP9]], align 8
// CHECK-NEXT:    [[TMP11:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP]], i32 0, i32 1
// CHECK-NEXT:    store ptr [[TMP10]], ptr [[TMP11]], align 8
// CHECK-NEXT:    [[TMP12:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[TMP]], i32 0, i32 2
// CHECK-NEXT:    [[TMP13:%.*]] = load ptr, ptr [[TMP12]], align 8
// CHECK-NEXT:    [[TMP14:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP]], i32 0, i32 2
// CHECK-NEXT:    store ptr [[TMP13]], ptr [[TMP14]], align 8
// CHECK-NEXT:    [[TMP15:%.*]] = load ptr, ptr [[S_ADDR]], align 8
// CHECK-NEXT:    [[LEN1:%.*]] = getelementptr inbounds nuw %struct.Item, ptr [[TMP15]], i32 0, i32 1
// CHECK-NEXT:    [[TMP16:%.*]] = load i32, ptr [[LEN1]], align 8
// CHECK-NEXT:    [[SUB:%.*]] = sub i32 [[TMP16]], 1
// CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[AGG_TEMP2]], ptr align 8 [[AGG_TEMP]], i64 24, i1 false), {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR_ADDR:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP2]], i32 0, i32 0, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR:%.*]] = load ptr, ptr [[WIDE_PTR_PTR_ADDR]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP2]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP2]], i32 0, i32 2, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[AGG_TEMP3]], ptr align 8 [[AGG_TEMP]], i64 24, i1 false), {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR4:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP3]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB5:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR4]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[CMP:%.*]] = icmp ule ptr [[WIDE_PTR_PTR]], [[WIDE_PTR_UB5]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    br i1 [[CMP]], label [[LAND_LHS_TRUE:%.*]], label [[LAND_END:%.*]], {{!annotation ![0-9]+}}
// CHECK:       land.lhs.true:
// CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[AGG_TEMP6]], ptr align 8 [[AGG_TEMP]], i64 24, i1 false), {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR7:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP6]], i32 0, i32 2, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB8:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR7]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[AGG_TEMP9]], ptr align 8 [[AGG_TEMP]], i64 24, i1 false), {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR_ADDR10:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP9]], i32 0, i32 0, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR11:%.*]] = load ptr, ptr [[WIDE_PTR_PTR_ADDR10]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR12:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP9]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB13:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR12]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR14:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP9]], i32 0, i32 2, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB15:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR14]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[CMP16:%.*]] = icmp ule ptr [[WIDE_PTR_LB8]], [[WIDE_PTR_PTR11]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    br i1 [[CMP16]], label [[LAND_RHS:%.*]], label [[LAND_END]], {{!annotation ![0-9]+}}
// CHECK:       land.rhs:
// CHECK-NEXT:    [[CONV:%.*]] = zext i32 [[SUB]] to i64, {{!annotation ![0-9]+}}
// CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[AGG_TEMP17]], ptr align 8 [[AGG_TEMP]], i64 24, i1 false), {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR18:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP17]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB19:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR18]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    call void @llvm.memcpy.p0.p0.i64(ptr align 8 [[AGG_TEMP21]], ptr align 8 [[AGG_TEMP]], i64 24, i1 false), {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR_ADDR22:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP21]], i32 0, i32 0, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR23:%.*]] = load ptr, ptr [[WIDE_PTR_PTR_ADDR22]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR24:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP21]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB25:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR24]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR26:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable.0", ptr [[AGG_TEMP21]], i32 0, i32 2, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB27:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR26]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[TMP17:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP20]], i32 0, i32 0, {{!annotation ![0-9]+}}
// CHECK-NEXT:    store ptr [[WIDE_PTR_PTR23]], ptr [[TMP17]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[TMP18:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP20]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    store ptr [[WIDE_PTR_UB25]], ptr [[TMP18]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[TMP19:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP20]], i32 0, i32 2, {{!annotation ![0-9]+}}
// CHECK-NEXT:    store ptr [[WIDE_PTR_LB27]], ptr [[TMP19]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR_ADDR28:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP20]], i32 0, i32 0, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_PTR29:%.*]] = load ptr, ptr [[WIDE_PTR_PTR_ADDR28]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB_ADDR30:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP20]], i32 0, i32 1, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_UB31:%.*]] = load ptr, ptr [[WIDE_PTR_UB_ADDR30]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB_ADDR32:%.*]] = getelementptr inbounds nuw %"__bounds_safety::wide_ptr.bidi_indexable", ptr [[AGG_TEMP20]], i32 0, i32 2, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[WIDE_PTR_LB33:%.*]] = load ptr, ptr [[WIDE_PTR_LB_ADDR32]], align 8, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[SUB_PTR_LHS_CAST:%.*]] = ptrtoint ptr [[WIDE_PTR_UB19]] to i64, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[SUB_PTR_RHS_CAST:%.*]] = ptrtoint ptr [[WIDE_PTR_PTR29]] to i64, {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[SUB_PTR_SUB:%.*]] = sub i64 [[SUB_PTR_LHS_CAST]], [[SUB_PTR_RHS_CAST]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    [[CMP34:%.*]] = icmp sle i64 [[CONV]], [[SUB_PTR_SUB]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    br label [[LAND_END]], {{!annotation ![0-9]+}}
// CHECK:       land.end:
// CHECK-NEXT:    [[TMP20:%.*]] = phi i1 [ false, [[LAND_LHS_TRUE]] ], [ false, [[ENTRY:%.*]] ], [ [[CMP34]], [[LAND_RHS]] ], {{!annotation ![0-9]+}}
// CHECK-NEXT:    br i1 [[TMP20]], label [[CONT:%.*]], label [[TRAP:%.*]], {{!prof ![0-9]+}}, {{!annotation ![0-9]+}}
// CHECK:       trap:
// CHECK-NEXT:    call void @llvm.ubsantrap(i8 25) #[[ATTR3]], {{!annotation ![0-9]+}}
// CHECK-NEXT:    unreachable, {{!annotation ![0-9]+}}
// CHECK:       cont:
// CHECK-NEXT:    [[TMP21:%.*]] = load ptr, ptr [[BUF]], align 8
// CHECK-NEXT:    [[INCDEC_PTR:%.*]] = getelementptr inbounds nuw i8, ptr [[TMP21]], i32 1
// CHECK-NEXT:    store ptr [[INCDEC_PTR]], ptr [[BUF]], align 8
// CHECK-NEXT:    [[TMP22:%.*]] = load i32, ptr [[LEN1]], align 8
// CHECK-NEXT:    [[DEC:%.*]] = add i32 [[TMP22]], -1
// CHECK-NEXT:    store i32 [[DEC]], ptr [[LEN1]], align 8
// CHECK-NEXT:    ret void
//
void TestInc(struct Item *s) {
  s->buf++;
  s->len--;
}
