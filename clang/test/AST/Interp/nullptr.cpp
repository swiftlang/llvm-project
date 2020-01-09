// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int nullptr_deref() {   // expected-error {{constexpr function never produces a constant expression}}
  int *a = nullptr;
  return *a;                      // expected-note {{read of dereferenced null pointer is not allowed in a constant expression}}
}

constexpr int zero_deref() {      // expected-error {{constexpr function never produces a constant expression}}
  int *a = (int*)0;
  return *a;                      // expected-note {{read of dereferenced null pointer is not allowed in a constant expression}}
}

constexpr int nullptr_write() {   // expected-error {{constexpr function never produces a constant expression}}
  int *a = (int*)0;
  *a = 5;                         // expected-note {{assignment to dereferenced null pointer is not allowed in a constant expression}}
  return *a;
}

constexpr int *nullptr_inc() {    // expected-error {{constexpr function never produces a constant expression}}
  int *a = (int*)nullptr;
  return a++;                     // expected-note {{cannot perform pointer arithmetic on null pointer}}
}

constexpr int *nullptr_dec() {    // expected-error {{constexpr function never produces a constant expression}}
  int *a = (int*)nullptr;
  return a--;                     // expected-note {{cannot perform pointer arithmetic on null pointer}}
}

constexpr int *nullptr_offset() { // expected-error {{constexpr function never produces a constant expression}}
  int *a = (int*)nullptr;
  return a + 2;                   // expected-note {{cannot perform pointer arithmetic on null pointer}}
}

constexpr int nullptr_ref_int() { // expected-error {{constexpr function never produces a constant expression}}
  int *a = nullptr;
  int &b = *a;
  return b;                       // expected-note {{read of dereferenced null pointer is not allowed in a constant expression}}
}

constexpr int nullptr_ref_inc() { // expected-error {{constexpr function never produces a constant expression}}
  int *a = nullptr;
  int &b = *a;
  return b++;                     // expected-note {{increment of dereferenced null pointer is not allowed in a constant expression}}
}

constexpr int nullptr_ref_dec() { // expected-error {{constexpr function never produces a constant expression}}
  int *a = nullptr;
  int &b = *a;
  return b--;                     // expected-note {{decrement of dereferenced null pointer is not allowed in a constant expression}}
}
