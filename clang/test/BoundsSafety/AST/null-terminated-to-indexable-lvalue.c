// RUN: %clang_cc1 -fbounds-safety -ast-dump %s | FileCheck %s
// RUN: %clang_cc1 -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -ast-dump %s | FileCheck %s

#include <ptrcheck.h>

char *__counted_by(len) returns_counted_by(const char *__counted_by(len) p, unsigned len);

// CHECK:      FunctionDecl [[func_pass:0x[^ ]+]] {{.+}} pass_as_argument 'void (const char *__single __terminated_by(0))'
// CHECK-NEXT: |-ParmVarDecl [[var_src:0x[^ ]+]] {{.+}} src 'const char *__single __terminated_by(0)':'const char *__single'
// CHECK-NEXT: `-CompoundStmt {{.+}}
// CHECK-NEXT:   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT:     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT:     | |-BoundsSafetyPointerPromotionExpr {{.+}}
// CHECK-NEXT:     | | |-OpaqueValueExpr [[ove_call:0x[^ ]+]] {{.+}} 'char *__single __counted_by(len)':'char *__single'
// CHECK-NEXT:     | | | `-BoundsCheckExpr {{.+}}
// CHECK-NEXT:     | | |   |-CallExpr {{.+}}
// CHECK-NEXT:     | | |   | |-ImplicitCastExpr {{.+}} <FunctionToPointerDecay>
// CHECK-NEXT:     | | |   | | `-DeclRefExpr {{.+}} Function {{.+}} 'returns_counted_by'
// CHECK-NEXT:     | | |   | |-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:     | | |   | | `-OpaqueValueExpr [[ove_indexable:0x[^ ]+]] {{.+}} 'const char *__indexable'
// CHECK-NEXT:     | | |   | |   `-TerminatedByToIndexableExpr [[term_expr:0x[^ ]+]] {{.+}} 'const char *__indexable'
// CHECK-NEXT:     | | |   | |     |-ImplicitCastExpr {{.+}} 'const char *__single __terminated_by(0)':'const char *__single' <LValueToRValue>
// CHECK-NEXT:     | | |   | |     | `-ParenExpr {{.+}} 'const char *__single __terminated_by(0)':'const char *__single' lvalue
// CHECK-NEXT:     | | |   | |     |   `-DeclRefExpr {{.+}} 'const char *__single __terminated_by(0)':'const char *__single' lvalue ParmVar [[var_src]] 'src'
// CHECK-NEXT:     | | |   | |     `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:     | | |   | `-OpaqueValueExpr [[ove_len:0x[^ ]+]] {{.+}} 'unsigned int'
// CHECK-NEXT:     | | |   |   `-ImplicitCastExpr {{.+}} <IntegralCast>
// CHECK-NEXT:     | | |   |     `-IntegerLiteral {{.+}} 'int' 10
// CHECK-NEXT:     | | |   `-BinaryOperator {{.+}} '&&'
// CHECK-NEXT:     | | |     |-BinaryOperator {{.+}} '&&'
// CHECK-NEXT:     | | |     | |-BinaryOperator {{.+}} '<='
// CHECK-NEXT:     | | |     | | |-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:     | | |     | | | `-OpaqueValueExpr [[ove_indexable]] {{.*}} 'const char *__indexable'
// CHECK-NEXT:     | | |     | | |   `-TerminatedByToIndexableExpr [[term_expr]] {{.*}} 'const char *__indexable'
void pass_as_argument(const char *src) {
   returns_counted_by(__unsafe_null_terminated_to_indexable(src), 10);
}

