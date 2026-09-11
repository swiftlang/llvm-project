// Checks generated with clang/utils/simplify_ast_dump_for_checks.py.

// RUN: %clang_cc1 -ast-dump -fbounds-safety -fblocks %s 2>&1 | FileCheck %s
// RUN: %clang_cc1 -ast-dump -fbounds-safety -fblocks -x objective-c -fexperimental-bounds-safety-objc %s 2>&1 | FileCheck %s

#include <ptrcheck.h>

typedef unsigned long size_t;

struct accumulator {
  size_t total_size;
  void *__sized_by(total_size) payload;
};

struct accumulator *new_accumulator(void);

typedef void *(^create_b)(void);
int find_or_create(create_b create);

int test_pair_in_block(size_t n, void *__sized_by(n) buf) {
  return find_or_create(^void *(void) {
    struct accumulator *acc = new_accumulator();
    acc->total_size = n;
    acc->payload = buf;
    return acc;
  });
}

// CHECK:      {{^}}TranslationUnitDecl
// CHECK:      {{^}}|-TypedefDecl
// CHECK:      {{^}}| `-BuiltinType
// CHECK:      {{^}}|-RecordDecl
// CHECK:      {{^}}| |-FieldDecl
// CHECK:      {{^}}| | `-DependerDeclsAttr
// CHECK:      {{^}}| `-FieldDecl
// CHECK:      {{^}}|-FunctionDecl [[func_new_accumulator:0x[^ ]+]] {{.+}} new_accumulator
// CHECK:      {{^}}|-TypedefDecl
// CHECK:      {{^}}| `-BlockPointerType
// CHECK:      {{^}}|   `-ParenType
// CHECK:      {{^}}|     `-FunctionProtoType
// CHECK:      {{^}}|       `-PointerType
// CHECK:      {{^}}|         `-BuiltinType
// CHECK:      {{^}}|-FunctionDecl [[func_find_or_create:0x[^ ]+]] {{.+}} find_or_create
// CHECK:      {{^}}| `-ParmVarDecl [[var_create:0x[^ ]+]]
// CHECK:      {{^}}`-FunctionDecl [[func_test_pair_in_block:0x[^ ]+]] {{.+}} test_pair_in_block
// CHECK-NEXT: {{^}}  |-ParmVarDecl [[var_n:0x[^ ]+]]
// CHECK-NEXT: {{^}}  | `-DependerDeclsAttr
// CHECK-NEXT: {{^}}  |-ParmVarDecl [[var_buf:0x[^ ]+]]
// CHECK-NEXT: {{^}}  `-CompoundStmt
// CHECK-NEXT: {{^}}    `-ReturnStmt
// CHECK-NEXT: {{^}}      `-ExprWithCleanups
// CHECK-NEXT: {{^}}        |-cleanup Block
// CHECK-NEXT: {{^}}        `-CallExpr
// CHECK-NEXT: {{^}}          |-ImplicitCastExpr {{.+}} 'int (*__single)(create_b)' <FunctionToPointerDecay>
// CHECK-NEXT: {{^}}          | `-DeclRefExpr {{.+}} [[func_find_or_create]]
// CHECK-NEXT: {{^}}          `-BlockExpr
// CHECK-NEXT: {{^}}            `-BlockDecl
// CHECK-NEXT: {{^}}              |-capture ParmVar
// CHECK-NEXT: {{^}}              |-capture ParmVar
// CHECK-NEXT: {{^}}              `-CompoundStmt
// CHECK-NEXT: {{^}}                |-DeclStmt
// CHECK-NEXT: {{^}}                | `-VarDecl [[var_acc:0x[^ ]+]]
// CHECK-NEXT: {{^}}                |   `-ImplicitCastExpr {{.+}} 'struct accumulator *__bidi_indexable' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                |     `-CallExpr
// CHECK-NEXT: {{^}}                |       `-ImplicitCastExpr {{.+}} 'struct accumulator *__single(*__single)(void)' <FunctionToPointerDecay>
// CHECK-NEXT: {{^}}                |         `-DeclRefExpr {{.+}} [[func_new_accumulator]]
// CHECK-NEXT: {{^}}                |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}                | |-BoundsCheckExpr {{.+}} 'buf <= __builtin_get_pointer_upper_bound(buf) && __builtin_get_pointer_lower_bound(buf) <= buf && n <= (char *)__builtin_get_pointer_upper_bound(buf) - (char *__bidi_indexable)buf'
// CHECK-NEXT: {{^}}                | | |-BinaryOperator {{.+}} 'size_t':'unsigned long' '='
// CHECK-NEXT: {{^}}                | | | |-MemberExpr {{.+}} ->total_size
// CHECK-NEXT: {{^}}                | | | | `-ImplicitCastExpr {{.+}} 'struct accumulator *__bidi_indexable' <LValueToRValue>
// CHECK-NEXT: {{^}}                | | | |   `-DeclRefExpr {{.+}} [[var_acc]]
// CHECK-NEXT: {{^}}                | | | `-OpaqueValueExpr [[ove:0x[^ ]+]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                | | `-BinaryOperator {{.+}} 'int' '&&'
// CHECK-NEXT: {{^}}                | |   |-BinaryOperator {{.+}} 'int' '&&'
// CHECK-NEXT: {{^}}                | |   | |-BinaryOperator {{.+}} 'int' '<='
// CHECK-NEXT: {{^}}                | |   | | |-ImplicitCastExpr {{.+}} 'void *' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                | |   | | | `-OpaqueValueExpr [[ove_1:0x[^ ]+]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |   | | |     | | |-OpaqueValueExpr [[ove_2:0x[^ ]+]] {{.*}} 'void *__single __sized_by(n)':'void *__single'
// CHECK:      {{^}}                | |   | | |     | | |     |-OpaqueValueExpr [[ove_3:0x[^ ]+]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                | |   | | `-GetBoundExpr {{.+}} upper
// CHECK-NEXT: {{^}}                | |   | |   `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |   | `-BinaryOperator {{.+}} 'int' '<='
// CHECK-NEXT: {{^}}                | |   |   |-GetBoundExpr {{.+}} lower
// CHECK-NEXT: {{^}}                | |   |   | `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |   |   `-ImplicitCastExpr {{.+}} 'void *' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                | |   |     `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |   `-BinaryOperator {{.+}} 'int' '<='
// CHECK-NEXT: {{^}}                | |     |-OpaqueValueExpr [[ove]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                | |     `-ImplicitCastExpr {{.+}} 'size_t':'unsigned long' <IntegralCast>
// CHECK-NEXT: {{^}}                | |       `-BinaryOperator {{.+}} '__ptrdiff_t':'long' '-'
// CHECK-NEXT: {{^}}                | |         |-CStyleCastExpr {{.+}} 'char *' <BitCast>
// CHECK-NEXT: {{^}}                | |         | `-GetBoundExpr {{.+}} upper
// CHECK-NEXT: {{^}}                | |         |   `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |         `-ImplicitCastExpr {{.+}} 'char *' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                | |           `-CStyleCastExpr {{.+}} 'char *__bidi_indexable' <BitCast>
// CHECK-NEXT: {{^}}                | |             `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |-OpaqueValueExpr [[ove]]
// CHECK-NEXT: {{^}}                | | `-ImplicitCastExpr {{.+}} 'size_t':'unsigned long' <LValueToRValue>
// CHECK-NEXT: {{^}}                | |   `-DeclRefExpr {{.+}} [[var_n]]
// CHECK-NEXT: {{^}}                | `-OpaqueValueExpr [[ove_1]]
// CHECK-NEXT: {{^}}                |   `-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}                |     |-MaterializeSequenceExpr {{.+}} <Bind>
// CHECK-NEXT: {{^}}                |     | |-BoundsSafetyPointerPromotionExpr {{.+}} 'void *__bidi_indexable'
// CHECK-NEXT: {{^}}                |     | | |-OpaqueValueExpr [[ove_2]] {{.*}} 'void *__single __sized_by(n)':'void *__single'
// CHECK:      {{^}}                |     | | |-ImplicitCastExpr {{.+}} 'void *' <BitCast>
// CHECK-NEXT: {{^}}                |     | | | `-BinaryOperator {{.+}} 'char *' '+'
// CHECK-NEXT: {{^}}                |     | | |   |-CStyleCastExpr {{.+}} 'char *' <BitCast>
// CHECK-NEXT: {{^}}                |     | | |   | `-ImplicitCastExpr {{.+}} 'void *' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                |     | | |   |   `-OpaqueValueExpr [[ove_2]] {{.*}} 'void *__single __sized_by(n)':'void *__single'
// CHECK:      {{^}}                |     | | |   `-AssumptionExpr
// CHECK-NEXT: {{^}}                |     | | |     |-OpaqueValueExpr [[ove_3]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                |     | | |     `-BinaryOperator {{.+}} 'int' '>='
// CHECK-NEXT: {{^}}                |     | | |       |-ImplicitCastExpr {{.+}} 'long' <IntegralCast>
// CHECK-NEXT: {{^}}                |     | | |       | `-OpaqueValueExpr [[ove_3]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                |     | | |       `-ImplicitCastExpr {{.+}} 'long' <IntegralCast>
// CHECK-NEXT: {{^}}                |     | | |         `-IntegerLiteral {{.+}} 0
// CHECK:      {{^}}                |     | |-OpaqueValueExpr [[ove_2]]
// CHECK-NEXT: {{^}}                |     | | `-ImplicitCastExpr {{.+}} 'void *__single __sized_by(n)':'void *__single' <LValueToRValue>
// CHECK-NEXT: {{^}}                |     | |   `-DeclRefExpr {{.+}} [[var_buf]]
// CHECK-NEXT: {{^}}                |     | `-OpaqueValueExpr [[ove_3]]
// CHECK-NEXT: {{^}}                |     |   `-ImplicitCastExpr {{.+}} 'size_t':'unsigned long' <LValueToRValue>
// CHECK-NEXT: {{^}}                |     |     `-DeclRefExpr {{.+}} [[var_n]]
// CHECK-NEXT: {{^}}                |     |-OpaqueValueExpr [[ove_2]] {{.*}} 'void *__single __sized_by(n)':'void *__single'
// CHECK:      {{^}}                |     `-OpaqueValueExpr [[ove_3]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                |-MaterializeSequenceExpr {{.+}} <Unbind>
// CHECK-NEXT: {{^}}                | |-BinaryOperator {{.+}} 'void *__single __sized_by(total_size)':'void *__single' '='
// CHECK-NEXT: {{^}}                | | |-MemberExpr {{.+}} ->payload
// CHECK-NEXT: {{^}}                | | | `-ImplicitCastExpr {{.+}} 'struct accumulator *__bidi_indexable' <LValueToRValue>
// CHECK-NEXT: {{^}}                | | |   `-DeclRefExpr {{.+}} [[var_acc]]
// CHECK-NEXT: {{^}}                | | `-ImplicitCastExpr {{.+}} 'void *__single __sized_by(total_size)':'void *__single' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                | |   `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                | |-OpaqueValueExpr [[ove]] {{.*}} 'size_t':'unsigned long'
// CHECK:      {{^}}                | `-OpaqueValueExpr [[ove_1]] {{.*}} 'void *__bidi_indexable'
// CHECK:      {{^}}                `-ReturnStmt
// XXX: __single isn't automatically added to block return type rdar://132927229
// CHECK-NEXT: {{^}}                  `-ImplicitCastExpr {{.+}} 'void *' <BoundsSafetyPointerCast>
// CHECK-NEXT: {{^}}                    `-ImplicitCastExpr {{.+}} 'void *__bidi_indexable' <BitCast>
// CHECK-NEXT: {{^}}                      `-ImplicitCastExpr {{.+}} 'struct accumulator *__bidi_indexable' <LValueToRValue>
// CHECK-NEXT: {{^}}                        `-DeclRefExpr {{.+}} [[var_acc]]
