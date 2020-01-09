// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

typedef int __attribute__((vector_size(8))) VI2;
typedef int __attribute__((vector_size(16))) VI4;
constexpr auto v0 = VI4{};
constexpr auto v1 = VI4{ 1 };
constexpr auto v2 = VI4{ 1, 2 };
constexpr auto v3 = VI4{ 1, 2, 3, 4 };
