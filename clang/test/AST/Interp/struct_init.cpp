// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct P {
  int x;
};

struct Q {
  int y;
  P p;
};


constexpr int f() {
  Q q = { 1, { 2 } };
  return q.y + q.p.x;
}

using A = int[3];
using A = int[f()];
