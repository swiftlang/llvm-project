// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

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

constexpr int (D::*pB) = (int(B::*))&A::a;
