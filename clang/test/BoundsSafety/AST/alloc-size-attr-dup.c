// RUN: not %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -ast-dump %s 2>&1 | FileCheck %s
// RUN: not %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -ast-dump %s 2>&1 | FileCheck %s

__attribute__((alloc_size(1))) __attribute__((alloc_size(1))) void * dup_attr(unsigned);

// CHECK: |-FunctionDecl {{.*}} dup_attr 'void *__single __sized_by_or_null(function-parameter-0-0)(unsigned int)'
// CHECK-NEXT: | |-ParmVarDecl {{.*}} 'unsigned int'
// CHECK-NEXT: | `-AllocSizeAttr {{.*}} 1
//
// CHECK-NOT: AllocSizeAttr

__attribute__((alloc_size(1, 2))) __attribute__((alloc_size(2, 1))) void * mismatch_attr(unsigned, unsigned);
void * mismatch_attr(unsigned, unsigned);

// CHECK-NEXT: |-FunctionDecl {{.*}} mismatch_attr 'void *__single __sized_by_or_null(function-parameter-0-0 * function-parameter-0-1)(unsigned int, unsigned int)'
// CHECK-NEXT: | |-ParmVarDecl {{.*}} 'unsigned int'
// CHECK-NEXT: | |-ParmVarDecl {{.*}} 'unsigned int'
// CHECK-NEXT: | `-AllocSizeAttr {{.*}} 1 2
// CHECK-NEXT: `-FunctionDecl {{.*}} mismatch_attr 'void *__single __sized_by_or_null(function-parameter-0-0 * function-parameter-0-1)(unsigned int, unsigned int)'
// CHECK-NEXT:   |-ParmVarDecl {{.*}} 'unsigned int'
// CHECK-NEXT:   |-ParmVarDecl {{.*}} 'unsigned int'
// CHECK-NEXT:   `-AllocSizeAttr {{.*}} Inherited 1 2
//
// CHECK-NOT: AllocSizeAttr
