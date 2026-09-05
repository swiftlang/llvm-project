// RUN: %clang_cc1 -Rtmo-remarks -Wtyped-memory-inference-conflict -verify -fsyntax-only \
// RUN:            -fbounds-safety -ftyped-memory-operations -triple arm64-apple-macosx -nostdsysteminc -O0 %s

#include <ptrcheck.h>

void *__sized_by_or_null(size)
    typed_malloc(__SIZE_TYPE__ size, unsigned long long);
void *__sized_by_or_null(size) test_malloc(__SIZE_TYPE__ size)
    __attribute__((typed_memory_operation(typed_malloc, 1)));

struct S {
  int i;
  int j;
};

struct S *from_cast(__SIZE_TYPE__ n) {
  return (struct S *)test_malloc(n); // #from_cast
  // expected-remark@#from_cast {{passing TMO information for type 'char' to 'typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@#from_cast {{inferred 'char' from cast of result from call to 'test_malloc(n)'}}
  // expected-note@#from_cast {{encoding 'char' as 72057870920141092. { "Summary": { "LayoutSemantics": [ "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 2004306212 }}}
}

struct S *from_size(void) {
  return (struct S *)test_malloc(sizeof(struct S)); // #from_size
  // expected-remark@#from_size {{passing TMO information for type 'struct S' to 'typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@#from_size {{inferred 'struct S' from expression 'sizeof(struct S)'}}
  // expected-note@#from_size {{encoding 'struct S' as 72057868919062295. { "Summary": { "LayoutSemantics": [ "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 3227415 }}}
}

struct S *via_stmt_expr(__SIZE_TYPE__ n) {
  return (struct S *)({
    (void)0;
    test_malloc(n); // #via_stmt_expr
  });
  // expected-remark@#via_stmt_expr {{passing TMO information for type 'char' to 'typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@#via_stmt_expr {{inferred 'char' from cast of result from call to 'test_malloc(n)'}}
  // expected-note@#via_stmt_expr {{encoding 'char' as 72057870920141092. { "Summary": { "LayoutSemantics": [ "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 2004306212 }}}
}
