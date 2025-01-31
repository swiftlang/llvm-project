// RUN: %clang_cc1 -fexperimental-bounds-safety-attributes -ast-dump %s 2>&1 | FileCheck %s --check-prefix=ATTR-ONLY
// RUN: %clang_cc1 -fbounds-safety -fbounds-attributes-cxx-experimental -ast-dump %s 2>&1 | FileCheck %s --check-prefix=BOUNDS-CHECK

#include <ptrcheck.h>

void * __sized_by_or_null(size) malloc(int size);

// ATTR-ONLY: |-FunctionDecl {{.*}} used malloc 'void * __sized_by_or_null(size)(int)'
// BOUNDS-CHECK: |-FunctionDecl {{.*}} used malloc 'void *__single __sized_by_or_null(size)(int)'

template <typename T>
T * __sized_by_or_null(sizeof(T) * n) mymalloc(int n) {
    return static_cast<T * __sized_by_or_null(4 * n)>(malloc(sizeof(T) * n));
}

// ATTR-ONLY: |-FunctionTemplateDecl {{.*}} mymalloc
// ATTR-ONLY: | |-TemplateTypeParmDecl {{.*}} referenced typename depth 0 index 0 T
// ATTR-ONLY: | |-FunctionDecl {{.*}} mymalloc 'T *(int)'
// ATTR-ONLY: | | | `-ReturnStmt
// FIXME: late parse attributes when applied to types rdar://143865865
// ATTR-ONLY: | | |   `-CXXStaticCastExpr {{.*}} 'T *' static_cast<T *> <Dependent>
// ATTR-ONLY: | | |     `-CallExpr {{.*}} 'void * __sized_by_or_null(size)':'void *'
// ATTR-ONLY: | | `-SizedByOrNullAttr
// ATTR-ONLY: | |   `-BinaryOperator {{.*}} '*'
// ATTR-ONLY: | |     |-UnaryExprOrTypeTraitExpr {{.*}} sizeof 'T' 
// ATTR-ONLY: | |       `-DeclRefExpr {{.*}} 'n' 'int'

// BOUNDS-CHECK: |-FunctionTemplateDecl {{.*}} mymalloc
// BOUNDS-CHECK: | |-TemplateTypeParmDecl {{.*}} referenced typename depth 0 index 0 T
// BOUNDS-CHECK: | |-FunctionDecl {{.*}} mymalloc 'T *__single(int)'
// BOUNDS-CHECK: | | | `-ReturnStmt
// BOUNDS-CHECK: | | |   `-CXXStaticCastExpr {{.*}} 'T *' static_cast<T *> <Dependent>
// BOUNDS-CHECK: | | |     `-CallExpr {{.*}} 'void *__single  __sized_by_or_null(size)':'void *__single'
// BOUNDS-CHECK: | | `-SizedByOrNullAttr
// BOUNDS-CHECK: | |   `-BinaryOperator {{.*}} '*'
// BOUNDS-CHECK: | |     |-UnaryExprOrTypeTraitExpr {{.*}} sizeof 'T' 
// BOUNDS-CHECK: | |       `-DeclRefExpr {{.*}} 'n' 'int'

// ATTR-ONLY: | `-FunctionDecl {{.*}} used mymalloc 'int * __sized_by_or_null(4UL * n)(int)' implicit_instantiation
// ATTR-ONLY: |   |-TemplateArgument type 'int'
// ATTR-ONLY: |       `-CXXStaticCastExpr {{.*}} 'int *' static_cast<int *> <BitCast>
// ATTR-ONLY: |         `-CallExpr {{.*}} 'void * __sized_by_or_null(size)':'void *'
// ATTR-ONLY: |             `-BinaryOperator {{.*}} '*'
// ATTR-ONLY: |               |-UnaryExprOrTypeTraitExpr {{.*}} sizeof 'int'
// ATTR-ONLY: |                   `-DeclRefExpr {{.*}} 'n' 'int'

