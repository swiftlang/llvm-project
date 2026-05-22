// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++17 -x c++ -fmodules \
// RUN:   -ftyped-memory-operations -Wtyped-memory-inference-failure -Rtmo-remarks \
// RUN:   -emit-module -fmodule-name=tmoalloc -fmodule-map-file=%t/tmoalloc.modulemap \
// RUN:   %t/tmoalloc.modulemap -verify=module -o %t/tmoalloc.pcm
//
// RUN: %clang_cc1 -std=c++17 -x c++ -fmodules \
// RUN:   -ftyped-memory-operations -Wtyped-memory-inference-failure -Rtmo-remarks \
// RUN:   -fmodule-map-file=%t/tmoalloc.modulemap -fmodule-file=tmoalloc=%t/tmoalloc.pcm \
// RUN:   -fsyntax-only -verify %t/tmo-user.cpp

//--- tmoalloc.modulemap
module tmoalloc {
  header "tmoalloc.h"
}

//--- tmoalloc.h

#ifndef TMOALLOC_H
#define TMOALLOC_H

void *test_typed_malloc(__SIZE_TYPE__ size, unsigned long long);
void *test_malloc(__SIZE_TYPE__ size) __attribute__((typed_memory_operation(test_typed_malloc, 1)));

struct S1 {
  void *p;
  int i;
  int j;
  void (*fptr)();
};

inline S1 *test_inline_tmo_call(int n) {
  return (S1 *)test_malloc(sizeof(S1) * n);
  // module-remark@-1 {{passing TMO information for array of type 'S1' to 'test_typed_malloc' (retargeted from 'test_malloc')}}
  // module-note@-2 {{inferred array of 'S1' from expression 'sizeof(S1) * n'}}
  // module-note@-3 {{encoding array of 'S1' as 74309947616562710. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "Array" ] }, "TypeHash": 4009135638 }}}
}

template <class T> T *alloc_t(T, int n) {
  return (T *)test_malloc(sizeof(T) * n); // #alloc_t
}

inline int *alloc_int(int n) {
  return alloc_t(n, n);
  // module-note@-1 {{in instantiation of function template specialization 'alloc_t<int>' requested here}}
  // module-remark@#alloc_t {{passing TMO information for array of type 'int' to 'test_typed_malloc' (retargeted from 'test_malloc')}}
  // module-note@#alloc_t {{inferred array of 'int' from expression 'sizeof(int) * n'}}
  // module-note@#alloc_t {{encoding array of 'int' as 72058145178419728. { "Summary": { "LayoutSemantics": [ "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "Array" ] }, "TypeHash": 1384677904 }}}
}

inline int *alloc_inference_fail(int n) {
  auto *r = test_malloc(n);
  // module-warning@-1 {{could not infer allocation type in call to 'test_malloc'}}
  // module-note@-2 {{unable to infer allocation type from expression 'n'}}
  return (int *)r;
}

#endif

//--- tmo-user.cpp

#include "tmoalloc.h"

void test_in_module() {
  test_inline_tmo_call(10);
  S1 * s = alloc_t(S1{}, 10);
  // expected-note@-1 {{in instantiation of function template specialization 'alloc_t<S1>' requested here}}
  // expected-remark@tmoalloc.h:23 {{passing TMO information for array of type 'S1' to 'test_typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@tmoalloc.h:23 {{inferred array of 'S1' from expression 'sizeof(S1) * n'}}
  // expected-note@tmoalloc.h:23 {{encoding array of 'S1' as 74309947616562710. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "Array" ] }, "TypeHash": 4009135638 }}}
  alloc_int(10);
  test_malloc(10);
  // expected-warning@-1 {{could not infer allocation type in call to 'test_malloc'}}
  // expected-note@-2 {{unable to infer allocation type from expression '10'}}
}

