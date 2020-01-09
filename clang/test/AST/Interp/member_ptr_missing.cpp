// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct A {
    int a = 1;
};

struct B : public A {
    int b = 2;
};

constexpr A a;

constexpr int (A::*pB) = (int(A::*))&B::b;
static_assert(a.*pB, ""); // expected-error {{static_assert expression is not an integral constant expression}}

constexpr int (A::*pA) = (int(A::*))&B::a;
static_assert(a.*pA == 1, "");

constexpr B b;
constexpr int (B::*pBack) = pB;
static_assert(b.*pBack == 2, "");
