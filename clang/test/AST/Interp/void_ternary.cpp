// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fcxx-exceptions -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fcxx-exceptions %s -verify
// expected-no-diagnostics

constexpr bool f(bool flag) {
  flag ? void() : throw;
  return true;
}

static_assert(f(true));
