// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify

constexpr int uninitialized() {
  int a[2] = { [0] = a[1] };       // expected-note {{read of object outside its lifetime is not allowed in a constant expression}}\
                                   // expected-warning {{array designators are a C99 extension}}
  return a[1];
}

constexpr int i = uninitialized(); // expected-error {{constexpr variable 'i' must be initialized by a constant expression}}\
                                   // expected-note {{in call to 'uninitialized()'}}
