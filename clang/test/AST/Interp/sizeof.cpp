// RUN: %clang_cc1 -triple=x86_64-unknown-linux -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -triple=x86_64-unknown-linux -std=c++17 -fsyntax-only %s -verify

static_assert(sizeof(char) == 1);
static_assert(sizeof(short) == 2);
static_assert(sizeof(int) == 4);
static_assert(sizeof(long) == 8);
static_assert(sizeof(long long) == 8);

struct S; // expected-note {{forward declaration of 'S'}}

constexpr int size_of_bad_struct = sizeof(S); // expected-error {{invalid application of 'sizeof' to an incomplete type 'S'}}


template <typename... Ts>
struct P {
  static constexpr unsigned n = sizeof...(Ts);
};

static_assert(P<int, int, int>::n == 3);
