// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fcxx-exceptions -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fcxx-exceptions %s -verify

struct A {
    int field = 0;

    constexpr int fn() { return 0; }
};

constexpr int call_field(int (A::*p), bool flag = true) {
    return flag ? throw : 0;                              // expected-note 2{{subexpression not valid in a constant expression}}
}

constexpr int a0 = call_field(&A::field);                 // expected-error {{constexpr variable 'a0' must be initialized by a constant expression}}\
                                                          // expected-note {{in call to 'call_field(&A::field, true)'}}
constexpr int a1 = call_field(nullptr);                   // expected-error {{onstexpr variable 'a1' must be initialized by a constant expression}}\
                                                          // expected-note {{in call to 'call_field(0, true)'}}
constexpr int call_fn(int (A::*p)(), bool flag = true) {
    return flag ? throw : 0;                              // expected-note 2{{subexpression not valid in a constant expression}}
}

constexpr int a2 = call_fn(&A::fn);                       // expected-error {{constexpr variable 'a2' must be initialized by a constant expression}}\
                                                          // expected-note {{in call to 'call_fn(&A::fn, true)'}}
constexpr int a3 = call_fn(nullptr);                      // expected-error {{constexpr variable 'a3' must be initialized by a constant expression}}\
                                                          // expected-note {{in call to 'call_fn(0, true)'}}
