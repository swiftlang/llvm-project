// RUN: %clang_cc1 -fbounds-safety -ftyped-memory-operations -fsyntax-only -verify %s

// Check that casts generated as part of bounds check expressions don't trip up
// the type allocator analysis.

// expected-no-diagnostics

#include <ptrcheck.h>

#define _TYPED_ALLOC(rewrite_target, type_param_pos)                           \
  __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

void *__sized_by_or_null(size) typed_alloc(__SIZE_TYPE__ size,
                                           unsigned long long descriptor);
void *__sized_by_or_null(size) my_alloc(__SIZE_TYPE__ size)
    _TYPED_ALLOC(typed_alloc, 1);

struct foo {
  int a;
  int b;
};

struct bar {
  int len;
  struct foo *__sized_by_or_null(len) p;
};

void g(struct bar *s, int n) {
  s->p = my_alloc((__SIZE_TYPE__)n * sizeof(struct foo));
  s->len = n;
}