// BOUNDS-CHECK: | `-FunctionDecl {{.*}} used mymalloc 'int *__single __sized_by_or_null(4UL * n)(int)' implicit_instantiation
// BOUNDS-CHECK: |   |-TemplateArgument type 'int'
// BOUNDS-CHECK: |       `-CXXStaticCastExpr {{.*}} 'int *' static_cast<int *> <BitCast>
// BOUNDS-CHECK: |           `-MaterializeSequenceExpr {{.*}} <Unbind>
// BOUNDS-CHECK: |             |-MaterializeSequenceExpr {{.*}} <Bind>
// BOUNDS-CHECK: |             | |-BoundsSafetyPointerPromotionExpr {{.*}} 'void *__bidi_indexable'
// BOUNDS-CHECK: |             | | |-OpaqueValueExpr {{.*}} 'void *__single __sized_by_or_null(size)':'void *__single'
// BOUNDS-CHECK: |             | | | `-CallExpr {{.*}} 'void *__single __sized_by_or_null(size)':'void *__single'
// BOUNDS-CHECK: |             | | |   `-OpaqueValueExpr {{.*}}
// BOUNDS-CHECK: |             | | |       `-BinaryOperator {{.*}} '*'
// BOUNDS-CHECK: |             | | |         |-UnaryExprOrTypeTraitExpr {{.*}} sizeof 'int'
// BOUNDS-CHECK: |             | | |             `-DeclRefExpr {{.*}} 'n' 'int'

template <typename T>
T bar(int x) {
    int y;
    int * __sized_by_or_null(sizeof(T) * y) p;
    p = mymalloc<T>(x);
    y = x;
    return *p;
}

// ATTR-ONLY: |-FunctionTemplateDecl {{.*}} bar
// ATTR-ONLY: | |-TemplateTypeParmDecl {{.*}} referenced typename depth 0 index 0 T
// ATTR-ONLY: | |-FunctionDecl {{.*}} bar 'T (int)'
// ATTR-ONLY: | | `-CompoundStmt
// ATTR-ONLY: | |   | `-VarDecl {{.*}} y 'int'
// ATTR-ONLY: | |   | `-VarDecl {{.*}} p 'int *'
// ATTR-ONLY: | |   |   `-SizedByOrNullAttr
// ATTR-ONLY: | |   |     `-BinaryOperator {{.*}} '*'
// ATTR-ONLY: | |   |       |-UnaryExprOrTypeTraitExpr {{.*}} sizeof 'T'
// ATTR-ONLY: | |   |       `-DeclRefExpr {{.*}} 'y'
// ATTR-ONLY: | |   |-BinaryOperator {{.*}} '<dependent type>' '='
// ATTR-ONLY: | |   | |-DeclRefExpr {{.*}} 'p'
// ATTR-ONLY: | |   | `-CallExpr {{.*}} '<dependent type>'
// ATTR-ONLY: | |   |   |-UnresolvedLookupExpr {{.*}} '<dependent type>' lvalue (ADL) = 'mymalloc'
// ATTR-ONLY: | |   |   | `-TemplateArgument type 'T':'type-parameter-0-0'
// ATTR-ONLY: | |   |   |   `-TemplateTypeParmType {{.*}} 'T' dependent depth 0 index 0
// ATTR-ONLY: | |   |   |     `-TemplateTypeParm {{.*}} 'T'
// ATTR-ONLY: | |   |   `-DeclRefExpr {{.*}} 'x'
// ATTR-ONLY: | |   |-BinaryOperator {{.*}} '='
// ATTR-ONLY: | |   | |-DeclRefExpr {{.*}} 'y'
// ATTR-ONLY: | |   | `-DeclRefExpr {{.*}} 'x'
// ATTR-ONLY: | |   `-ReturnStmt
// ATTR-ONLY: | |     `-UnaryOperator {{.*}} '*'
// ATTR-ONLY: | |       `-DeclRefExpr {{.*}} 'p' 'int *'

