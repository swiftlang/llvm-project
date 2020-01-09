// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify


struct S {
  constexpr void f() { }
};

constexpr void f() { // expected-error {{constexpr function never produces a constant expression}}
  S *s = nullptr;
  s->f();           // expected-note {{member call on dereferenced null pointer is not allowed in a constant expression}}
}
