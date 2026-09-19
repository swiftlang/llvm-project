
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s

#include <limits.h>
#include <ptrcheck.h>

int *__counted_by(len * count) foo(int len, short count);
int *__counted_by(len * count) baz(unsigned int len, long long count);

int *__counted_by((int)(len * count)) mul(unsigned len, unsigned count);
int *__counted_by((int)((len) * (count))) mul_paren(unsigned len, unsigned count);

void Test(void) {
  // expected-error@+1{{negative count value of -1 for 'int *__single __counted_by(len * count)'}}
  foo(1, -1);

  // expected-error@+1{{negative count value of -4294967295 for 'int *__single __counted_by(len * count)'}}
  baz(UINT_MAX, -1LL);

  // expected-error@+1{{negative count value of -1 for 'int *__single __counted_by((int)(len * count))'}}
  mul(0xFFFFFFFFu, 1u); // Should error. 

  // expected-error@+1{{negative count value of -1 for 'int *__single __counted_by((int)(len * count))' (aka 'int *__single')}}
  mul(0xFFFFFFFFu, 1u); // Should error.
}
