// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct S { int x; constexpr S() : x(0) {}; constexpr int f(); };

constexpr int *nullptr_ptr() {    // expected-error {{constexpr function never produces a constant expression}}
  S *s = nullptr;
  return &s->x;                   // expected-note {{cannot access field of null pointer}}
}

constexpr int nullptr_field() {   // expected-error {{constexpr function never produces a constant expression}}
  S *s = nullptr;
  return s->x;                    // expected-note {{cannot access field of null pointer}}
}

constexpr void nullptr_invoke() { // expected-error {{constexpr function never produces a constant expression}}
  S *s = nullptr;
  s->f();                         // expected-note {{member call on dereferenced null pointer is not allowed in a constant expression}}
}
