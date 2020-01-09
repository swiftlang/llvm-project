// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr void modifier(int *a) {
  *a = 5; // expected-note {{modification of object of const-qualified type 'const int' is not allowed in a constant expression}}
}

constexpr bool mod_non_const() {
  int a = 0;
  modifier(&a);
  return a == 5;
}

static_assert(mod_non_const());

constexpr bool mod_const() {
  const int a = 0;
  modifier((int *)&a); // expected-note {{in call to 'modifier(&a)'}}
  return a == 5;
}

static_assert(mod_const()); // expected-error {{static_assert expression is not an integral constant expression}} \
                            // expected-note {{in call to 'mod_const()'}}
