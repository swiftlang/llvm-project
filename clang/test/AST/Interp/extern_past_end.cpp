// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

extern int a;

constexpr int *p0 = &*(&a);
constexpr int *p1 = &*(p0 + 1);
