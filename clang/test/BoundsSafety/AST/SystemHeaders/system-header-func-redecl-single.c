// RUN: %clang_cc1 -fsyntax-only -ast-dump -fbounds-safety -I %S/include %s | FileCheck %s
// RUN: %clang_cc1 -fsyntax-only -ast-dump -fbounds-safety -I %S/include -x objective-c -fexperimental-bounds-safety-objc %s | FileCheck %s

#include <ptrcheck.h>
#include <system-header-func-decl.h>

int *__single foo(int *__single *__single);

void bar(void) {
    int *__single s;
    foo(&s);
}
// CHECK: `-FunctionDecl {{.+}} bar 'void (void)'
// CHECK:   `-CompoundStmt
// CHECK:     |-DeclStmt
// CHECK:     | `-VarDecl {{.+}} used s 'int *__single':'int *'
// CHECK:     `-CallExpr {{.+}} 'int *'
// CHECK:       |-ImplicitCastExpr {{.+}} 'int *(*__single)(int **)' <FunctionToPointerDecay>
// CHECK:       | `-DeclRefExpr {{.+}} 'int *(int **)' Function {{.+}} 'foo' 'int *(int **)'
// CHECK:       `-ImplicitCastExpr {{.+}} 'int **' <BoundsSafetyPointerCast>
// CHECK:         `-UnaryOperator {{.+}} 'int *__single*__bidi_indexable' prefix '&' cannot overflow
// CHECK:           `-DeclRefExpr {{.+}} 'int *__single':'int *' lvalue Var {{.+}} 's' 'int *__single':'int *'
