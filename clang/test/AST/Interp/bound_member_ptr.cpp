// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

struct Indirect {
  constexpr int f() const { return 2; };
};

constexpr int (Indirect::*ptrF) () const = &Indirect::f;

using CheckThat = int[2];
using CheckThat = int[(Indirect().*ptrF)()];
