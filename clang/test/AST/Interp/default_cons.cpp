// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct B {
  int a = 0;
  int b = 2;

  constexpr int f() const { return a; }
};

constexpr static B b;

constexpr int c = b.f();
