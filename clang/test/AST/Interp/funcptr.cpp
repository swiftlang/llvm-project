// RUN: %clang_cc1 -std=c++11 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++11 -fsyntax-only %s -verify

constexpr int f(int a, int b);

constexpr int apply(int (*f)(int, int), int a, int b) {
  return f(a, b);   // expected-note {{undefined function 'g' cannot be used in a constant expression}}\\
                    // expected-note {{subexpression not valid in a constant expression}}
}

constexpr int a = apply(nullptr, 0, 1); // expected-error {{constexpr variable 'a' must be initialized by a constant expression}}\
                                        // expected-note {{in call to 'apply(nullptr, 0, 1)'}}

constexpr int bind_delayed() {
  return apply(f, 1, 2);
}

constexpr int f(int a, int b) {
  return a + b;
}

constexpr int bind_now() {
  return apply(f, 1, 2);
}

using A = int[3];
using A = int[bind_delayed()];
using A = int[bind_now()];

constexpr int g(int a, int b);  // expected-note {{declared here}}

constexpr int bind_never() {
  return apply(g, 1, 2);        // expected-note {{in call to 'apply(&g, 1, 2)'}}
}

constexpr int x = bind_never(); // expected-error {{constexpr variable 'x' must be initialized by a constant expression}}\
                                // expected-note {{in call to 'bind_never()'}}


constexpr int (*fnPtr)(int, int) = f;
static_assert(fnPtr(1, 2) == 3, "oops");


static_assert(f != g, "!=");
static_assert(f != f, "!="); // expected-error {{static_assert failed due to requirement 'f != f' "!="}}
static_assert(f == f, "!=");
static_assert(f == g, "!="); // expected-error {{static_assert failed due to requirement 'f == g' "!="}}

static_assert(&f != nullptr, "!=nullptr");
static_assert(nullptr != &g, "nullptr!=");
