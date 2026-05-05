#pragma clang system_header

void * __sized_by_or_null(size) explicitly_sized(int size) __attribute__((alloc_size(1)));

// CHECK:      {{^}}|-FunctionDecl [[func_explicitly_sized:0x[^ ]+]] {{.+}} explicitly_sized
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size:0x[^ ]+]]
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * __sized_by(size) explicitly_sized_nonnull(int size) __attribute__((alloc_size(1)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_explicitly_sized_nonnull:0x[^ ]+]] {{.+}} explicitly_sized_nonnull
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_1:0x[^ ]+]]
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * _Nonnull implicitly_sized_nonnull_non_semantic(int size) __attribute__((alloc_size(1)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_implicitly_sized_nonnull_non_semantic:0x[^ ]+]] {{.+}} implicitly_sized_nonnull_non_semantic
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_2:0x[^ ]+]]
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * implicitly_sized_nonnull_semantic(int size) __attribute__((returns_nonnull)) __attribute__((alloc_size(1)));
//
// CHECK-NEXT: {{^}}|-FunctionDecl [[func_implicitly_sized_nonnull_semantic:0x[^ ]+]] {{.+}} implicitly_sized_nonnull_semantic
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_3:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-ReturnsNonNullAttr
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * implicitly_sized_with_count(int size, int count) __attribute__((alloc_size(1, 2)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_implicitly_sized_with_count:0x[^ ]+]] {{.+}} implicitly_sized_with_count
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_4:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_count:0x[^ ]+]]
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * __sized_by(size * count) explicitly_sized_nonnull_with_count(int size, int count) __attribute__((alloc_size(1, 2)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_explicitly_sized_nonnull_with_count:0x[^ ]+]] {{.+}} explicitly_sized_nonnull_with_count
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_5:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_count_1:0x[^ ]+]]
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * _Nonnull implicitly_sized_nonnull_non_semantic_with_count(int size, int count) __attribute__((alloc_size(1, 2)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_implicitly_sized_nonnull_non_semantic_with_count:0x[^ ]+]] {{.+}} implicitly_sized_nonnull_non_semantic_with_count
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_6:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_count_2:0x[^ ]+]]
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * implicitly_sized_nonnull_semantic_with_count(int size, int count) __attribute__((returns_nonnull)) __attribute__((alloc_size(1, 2)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_implicitly_sized_nonnull_semantic_with_count:0x[^ ]+]] {{.+}} implicitly_sized_nonnull_semantic_with_count
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_size_7:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-ParmVarDecl [[var_count_3:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |-ReturnsNonNullAttr
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

void * unnamed_param_with_count(unsigned, unsigned) __attribute__((alloc_size(1, 2)));

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_unnamed_param_with_count:0x[^ ]+]] {{.+}} unnamed_param_with_count
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| `-AllocSizeAttr

__attribute__((alloc_size(1, 2))) void * unnamed_param_with_count(unsigned, unsigned) {
    return (void*)0;
}

// CHECK-NEXT: {{^}}|-FunctionDecl [[func_unnamed_param_with_count_1:0x[^ ]+]] {{.+}} unnamed_param_with_count
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-ParmVarDecl
// CHECK-NEXT: {{^}}| |-CompoundStmt
// CHECK-NEXT: {{^}}| | `-ReturnStmt
// CHECK-NEXT: {{^}}| |   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}| |     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}| |     | |-ImplicitCastExpr {{.+}} 'void *__single __sized_by_or_null(function-parameter-0-0 * function-parameter-0-1)':'void *__single' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}| |     | | `-OpaqueValueExpr [[ove:0x[^ ]+]] {{.*}} 'void *'
// CHECK:      {{^}}| |     | |-OpaqueValueExpr [[ove]]
// CHECK-NEXT: {{^}}| |     | | `-CStyleCastExpr {{.+}} 'void *' <NullToPointer>
// CHECK-NEXT: {{^}}| |     | |   `-IntegerLiteral {{.+}} 0
// CHECK-NEXT: {{^}}| |     | |-OpaqueValueExpr [[ove_1:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |     | | `-ImplicitCastExpr {{.+}} 'unsigned int' <LValueToRValue>
// CHECK-NEXT: {{^}}| |     | |   `-DeclRefExpr {{.+}}
// CHECK-NEXT: {{^}}| |     | `-OpaqueValueExpr [[ove_2:0x[^ ]+]]
// CHECK-NEXT: {{^}}| |     |   `-ImplicitCastExpr {{.+}} 'unsigned int' <LValueToRValue>
// CHECK-NEXT: {{^}}| |     |     `-DeclRefExpr {{.+}}
// CHECK-NEXT: {{^}}| |     |-OpaqueValueExpr [[ove]] {{.*}} 'void *'
// CHECK:      {{^}}| |     |-OpaqueValueExpr [[ove_1]] {{.*}} 'unsigned int'
// CHECK:      {{^}}| |     `-OpaqueValueExpr [[ove_2]] {{.*}} 'unsigned int'
// CHECK:      {{^}}| `-AllocSizeAttr

/* --- checked in C file --- */

void * unnamed_param(unsigned) __attribute__((alloc_size(1)));

void * inherit_attr(unsigned size) __attribute__((alloc_size(1)));

void * unnamed_param_inherit_attr(unsigned) __attribute__((alloc_size(1)));

void * unnamed_param_with_count_flipped(unsigned, unsigned)
    __attribute__((alloc_size(1,2)));

void * unnamed_param_with_count_mismatching_attrs_after_param_list(unsigned, unsigned)
    __attribute__((alloc_size(1,2)));

void * override_nullability(unsigned, unsigned)
    __attribute__((alloc_size(1, 2)));

void * override_nullability2(unsigned, unsigned)
    __attribute__((returns_nonnull))
    __attribute__((alloc_size(1, 2)));

