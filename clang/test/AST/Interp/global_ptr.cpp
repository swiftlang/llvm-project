// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

extern int a;
static_assert(&a < &a + 1);

extern int *b;
static_assert(&b < &b + 1);

extern int c;
static_assert(&c == &c);

extern int d[10];
static_assert(d < d + 10);
static_assert(&d[0] == d);
static_assert((&d[10] - &d[0]) == 10);
