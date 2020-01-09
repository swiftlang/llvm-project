// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

struct A {
    int a = 0;
};

struct B : public A { int b = 1;};
struct C : public B { int c = 2;};
struct D : public C { int d = 3;};

constexpr D d{};
constexpr A *ap = (A*)&d;
constexpr D *dp = (D*)ap;
static_assert(dp->d == 3);
