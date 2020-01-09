// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct AggregateInit {
  const char &c;
};

constexpr AggregateInit agg1 = { "hello"[0] };

using A = int[5];
using A = int[__builtin_strlen(&agg1.c)];
