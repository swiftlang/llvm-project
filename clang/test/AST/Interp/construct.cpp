// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct S {
    int x[5] = {};

    constexpr S() {
        x[0] = 1;
    }
};

constexpr const S s{};

struct P {
    int x[5] = {};
    S s;

    constexpr P() {
        f();
    }

    constexpr void f() {
        x[0] = 5;
        s.x[2] = 8;
    }
};

constexpr const P p{};


