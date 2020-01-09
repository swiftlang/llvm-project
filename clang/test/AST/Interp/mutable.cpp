// RUN: %clang_cc1 -std=c++14 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++14 -fsyntax-only %s -verify
// expected-no-diagnostics

template <int I> struct sink {};

struct A {
    mutable int mutable_field = 0;
    int non_mutable_field = 0;
};

struct B {
    A non_mutable_a;
    mutable A mutable_a;
    int non_mutable_field;
    mutable int mutable_field;

    constexpr B()
        : non_mutable_a()
        , mutable_a()
        , non_mutable_field()
        , mutable_field()
    {
    }
};

sink<((A()).mutable_field)> s0;
sink<((A()).non_mutable_field)> s1;
sink<((B()).mutable_field)> s2;
sink<((B()).non_mutable_field)> s3;
sink<((B()).mutable_a.mutable_field)> s4;
sink<((B()).mutable_a.non_mutable_field)> s5;
sink<((B()).non_mutable_a.mutable_field)> s6;
sink<((B()).non_mutable_a.non_mutable_field)> s7;

constexpr int read_a_mutable_field() {
    A a;
    return a.mutable_field;
}
sink<read_a_mutable_field()> p0;

constexpr int read_a_non_mutable_field() {
    A a;
    return a.non_mutable_field;
}
sink<read_a_non_mutable_field()> p1;
