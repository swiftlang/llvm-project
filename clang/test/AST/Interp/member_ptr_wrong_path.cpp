// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct A {
    int a = 1;
};

struct B : public A {
    int b = 2;
};

struct C : public A {
    int c = 2;
};

struct D : public B, public C {
    int d = 3;
};

constexpr D d;
constexpr int (D::*pB) = (int(C::*))((int(A::*))&B::b); // expected-error {{constexpr variable 'pB' must be initialized by a constant expression}}