// BOUNDS-CHECK: |-FunctionTemplateDecl {{.*}} bar
// BOUNDS-CHECK: | |-TemplateTypeParmDecl {{.*}} referenced typename depth 0 index 0 T
// BOUNDS-CHECK: | |-FunctionDecl {{.*}} bar 'T (int)'
// BOUNDS-CHECK: | | `-CompoundStmt
// BOUNDS-CHECK: | |   | `-VarDecl {{.*}} y 'int'
// BOUNDS-CHECK: | |   | `-VarDecl {{.*}} p 'int *__bidi_indexable'
// BOUNDS-CHECK: | |   |   `-SizedByOrNullAttr
// BOUNDS-CHECK: | |   |     `-BinaryOperator {{.*}} '*'
// BOUNDS-CHECK: | |   |       |-UnaryExprOrTypeTraitExpr {{.*}} sizeof 'T'
// BOUNDS-CHECK: | |   |           `-DeclRefExpr {{.*}} 'y'
// BOUNDS-CHECK: | |   |-BinaryOperator {{.*}} '<dependent type>' '='
// BOUNDS-CHECK: | |   | |-DeclRefExpr {{.*}} 'p' 'int *__bidi_indexable'
// BOUNDS-CHECK: | |   | `-CallExpr {{.*}} '<dependent type>'
// BOUNDS-CHECK: | |   |   |-UnresolvedLookupExpr {{.*}} '<dependent type>' lvalue (ADL) = 'mymalloc'
// BOUNDS-CHECK: | |   |   | `-TemplateArgument type 'T':'type-parameter-0-0'
// BOUNDS-CHECK: | |   |   |   `-TemplateTypeParmType {{.*}} 'T'
// BOUNDS-CHECK: | |   |   |     `-TemplateTypeParm {{.*}} 'T'
// BOUNDS-CHECK: | |   |   `-DeclRefExpr {{.*}} 'x' 'int'
// BOUNDS-CHECK: | |   |-BinaryOperator {{.*}} 'int' lvalue '='
// BOUNDS-CHECK: | |   | |-DeclRefExpr {{.*}} 'y'
// BOUNDS-CHECK: | |   |   `-DeclRefExpr {{.*}} 'x'
// BOUNDS-CHECK: | |   `-ReturnStmt
// BOUNDS-CHECK: | |     `-UnaryOperator {{.*}} '*'
// BOUNDS-CHECK: | |         `-DeclRefExpr {{.*}} 'p'


// ATTR-ONLY: | `-FunctionDecl {{.*}} bar 'int (int)' implicit_instantiation
// ATTR-ONLY: |   |-TemplateArgument type 'int'
// ATTR-ONLY: |   `-CompoundStmt
// ATTR-ONLY: |     | `-VarDecl {{.*}} y
// ATTR-ONLY: |     |   `-DependerDeclsAttr
// ATTR-ONLY: |     | `-VarDecl {{.*}} p 'int * __sized_by_or_null(4UL * y)':'int *'
// ATTR-ONLY: |     |-BinaryOperator {{.*}} '='
// ATTR-ONLY: |     | |-DeclRefExpr {{.*}} 'p' 'int * __sized_by_or_null(4UL * y)':'int *'
// ATTR-ONLY: |     | `-CallExpr
// ATTR-ONLY: |     |   `-DeclRefExpr {{.*}} 'x' 'int'
// ATTR-ONLY: |     |-BinaryOperator {{.*}} '='
// ATTR-ONLY: |     | |-DeclRefExpr {{.*}} 'y' 'int'
// ATTR-ONLY: |     | `-DeclRefExpr {{.*}} 'x' 'int'
// ATTR-ONLY: |     `-ReturnStmt
// ATTR-ONLY: |         `-UnaryOperator {{.*}} '*'
// ATTR-ONLY: |             `-DeclRefExpr {{.*}} 'p' 'int * __sized_by_or_null(4UL * y)':'int *'

