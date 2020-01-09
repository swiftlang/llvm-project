// RUN: %clang_cc1 %s -std=c++11 -fsyntax-only -verify -triple x86_64-linux-gnu
// RUN: %clang_cc1 %s -std=c++11 -fsyntax-only -verify -triple x86_64-linux-gnu -fexperimental-new-constant-interpreter

struct S;
constexpr int extract(const S &s);

struct S {
  constexpr S() : n(extract(*this)), m(0) {} // expected-note {{in call to 'extract(s1)'}}
  constexpr S(int k) : n(k), m(extract(*this)) {}
  int n, m;
};

constexpr int extract(const S &s) { return s.n; } // expected-note {{read of object outside its lifetime is not allowed in a constant expression}}

void f() {
  constexpr S s1; // expected-error {{constant expression}} \
                  // expected-note {{in call to 'S()'}}
}
