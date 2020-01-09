// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// expected-no-diagnostics

struct A {
  int x = 1;
  constexpr virtual int f() const { return x; }
};

constexpr int (A::*pF) () const = &A::f;

struct B : public A {
  int y = 2;
  constexpr int f() const override { return y; }
};

constexpr A a{};
static_assert((a.*pF)() == 1);

constexpr B b{};
static_assert((b.*pF)() == 2);

// The constexpr tree evaluator incorrectly marks this as never producing a constant.
constexpr int invoke_a() { A a{}; return (a.*pF)(); }
static_assert(invoke_a() == 1);

constexpr int invoke_b() { B b{}; return (b.*pF)(); }
static_assert(invoke_b() == 2);
