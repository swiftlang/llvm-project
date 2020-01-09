// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify -fexperimental-new-constant-interpreter
// expected-no-diagnostics


struct S {
  int X;

  constexpr S(int X) : X(X) {}

  constexpr S operator+(const S& RHS) {
    return S(X + RHS.X);
  }
};


static_assert((S(2) + S(3)).X == 5);
