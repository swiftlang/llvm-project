// RUN: %clang_cc1 -std=c++11 %s -verify
// RUN: %clang_cc1 -std=c++11 -fexperimental-new-constant-interpreter %s -verify

void ref_to_int() {
  // expected-note@+1 {{declared here}}
  int x;
  // expected-error@+2 {{constexpr variable 'refX' must be initialized by a constant expression}}
  // expected-note@+1 {{reference to 'x' is not a constant expression}}
  constexpr const int &refX = x;
}

constexpr int x = 0;
constexpr const int &refX = x;
