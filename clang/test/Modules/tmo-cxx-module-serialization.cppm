// RUN: rm -rf %t
// RUN: mkdir %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++20 %t/M.cppm -ftyped-memory-operations -fsyntax-only -verify

//--- foo.h

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
}

template <class T> T *alloc_t(int n) {
  return (T *)test_malloc(sizeof(T) * n);
}

inline int *alloc_int(int n) {
  return alloc_t<int>(n);
}


//--- M.cppm
// expected-no-diagnostics
module;
#include "foo.h"
export module M;
export using ::test_inline_tmo_call;
export using ::alloc_t;
export using ::alloc_int;


export void test_in_module() {
  test_inline_tmo_call(10);
  alloc_t<S1>(10);
  alloc_int(10);
}
