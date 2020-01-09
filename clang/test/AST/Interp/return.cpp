// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int f(int a) {
  if (a < 0)
    return 10;
} // expected-warning {{non-void function does not return a value in all control paths}} \
                              // expected-note {{control reached end of constexpr function}}

constexpr int g(int a) {
  return f(a); // expected-note {{in call to 'f(20)'}}
}

constexpr int h(int a) {
  return g(a); // expected-note {{in call to 'g(20)'}}
}

constexpr int i = h(20); // expected-error {{constexpr variable 'i' must be initialized by a constant expression}} \
                              // expected-note {{in call to 'h(20)'}}
