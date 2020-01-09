// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

struct VA { int va; };
struct VB { int vb; };
struct A : public virtual VA {};
struct B : public virtual VB {};
struct C : public A, public B, public virtual VA {};

C c;

constexpr int *a = &c.va;