// BOUNDS-CHECK: | `-FunctionDecl {{.*}} bar 'int (int)' implicit_instantiation
// BOUNDS-CHECK: |   |-TemplateArgument type 'int'
// BOUNDS-CHECK: |   `-CompoundStmt
// BOUNDS-CHECK: |     | `-VarDecl {{.*}} y
// BOUNDS-CHECK: |     |   `-DependerDeclsAttr
// BOUNDS-CHECK: |     | `-VarDecl {{.*}} p 'int *__single __sized_by_or_null(4UL * y)':'int *__single'
// BOUNDS-CHECK: |     |-MaterializeSequenceExpr {{.*}} <Bind>
// BOUNDS-CHECK: |     | |-BoundsCheckExpr {{.*}} 'mymalloc<int>(x) <= __builtin_get_pointer_upper_bound(mymalloc<int>(x)) && __builtin_get_pointer_lower_bound(mymalloc<int>(x)) <= mymalloc<int>(x) && !mymalloc<int>(x) || 4UL * x <= (char *)__builtin_get_pointer_upper_bound(mymalloc<int>(x)) - (char *__single)mymalloc<int>(x)'
// BOUNDS-CHECK: |     | | |-BinaryOperator {{.*}} 'int *__single __sized_by_or_null(4UL * y)':'int *__single' lvalue '='
// BOUNDS-CHECK: |     | | | |-DeclRefExpr {{.*}} 'p'
// BOUNDS-CHECK: |     | | | `-OpaqueValueExpr
// BOUNDS-CHECK: |     | | |     `-MaterializeSequenceExpr {{.*}} <Unbind>
// BOUNDS-CHECK: |     | | |       |-MaterializeSequenceExpr {{.*}} <Bind>
// BOUNDS-CHECK: |     | | |       | |-BoundsSafetyPointerPromotionExpr {{.*}} 'int *__bidi_indexable'
// BOUNDS-CHECK: |     | | |       | | |-OpaqueValueExpr {{.*}}
// BOUNDS-CHECK: |     | | |       | | | `-CallExpr {{.*}} 'int *__single __sized_by_or_null(4UL * n)':'int *__single'
// BOUNDS-CHECK: |     | | |       | | |   `-OpaqueValueExpr
// BOUNDS-CHECK: |     | | |       | | |       `-DeclRefExpr {{.*}} 'x' 'int'
// BOUNDS-CHECK: |     |-MaterializeSequenceExpr {{.*}} <Unbind>
// BOUNDS-CHECK: |     | |-BinaryOperator {{.*}} 'int' lvalue '='
// BOUNDS-CHECK: |     | | |-DeclRefExpr {{.*}} 'y'
// BOUNDS-CHECK: |     | | `-OpaqueValueExpr
// BOUNDS-CHECK: |     | |     `-DeclRefExpr {{.*}} 'x'
// BOUNDS-CHECK: |     | |-OpaqueValueExpr
// BOUNDS-CHECK: |     | |   `-MaterializeSequenceExpr {{.*}} <Unbind>
// BOUNDS-CHECK: |     | |     |-MaterializeSequenceExpr {{.*}} <Bind>
// BOUNDS-CHECK: |     | |     | |-BoundsSafetyPointerPromotionExpr {{.*}} 'int *__bidi_indexable'
// BOUNDS-CHECK: |     | |     | | |-OpaqueValueExpr {{.*}}
// BOUNDS-CHECK: |     | |     | | | `-CallExpr {{.*}} 'int *__single __sized_by_or_null(4UL * n)':'int *__single'
// BOUNDS-CHECK: |     | |     | | |   `-OpaqueValueExpr
// BOUNDS-CHECK: |     | |     | | |       `-DeclRefExpr {{.*}} 'x' 'int'
// BOUNDS-CHECK: |     `-ReturnStmt
// BOUNDS-CHECK: |         `-UnaryOperator {{.*}} '*'
// BOUNDS-CHECK: |           `-MaterializeSequenceExpr {{.*}} <Unbind>
// BOUNDS-CHECK: |             |-MaterializeSequenceExpr {{.*}} <Bind>
// BOUNDS-CHECK: |             | |-BoundsSafetyPointerPromotionExpr {{.*}} 'int *__bidi_indexable'
// BOUNDS-CHECK: |             | | |-OpaqueValueExpr
// BOUNDS-CHECK: |             | | |   `-DeclRefExpr {{.*}} 'p' 'int *__single __sized_by_or_null(4UL * y)':'int *__single'
// BOUNDS-CHECK: |             | | | `-BinaryOperator {{.*}} 'char *' '+'
// BOUNDS-CHECK: |             | | |   |-CStyleCastExpr {{.*}} 'char *' <BitCast>
// BOUNDS-CHECK: |             | | |   | `-ImplicitCastExpr {{.*}} 'int *' <BoundsSafetyPointerCast> part_of_explicit_cast
// BOUNDS-CHECK: |             | | |   |   `-OpaqueValueExpr
// BOUNDS-CHECK: |             | | |   |       `-DeclRefExpr {{.*}} 'p' 'int *__single __sized_by_or_null(4UL * y)':'int *__single'
// BOUNDS-CHECK: |             | | |   `-AssumptionExpr
// BOUNDS-CHECK: |             | | |     `-BinaryOperator {{.*}} 'bool' '>='
// BOUNDS-CHECK: |             | | |       | `-BinaryOperator {{.*}} 'unsigned long' '*'
// BOUNDS-CHECK: |             | | |       |   |-IntegerLiteral {{.*}} 4
// BOUNDS-CHECK: |             | | |       |   `-DeclRefExpr {{.*}} 'y'
// BOUNDS-CHECK: |             | | |       `-IntegerLiteral {{.*}} 0

