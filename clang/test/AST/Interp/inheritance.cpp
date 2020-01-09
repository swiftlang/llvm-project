// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct A {
  int x = 1;
};

struct B {
  int y = 10;
};

struct C {
  int z = 100;
};

struct D : public A, public B, public C {
  int w = 1000;
};

constexpr int f() {
  D d;
  return d.x + d.y + d.z + d.w;
}

using CheckF = int[1111];
using CheckF = int[f()];
