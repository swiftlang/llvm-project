// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int callee(int a) {
  return a + 1000000000; // expected-note {{value 3000000000 is outside the range of representable values of type 'int'}}
}

constexpr int caller(int a) {
  return callee(a + 1000000000); // expected-note {{in call to 'callee(2000000000)'}}
}

constexpr int n = caller(1000000000); // expected-error {{constexpr variable 'n' must be initialized by a constant expression}} \
                                      // expected-note {{in call to 'caller(1000000000)'}}
static_assert(caller(0) == 2000000000);
