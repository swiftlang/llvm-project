// RUN: %clang_cc1 -std=c++14 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++14 -fsyntax-only %s -verify

constexpr int a[2] = {a[1] + 1, a[0] + 1};
using A = int[1];
using A = int[a[0]];
using B = int[2];
using B = int[a[1]];

constexpr int f() {
  constexpr int c[2] = {c[1] + 1, c[0] + 1}; // expected-note {{read of object outside its lifetime is not allowed in a constant expression}}\
                                             // expected-error {{constexpr variable 'c' must be initialized by a constant expression}}
  return c[0];
}

constexpr int b[2][2] = { b[1][1] + 1 };
using C = int[b[0][0]];
using C = int[1];
