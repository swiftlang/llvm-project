// RUN: %clang_cc1 -std=c++17 -fsyntax-only -Wno-unused-value -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only -Wno-unused-value %s -verify
// expected-no-diagnostics

struct S {
  int x = 0;
  int y = 9;
};

int s = (((S{1,2}, S{}), S{}), 2);
