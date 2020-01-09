// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int global_prim = 5;

constexpr int get_global_prim() {
  return global_prim;
}

static_assert(get_global_prim() == global_prim);

constexpr int global_array[3] = {1, 2, 3};

constexpr int get_global_array() {
  return global_array[0];
}

static_assert(get_global_array() == 1);

struct S {
  int n;
  constexpr S() : n(5) {}
};

constexpr S s;

static_assert(s.n == 5);
