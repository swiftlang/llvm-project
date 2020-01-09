// RUN: %clang_cc1 -triple=avr -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -triple=avr -std=c++2a -fsyntax-only %s -verify


constexpr int diff() {  // expected-error {{constexpr function never produces a constant expression}}
  char a[32769] = {0};
  return &a[32769] - &a[0]; // expected-note {{value 32769 is outside the range of representable values of type 'int'}}
}
