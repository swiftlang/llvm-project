// RUN: rm -rf %t
// RUN: mkdir %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -triple arm64-apple-macosx -std=c++20 %t/M.cppm -ftyped-memory-operations -fsyntax-only -verify
//
// RUN: %clang_cc1 -triple arm64-apple-macosx -std=c++20 %t/M.cppm -ftyped-memory-operations -Rtmo-remarks \
// RUN:   -fsyntax-only 2>&1 | FileCheck %s
//
// CHECK: remark: passing TMO information for array of type 'S1' to 'typed' (retargeted from 'alloc')
// CHECK: remark: passing TMO information for array of type 'int' to 'typed<int>' (retargeted from 'alloc')

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

template <class T> struct ModuleAllocator {
  static T *typed(__SIZE_TYPE__ size, unsigned long long id) {
    return (T *)test_typed_malloc(size, id);
  }
  static T *alloc(__SIZE_TYPE__ size) __attribute__((typed_memory_operation(typed, 1)));
};

struct ModuleHolder {
  template <class U> static void *typed(__SIZE_TYPE__ size, unsigned long long id) {
    return test_typed_malloc(size, id);
  }
};

template <class T> struct ModuleSubstitutedAllocator {
  static void *alloc(__SIZE_TYPE__ size) __attribute__((typed_memory_operation(ModuleHolder::typed<T>, 1)));
};


//--- M.cppm
// expected-no-diagnostics
module;
#include "foo.h"
export module M;
export using ::test_inline_tmo_call;
export using ::alloc_t;
export using ::alloc_int;
export using ::ModuleAllocator;
export using ::ModuleSubstitutedAllocator;


export void test_in_module() {
  test_inline_tmo_call(10);
  alloc_t<S1>(10);
  alloc_int(10);
  ModuleAllocator<S1>::alloc(sizeof(S1) * 10);
  ModuleSubstitutedAllocator<int>::alloc(sizeof(int) * 10);
}
