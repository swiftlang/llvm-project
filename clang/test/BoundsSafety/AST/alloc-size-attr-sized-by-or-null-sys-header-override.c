// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -ast-dump %s 2>&1 | FileCheck %S/alloc-size-attr-sized-by-or-null-sys-header.h
// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -ast-dump %s 2>&1 | FileCheck --check-prefix CHECK-HEADER %s
// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -ast-dump %s 2>&1 | FileCheck %s

// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -ast-dump %s 2>&1 | FileCheck %S/alloc-size-attr-sized-by-or-null-sys-header.h
// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -ast-dump %s 2>&1 | FileCheck --check-prefix CHECK-HEADER %s
// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -ast-dump %s 2>&1 | FileCheck %s

#include <ptrcheck.h>
#include "alloc-size-attr-sized-by-or-null-sys-header.h"

// This makes sure that checks with this prefix don't capture anything from the header
// CHECK-LABEL: NON_HEADER
void NON_HEADER();

__attribute__((alloc_size(1))) void * unnamed_param(unsigned size) {
    return (void*)0;
}
// CHECK-HEADER: |-FunctionDecl [[func_unnamed_param:0x[^ ]+]] {{.+}} unnamed_param 'void *__single __sized_by_or_null(function-parameter-0-0)(unsigned int)'
// CHECK-HEADER: | |-ParmVarDecl
// CHECK-HEADER: | `-AllocSizeAttr

// CHECK-LABEL: unnamed_param
// CHECK-SAME: 'void *__single __sized_by_or_null(size)(unsigned int)'
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_9:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-CompoundStmt
// CHECK-NEXT: {{^}}| | `-ReturnStmt
// CHECK-NEXT: {{^}}| |   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}| |     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}| |     | |-ImplicitCastExpr {{.+}} 'void *__single __sized_by_or_null(size)':'void *__single' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}| |     | | `-OpaqueValueExpr [[ove_3:0x[^ ]+]] {{.*}} 'void *'
// CHECK:      {{^}}| |     | |-OpaqueValueExpr [[ove_3]]
// CHECK-NEXT: {{^}}| |     | | `-CStyleCastExpr {{.+}} 'void *' <NullToPointer>
// CHECK-NEXT: {{^}}| |     | |   `-IntegerLiteral {{.+}} 0
// CHECK-NEXT: {{^}}| |     | `-OpaqueValueExpr [[ove_4:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |     |   `-ImplicitCastExpr {{.+}} 'unsigned int' <LValueToRValue>
// CHECK-NEXT: {{^}}| |     |     `-DeclRefExpr {{.+}} [[var_size_9]]
// CHECK-NEXT: {{^}}| |     |-OpaqueValueExpr [[ove_3]] {{.*}} 'void *'
// CHECK:      {{^}}| |     `-OpaqueValueExpr [[ove_4]] {{.*}} 'unsigned int'
// CHECK:      {{^}}| `-AllocSizeAttr

void * __sized_by_or_null(size) inherit_attr(unsigned size) {
    return (void*)0;
}

// CHECK-HEADER: |-FunctionDecl [[func_inherit_attr:0x[^ ]+]] {{.+}} inherit_attr 'void *__single __sized_by_or_null(size)(unsigned int)'
// CHECK-HEADER: | |-ParmVarDecl [[var_size_8:0x[^ ]+]]
// CHECK-HEADER: | `-AllocSizeAttr

// CHECK-LABEL: inherit_attr
// CHECK-SAME:  'void *__single __sized_by_or_null(size)(unsigned int)'
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_10:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-CompoundStmt
// CHECK-NEXT: {{^}}| | `-ReturnStmt
// CHECK-NEXT: {{^}}| |   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}| |     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}| |     | |-ImplicitCastExpr {{.+}} 'void *__single __sized_by_or_null(size)':'void *__single' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}| |     | | `-OpaqueValueExpr [[ove_5:0x[^ ]+]] {{.*}} 'void *'
// CHECK:      {{^}}| |     | |-OpaqueValueExpr [[ove_5]]
// CHECK-NEXT: {{^}}| |     | | `-CStyleCastExpr {{.+}} 'void *' <NullToPointer>
// CHECK-NEXT: {{^}}| |     | |   `-IntegerLiteral {{.+}} 0
// CHECK-NEXT: {{^}}| |     | `-OpaqueValueExpr [[ove_6:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |     |   `-ImplicitCastExpr {{.+}} 'unsigned int' <LValueToRValue>
// CHECK-NEXT: {{^}}| |     |     `-DeclRefExpr {{.+}} [[var_size_10]]
// CHECK-NEXT: {{^}}| |     |-OpaqueValueExpr [[ove_5]] {{.*}} 'void *'
// CHECK:      {{^}}| |     `-OpaqueValueExpr [[ove_6]] {{.*}} 'unsigned int'
// CHECK:      {{^}}| `-AllocSizeAttr

void * unnamed_param_inherit_attr(unsigned size) {
    return (void*)0;
}

// CHECK-HEADER: |-FunctionDecl [[func_unnamed_param_inherit_attr:0x[^ ]+]] {{.+}} unnamed_param_inherit_attr 'void *__single __sized_by_or_null(function-parameter-0-0)(unsigned int)'
// CHECK-HEADER: | |-ParmVarDecl
// CHECK-HEADER: | `-AllocSizeAttr

