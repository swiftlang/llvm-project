// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify


constexpr int fn_ptr_null() { // expected-error {{constexpr function never produces a constant expression}}
  int (*f)(int) = nullptr;
  return f(5); // expected-note {{subexpression not valid in a constant expression}}
}
