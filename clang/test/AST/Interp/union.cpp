// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

union U {
  int a;
  int b;
};


constexpr U u[4] = { { .a = 0 }, { .b = 1 }, { .a = 2 }, { .b = 3 } };
static_assert((&u[1].b)[1] == 2, ""); // expected-error {{constant expression}} expected-note {{read of dereferenced one-past-the-end pointer}}

constexpr U zero_initialise = {};
static_assert(zero_initialise.a == 0);
