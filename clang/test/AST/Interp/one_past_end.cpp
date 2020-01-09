// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify


constexpr void inc() { // expected-error {{constexpr function never produces a constant expression}}
  int a = 0;
  int *b = &a;
  ++b;
  ++b; // expected-note {{cannot refer to element 2 of non-array object in a constant expression}}
}
