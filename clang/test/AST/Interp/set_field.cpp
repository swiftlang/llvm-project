// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct S {
  int a = 0;
  int b = 0;

  constexpr S() { b = 2; }
};

constexpr int set_field() {
  S s;
  (s.a = 5) = 6;
  return s.a + s.b;
}

using A = int[8];
using A = int[set_field()];
