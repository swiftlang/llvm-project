// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

using A = int[10];

constexpr int arr1[2][2] = { { 1, 2 }, {3, 4 }};
using A = int[arr1[0][0] + arr1[0][1] + arr1[1][0] + arr1[1][1]];

constexpr int arr2[2][2] = { { 1, 2 }, {3, 4 }};
using A = int[*(arr2[0] + 0) + *(arr2[0] + 1) + *(arr2[1] + 0) + *(arr2[1] + 1)];
