// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify


struct S {
  int a;
  int b[10];
};

extern S s; // expected-note {{declared here}}

constexpr int fail_read_extern = s.a; // expected-error {{constexpr variable 'fail_read_extern' must be initialized by a constant expression}}\
                                      // expected-note {{read of non-constexpr variable 's' is not allowed in a constant expression}}\
