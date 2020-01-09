// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr bool get_boolean(int a) {
  return a < 0 ? true : false;
}

static_assert(get_boolean(-1));
static_assert(!get_boolean(1));
