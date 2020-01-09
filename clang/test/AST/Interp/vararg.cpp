// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int f(int a, ...) { return a; }

using A = int[f(0)];
using A = int[0];

using B = int[f(1, 0)];
using B = int[1];

using C = int[f(2, "test")];
using C = int[2];

using D = int[f(3, &f)];
using D = int[3];
