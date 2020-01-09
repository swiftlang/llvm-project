// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct A {
    int x = 1;
};

struct B {
    int x = 2;
};

struct C : public A, public B {
};


constexpr int (C::*pA) = &A::x;
constexpr int (C::*pB) = &B::x;

constexpr C c;

using Ax = int[1];
using Ax = int[c.*pA];


using Bx = int[2];
using Bx = int[c.*pB];
