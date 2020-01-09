// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct S {
  int x[2];

  constexpr S(int y) : x{y, y} {}
};

constexpr int f() {
  S a(1);
  S b(a);
  return b.x[0] + b.x[1];
}

static_assert(f() == 2);

struct P {
  S s[2];

  constexpr P(int y) : s{y, y} {}
};

constexpr int g() {
  P a(1);
  P b(a);

  return b.s[0].x[0] + b.s[0].x[1] + b.s[1].x[0] + b.s[1].x[1];
}

static_assert(g() == 4);
