// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

struct S {
  int x;
  int y;
  constexpr S(int x) : x(x), y(0) {}
};

constexpr int inc_to_end() {
  S s(5);
  int *p = &s.x;
  ++p;
  return *p;                                     // expected-note {{read of dereferenced one-past-the-end pointer is not allowed in a constant expression}}
}

constexpr int inc_to_end_val = inc_to_end();     // expected-error {{constexpr variable 'inc_to_end_val' must be initialized by a constant expression}}\
                                                 // expected-note {{in call to 'inc_to_end()'}}

constexpr int inc_past_end() {
  S s(5);
  int *p = &s.x;
  ++p;
  ++p;                                           // expected-note {{cannot refer to element 2 of non-array object in a constant expression}}
  return *p;
}

constexpr int inc_past_end_val = inc_past_end(); // expected-error {{constexpr variable 'inc_past_end_val' must be initialized by a constant expression}}\
                                                 // expected-note {{in call to 'inc_past_end()'}}

constexpr int off_past_end() {
  S s(5);
  int *p = &s.x;
  return *(p + 2);                               // expected-note {{cannot refer to element 2 of non-array object in a constant expression}}
}

constexpr int off_past_end_val = off_past_end(); // expected-error {{constexpr variable 'off_past_end_val' must be initialized by a constant expression}}\
                                                 // expected-note {{in call to 'off_past_end()'}}
