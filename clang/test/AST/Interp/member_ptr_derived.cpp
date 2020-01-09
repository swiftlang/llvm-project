// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct A {
  int a = 1;
};

struct B : A {
  int b = 2;
};

struct C : B {
  int c = 3;
};


constexpr int (A::*pA) = &A::a;
constexpr int (B::*pB) = &B::a;
constexpr int (C::*pC) = &C::a;

constexpr A a{};
constexpr B b{};
constexpr C c{};

static_assert(b.*pB == 1);
static_assert(c.*pC == 1);
static_assert(a.*pA == 1);
