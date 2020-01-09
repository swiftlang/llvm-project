// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fcxx-exceptions -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fcxx-exceptions %s -verify
// XFAIL: *
// expected-no-diagnostics

constexpr int callee(int) {
    return 1;
}

struct FunctionTest {
    int counter = 0;

    constexpr int (*get_callee())(int) {
        ++counter;
        return counter == 1 ? callee : throw;
    }

    constexpr int get_arg() {
        ++counter;
        return counter == 2 ? 1 : throw;
    }
};

constexpr bool check_func_order() {
    FunctionTest s;
    (*s.get_callee())(s.get_arg());
    return true;
}

static_assert(check_func_order());


struct MethodTest {
    int counter = 0;

    constexpr int callee(int) {
        return 1;
    }

    constexpr int (MethodTest::*get_callee())(int) {
        ++counter;
        return counter == 1 ? &MethodTest::callee : throw;
    }

    constexpr int get_arg() {
        ++counter;
        return counter == 2 ? 1 : throw;
    }
};

constexpr bool check_method_order() {
    MethodTest s;
    (s.*s.get_callee())(s.get_arg());
    return true;
}

static_assert(check_method_order());
