// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics


struct S {
  int x = 1;
};


constexpr int callee(S s, S t) {
  return s.x + t.x;
}

constexpr int caller(S s) {
  return callee(s, {});
}

static_assert(caller({}) == 2);
