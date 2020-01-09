// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr void f(int *a) {
  *a = 5;
}

constexpr void g(int *a) {
  *a = 6;
}

constexpr int test(bool cond) {
  int a = 0;
  cond ? f(&a) : g(&a);
  return a;
}

static_assert(test(true) == 5);
static_assert(test(false) == 6);