// rdar://131622509 This type is being overridden by the system header
// CHECK-LABEL: unnamed_param_inherit_attr
// CHECK-SAME: 'void *__single __sized_by_or_null(size)(unsigned int)'
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_11:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-CompoundStmt
// CHECK-NEXT: {{^}}| | `-ReturnStmt
// CHECK-NEXT: {{^}}| |   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}| |     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}| |     | |-ImplicitCastExpr {{.+}} 'void *__single __sized_by_or_null(size)':'void *__single' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}| |     | | `-OpaqueValueExpr [[ove_7:0x[^ ]+]] {{.*}} 'void *'
// CHECK:      {{^}}| |     | |-OpaqueValueExpr [[ove_7]]
// CHECK-NEXT: {{^}}| |     | | `-CStyleCastExpr {{.+}} 'void *' <NullToPointer>
// CHECK-NEXT: {{^}}| |     | |   `-IntegerLiteral {{.+}} 0
// CHECK-NEXT: {{^}}| |     | `-OpaqueValueExpr [[ove_8:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |     |   `-ImplicitCastExpr {{.+}} 'unsigned int' <LValueToRValue>
// CHECK-NEXT: {{^}}| |     |     `-DeclRefExpr {{.+}} [[var_size_11]]
// CHECK-NEXT: {{^}}| |     |-OpaqueValueExpr [[ove_7]] {{.*}} 'void *'
// CHECK:      {{^}}| |     `-OpaqueValueExpr [[ove_8]] {{.*}} 'unsigned int'
// CHECK:      {{^}}| `-AllocSizeAttr

void * override_nullability(unsigned, unsigned) __attribute__((returns_nonnull)) __attribute__((alloc_size(1, 2)));

// CHECK-HEADER: |-FunctionDecl [[func_override_nullability:0x[^ ]+]] {{.+}} override_nullability 'void *__single __sized_by_or_null(function-parameter-0-0 * function-parameter-0-1)(unsigned int, unsigned int)'
// CHECK-HEADER: | |-ParmVarDecl
// CHECK-HEADER: | |-ParmVarDecl
// CHECK-HEADER: | `-AllocSizeAttr

// CHECK-LABEL: override_nullability
// CHECK-SAME: 'void *__single __sized_by(function-parameter-0-0 * function-parameter-0-1)(unsigned int, unsigned int)'
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-ReturnsNonNullAttr
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * override_nullability2(unsigned, unsigned) __attribute__((alloc_size(1, 2)));

// CHECK-HEADER: |-FunctionDecl [[func_override_nullability2:0x[^ ]+]] {{.+}} override_nullability2 'void *__single __sized_by(function-parameter-0-0 * function-parameter-0-1)(unsigned int, unsigned int)'
// CHECK-HEADER: | |-ParmVarDecl
// CHECK-HEADER: | |-ParmVarDecl
// CHECK-HEADER: | |-ReturnsNonNullAttr
// CHECK-HEADER: | `-AllocSizeAttr

// CHECK-LABEL: override_nullability2
// CHECK-SAME: 'void *__single __sized_by_or_null(function-parameter-0-0 * function-parameter-0-1)(unsigned int, unsigned int)'
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-ReturnsNonNullAttr
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

__attribute__((alloc_size(1))) void * unnamed_param_non_sys(unsigned);
__attribute__((alloc_size(1))) void * unnamed_param_non_sys(unsigned size) {
    return (void*)0;
}

// CHECK: |-FunctionDecl [[func_unnamed_param_non_sys:0x[^ ]+]] {{.+}} unnamed_param_non_sys  'void *__single __sized_by_or_null(function-parameter-0-0)(unsigned int)'
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

// CHECK: `-FunctionDecl [[func_unnamed_param_non_sys_1:0x[^ ]+]] {{.+}} unnamed_param_non_sys  'void *__single __sized_by_or_null(size)(unsigned int)'
// CHECK-NEXT: {{^}}  |-ParmVarDecl [[var_size_12:0x[^ ]+]]
// CHECK-NEXT: {{^}}  |-CompoundStmt
// CHECK-NEXT: {{^}}  | `-ReturnStmt
// CHECK-NEXT: {{^}}  |   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}  |     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}  |     | |-ImplicitCastExpr {{.+}} 'void *__single __sized_by_or_null(size)':'void *__single' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}  |     | | `-OpaqueValueExpr [[ove_9:0x[^ ]+]] {{.*}} 'void *'
// CHECK:      {{^}}  |     | |-OpaqueValueExpr [[ove_9]]
// CHECK-NEXT: {{^}}  |     | | `-CStyleCastExpr {{.+}} 'void *' <NullToPointer>
// CHECK-NEXT: {{^}}  |     | |   `-IntegerLiteral {{.+}} 0
// CHECK-NEXT: {{^}}  |     | `-OpaqueValueExpr [[ove_10:0x[^ ]+]]
// CHECK-NEXT: {{^}}  |     |   `-ImplicitCastExpr {{.+}} 'unsigned int' <LValueToRValue>
// CHECK-NEXT: {{^}}  |     |     `-DeclRefExpr {{.+}} [[var_size_12]]
// CHECK-NEXT: {{^}}  |     |-OpaqueValueExpr [[ove_9]] {{.*}} 'void *'
// CHECK:      {{^}}  |     `-OpaqueValueExpr [[ove_10]] {{.*}} 'unsigned int'
// CHECK:      {{^}}  `-AllocSizeAttr

