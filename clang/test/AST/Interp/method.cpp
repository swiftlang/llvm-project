// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct S {
  const int x;
  constexpr S() : x(42) {}
  constexpr int get_x() { return x; }
};

constexpr int invoke_S_method() {
  return S().get_x();
}

using A = int[42];
using A = int[invoke_S_method()];

struct P {
  const int x;
  constexpr P();
  constexpr int get_x();
};

constexpr P::P() : x(42) {}
constexpr int P::get_x() { return x; }


constexpr int invoke_P_method() {
  return P().get_x();
}

using B = int[42];
using B = int[invoke_P_method()];
