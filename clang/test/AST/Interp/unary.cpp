// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int inc_callee(int a, int b) {
  return a;
}

constexpr int inc_caller() {
  int n = 5;
  return inc_callee(n++, ++n); // expected-warning {{multiple unsequenced modifications to 'n'}}
}
