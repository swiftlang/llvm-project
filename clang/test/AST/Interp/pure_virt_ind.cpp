// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify -Wno-invalid-constexpr

struct A {
    constexpr A();
    constexpr virtual void f() const = 0; // expected-note {{declared here}}
};

constexpr void (A::*pF)() const = &A::f;

constexpr A::A() {
  (this->*pF)(); // expected-note {{pure virtual function 'A::f' called}}
}

struct B : public A { // expected-note {{in call to 'A()'}}
    constexpr virtual void f() const {};
};

constexpr B a{}; // expected-error {{constexpr variable 'a' must be initialized by a constant expression}}\
                 // expected-note {{in call to 'B()'}}
