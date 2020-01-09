// RUN: %clang_cc1 -std=c++17 -fcxx-exceptions -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fcxx-exceptions -fsyntax-only %s -verify

namespace std { struct type_info {}; }

constexpr int iterate(const std::type_info *a, const std::type_info *b) {
  int i = 0;
  for (auto *p = a; p != b; ++p) {
    i += 1;
  }
  return i;
}

constexpr int test() {
    const std::type_info *a = &typeid(int);
    return iterate(a, a + 1);
}

using A = int[test()];
using A = int[1];

constexpr int print(int x, const std::type_info &info) {
    return x ? throw : 0; // expected-note {{subexpression not valid in a constant expression}}
}

static_assert(print(1, typeid(int (*) ())), "print"); // expected-error {{static_assert expression is not an integral constant expression}}\
                                                      // expected-note {{in call to 'print(1, typeid(int (*)()))'}}

