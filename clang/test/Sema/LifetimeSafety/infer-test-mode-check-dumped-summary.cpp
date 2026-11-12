// RUN: %clang_cc1 -fsyntax-only -std=c++23 -finterop-lifetime-inference-test-mode -mllvm -debug-only=lifetime-inference %s 2>&1 | FileCheck %s
// REQUIRES: asserts

// The level of an inferred lifetimebound: `lifetimebound(0)` is the value of
// the parameter, `lifetimebound(1)` is what a reference-to-pointer parameter
// refers to. The functions are analyzed until their summaries are stable, so
// each one may be dumped more than once. The parameters have distinct names.

// The referent of `x` flows to the result, not `x` itself.
int *get(int *&x) { return x; }
// CHECK-DAG: x: lifetimebound(1)

// The summary of `get` is used at the call.
int *ret(int *a) { return get(a); }
// CHECK-DAG: a: lifetimebound(0)

// The referent of `y` is stored in a field.
struct S {
  int *f;
  S(int *&y) : f(y) {}
};
// CHECK-DAG: y: lifetimebound(1)

S make(int *b) { return S(b); }
// CHECK-DAG: b: lifetimebound(0)
