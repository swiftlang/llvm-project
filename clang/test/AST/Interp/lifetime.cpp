// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct S { int x; constexpr S() : x(0) {} };

constexpr int extend_lifetime() {
  const S &s = S();
  return s.x;
}

constexpr int finish_lifetime() {
  return S().x;
}

constexpr int extend_integer() {
  const auto &s = 5;
  return s;
}
using A = int[5];
using A = int[extend_integer()];
