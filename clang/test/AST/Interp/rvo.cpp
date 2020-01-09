// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct S {
  int x;
  constexpr S(int x) : x(x) {}
};

constexpr S struct_return(int x) {
  return S(x);
}

static_assert(struct_return(5).x == 5);