void foo(int m) {
    int * p1 = mymalloc<int>(m);
    int * p2 = mymalloc<int>(10);
    int i = bar<int>(m);
}

// ATTR-ONLY: `-FunctionDecl {{.*}} foo
// ATTR-ONLY:       `-VarDecl {{.*}} p1 'int *' cinit
// ATTR-ONLY:         `-CallExpr {{.*}} 'int * __sized_by_or_null(4UL * n)':'int *'
// ATTR-ONLY:             `-DeclRefExpr {{.*}} 'm' 'int'
// ATTR-ONLY:       `-VarDecl {{.*}} p2 'int *' cinit
// ATTR-ONLY:         `-CallExpr {{.*}} 'int * __sized_by_or_null(4UL * n)':'int *'
// ATTR-ONLY:           `-IntegerLiteral {{.*}} 'int' 10

// BOUNDS-CHECK: `-FunctionDecl {{.*}} foo
// BOUNDS-CHECK:      `-VarDecl {{.*}} p1 'int *__bidi_indexable' cinit
// BOUNDS-CHECK:        `-MaterializeSequenceExpr {{.*}} 'int *__bidi_indexable' <Unbind>
// BOUNDS-CHECK:          |-MaterializeSequenceExpr {{.*}} 'int *__bidi_indexable' <Bind>
// BOUNDS-CHECK:          | |-BoundsSafetyPointerPromotionExpr {{.*}} 'int *__bidi_indexable'
// BOUNDS-CHECK:          | | |-OpaqueValueExpr {{.*}} 'int *__single __sized_by_or_null(4UL * n)':'int *__single'
// BOUNDS-CHECK:          | | | `-CallExpr {{.*}} 'int *__single __sized_by_or_null(4UL * n)':'int *__single'
// BOUNDS-CHECK:          | | |   `-OpaqueValueExpr {{.*}} 'int'
// BOUNDS-CHECK:          | | |     `-DeclRefExpr {{.*}} 'm' 'int'
// BOUNDS-CHECK:       `-VarDecl {{.*}} p2 'int *__bidi_indexable' cinit
// BOUNDS-CHECK:         `-MaterializeSequenceExpr {{.*}} 'int *__bidi_indexable' <Unbind>
// BOUNDS-CHECK:           |-MaterializeSequenceExpr {{.*}} 'int *__bidi_indexable' <Bind>
// BOUNDS-CHECK:           | |-BoundsSafetyPointerPromotionExpr {{.*}} 'int *__bidi_indexable'
// BOUNDS-CHECK:           | | |-OpaqueValueExpr {{.*}} 'int *__single __sized_by_or_null(4UL * n)':'int *__single'
// BOUNDS-CHECK:           | | | `-CallExpr {{.*}} 'int *__single __sized_by_or_null(4UL * n)':'int *__single'
// BOUNDS-CHECK:           | | |   `-OpaqueValueExpr {{.*}} 'int'
// BOUNDS-CHECK:           | | |     `-IntegerLiteral {{.*}} 'int' 10

