// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct S {
  int x;
  constexpr S() : x(1) {}
};

constexpr int f() {
  S arr[2] = { {} };
  return arr[0].x + arr[1].x;
}

using A = int[2];
using A = int[f()];

constexpr int g(S &s) {
  return (&s + 1)->x;
}

constexpr int h() {
  S arr[2] = {};
  return g(arr[0]);
}

using B = int[1];
using B = int[h()];
