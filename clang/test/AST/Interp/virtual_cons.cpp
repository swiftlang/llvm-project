// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

struct A {
  virtual constexpr int f() const { return 1; }
  int a = f();
};

struct B : public A {
  virtual constexpr int f() const { return 2; }
  int b = f();
};

struct C : public B {
  virtual constexpr int f() const { return 3; }
  int c = f();
};

constexpr B b{};
using CheckBa = int[1];
using CheckBa = int[b.a];
using CheckBb = int[2];
using CheckBb = int[b.b];

constexpr C c{};
using CheckCa = int[1];
using CheckCa = int[c.a];
using CheckCb = int[2];
using CheckCb = int[c.b];
using CheckCc = int[3];
using CheckCc = int[c.c];

constexpr A a{};
using CheckAa = int[1];
using CheckAa = int[a.a];
