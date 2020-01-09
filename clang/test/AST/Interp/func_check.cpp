// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fcxx-exceptions -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fcxx-exceptions %s -verify


// A check must be inserted before the call site to ensure that
// the callee is evaluated before arguments. In the first test,
// the callee is known at compile time, avoiding the check or
// hardcoding it. In the second test, a check is lazily inserted.

// Target known at compile time.

constexpr int known_callable_fn_callee() {
  return 0;
}

int known_non_callable_fn_callee() {
  return 0;
}

constexpr int known_callable_fn_caller(int flag) {
  return flag ? known_callable_fn_callee() : known_non_callable_fn_callee();
}

using A = int[0];
using A = int[known_callable_fn_caller(1)];
using B = int[known_callable_fn_caller(0)]; // expected-error {{variable length array declaration not allowed at file scope}}

// Target not known at compile time.

constexpr int unknown_callable_fn_callee();
int unknown_non_callable_fn_callee();

constexpr int unknown_callable_fn_caller(int flag) {
  return flag ? unknown_callable_fn_callee() : unknown_non_callable_fn_callee();
}

constexpr int unknown_callable_fn_callee() {
  return 0;
}

int unknown_non_callable_fn_callee() {
  return 0;
}

using C = int[0];
using C = int[unknown_callable_fn_caller(1)];
using D = int[unknown_callable_fn_caller(0)]; // expected-error {{variable length array declaration not allowed at file scope}}
