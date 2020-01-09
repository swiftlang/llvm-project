// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int and_short_circuit() {
  int a = 0;
  if (0 && ++a) {
    return 0;
  }
  return a == 0;
}

static_assert(and_short_circuit());

constexpr int or_short_circuit() {
  int a = 0;
  if (1 || ++a) {
    return a == 0;
  }
  return 0;
}

static_assert(or_short_circuit());
