// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct P {
  int x;
  int y;

  constexpr P(int x, int y) : x(x), y(y) {}
};

constexpr int f() {
  P p(5, 6);
  return p.x + p.y;
}

using A = int[11];
using A = int[f()];
