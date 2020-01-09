// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct S { const int x; int y; constexpr S() : x(0), y(1) {} };

constexpr int mutate_x() {
  S s;
  int *x = const_cast<int *>(&s.x);
  *x = 5;                           // expected-note {{modification of object of const-qualified type 'const int' is not allowed in a constant expression}}
  return 0;
}

constexpr int x = mutate_x();   // expected-error {{constexpr variable 'x' must be initialized by a constant expression}}\
                                    // expected-note {{in call to 'mutate_x()'}}

constexpr int mutate_y() {
  S s;
  int *y = const_cast<int *>(&s.y);
  *y = 5;
  return *y;
}

using A = int[5];
using A = int[mutate_y()];
