// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int f(int n) {
  switch (n) {
    case 0 ... 5: return 1;
    case 10 ... 20: return 2;
    default: return 3;
  }
}

using A = int[1];
using A = int[f(2)];
using B = int[2];
using B = int[f(12)];
using C = int[3];
using C = int[f(22)];