// CHECK:      FunctionDecl [[func_assign:0x[^ ]+]] {{.+}} assign 'void (char *__single __counted_by(*len)*__single, unsigned int *__single, char *__single __terminated_by(0))'
// CHECK-NEXT: |-ParmVarDecl [[var_p:0x[^ ]+]] {{.+}} p 'char *__single __counted_by(*len)*__single'
// CHECK-NEXT: |-ParmVarDecl [[var_len:0x[^ ]+]] {{.+}} len 'unsigned int *__single'
// CHECK-NEXT: | `-DependerDeclsAttr {{.+}} Implicit IsDeref [[var_p]]
// CHECK-NEXT: |-ParmVarDecl [[var_src2:0x[^ ]+]] {{.+}} src 'char *__single __terminated_by(0)':'char *__single'
// CHECK-NEXT: `-CompoundStmt {{.+}}
// CHECK-NEXT:   |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT:   | |-BoundsCheckExpr {{.+}}
// CHECK-NEXT:   | | |-BinaryOperator {{.+}} '='
// CHECK-NEXT:   | | | |-UnaryOperator {{.+}} lvalue prefix '*' cannot overflow
// CHECK-NEXT:   | | | | `-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | | | |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_p]] 'p'
// CHECK-NEXT:   | | | `-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | | |   `-OpaqueValueExpr [[ove_assign:0x[^ ]+]] {{.+}} 'char *__indexable'
// CHECK-NEXT:   | | |     `-TerminatedByToIndexableExpr [[term_assign:0x[^ ]+]] {{.+}} 'char *__indexable'
// CHECK-NEXT:   | | |       |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | | |       | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | | |       |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | | |       `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | | `-BinaryOperator {{.+}} '&&'
// CHECK-NEXT:   | |   |-BinaryOperator {{.+}} '&&'
// CHECK-NEXT:   | |   | |-BinaryOperator {{.+}} '<='
// CHECK-NEXT:   | |   | | |-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | |   | | | `-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   | | |   `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   | | |     |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |   | | |     | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |   | | |     |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |   | | |     `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | |   | | `-GetBoundExpr {{.+}} upper
// CHECK-NEXT:   | |   | |   `-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | |   | |     `-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   | |       `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   | |         |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |   | |         | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |   | |         |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |   | |         `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | |   | `-BinaryOperator {{.+}} '<='
// CHECK-NEXT:   | |   |   |-GetBoundExpr {{.+}} lower
// CHECK-NEXT:   | |   |   | `-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | |   |   |   `-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   |   |     `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   |   |       |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |   |   |       | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |   |   |       |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |   |   |       `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | |   |   `-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | |   |     `-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   |       `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   |         |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |   |         | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |   |         |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |   |         `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | |   `-BinaryOperator {{.+}} '<='
// CHECK-NEXT:   | |     |-ImplicitCastExpr {{.+}} <IntegralCast>
// CHECK-NEXT:   | |     | `-OpaqueValueExpr [[ove_len2:0x[^ ]+]] {{.+}} 'unsigned int'
// CHECK-NEXT:   | |     |   `-ImplicitCastExpr {{.+}} <IntegralCast>
// CHECK-NEXT:   | |     |     `-IntegerLiteral {{.+}} 'int' 10
// CHECK-NEXT:   | |     `-BinaryOperator {{.+}} '-'
// CHECK-NEXT:   | |       |-GetBoundExpr {{.+}} upper
// CHECK-NEXT:   | |       | `-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | |       |   `-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |       |     `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |       |       |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |       |       | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |       |       |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |       |       `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | |       `-ImplicitCastExpr {{.+}} <BoundsSafetyPointerCast>
// CHECK-NEXT:   | |         `-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |           `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |             |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |             | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |             |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |             `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | |-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | | `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:   | |   |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:   | |   | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:   | |   |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:   | |   `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:   | `-OpaqueValueExpr [[ove_len2]] {{.*}} 'unsigned int'
// CHECK-NEXT:   |   `-ImplicitCastExpr {{.+}} <IntegralCast>
// CHECK-NEXT:   |     `-IntegerLiteral {{.+}} 'int' 10
// CHECK-NEXT:   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT:     |-BinaryOperator {{.+}} '='
// CHECK-NEXT:     | |-UnaryOperator {{.+}} lvalue prefix '*' cannot overflow
// CHECK-NEXT:     | | `-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:     | |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_len]] 'len'
// CHECK-NEXT:     | `-OpaqueValueExpr [[ove_len2]] {{.*}} 'unsigned int'
// CHECK-NEXT:     |   `-ImplicitCastExpr {{.+}} <IntegralCast>
// CHECK-NEXT:     |     `-IntegerLiteral {{.+}} 'int' 10
// CHECK-NEXT:     |-OpaqueValueExpr [[ove_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:     | `-TerminatedByToIndexableExpr [[term_assign]] {{.*}} 'char *__indexable'
// CHECK-NEXT:     |   |-ImplicitCastExpr {{.+}} <LValueToRValue>
// CHECK-NEXT:     |   | `-ParenExpr {{.+}} lvalue
// CHECK-NEXT:     |   |   `-DeclRefExpr {{.+}} lvalue ParmVar [[var_src2]] 'src'
// CHECK-NEXT:     |   `-IntegerLiteral {{.+}} 'int' 0
// CHECK-NEXT:     `-OpaqueValueExpr [[ove_len2]] {{.*}} 'unsigned int'
// CHECK-NEXT:       `-ImplicitCastExpr {{.+}} <IntegralCast>
// CHECK-NEXT:         `-IntegerLiteral {{.+}} 'int' 10
void assign(char *__counted_by(*len) *p, unsigned *len, char *__null_terminated src) {
   *p = __unsafe_null_terminated_to_indexable(src);
   *len = 10;
}