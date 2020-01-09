// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify


constexpr int diff_bad() {                  // expected-error {{constexpr function never produces a constant expression}}
  int a = 0;
  int b = 0;
  return &a - &b;                           // expected-note 2{{subexpression not valid in a constant expression}}
}

constexpr int bad = diff_bad();             // expected-error {{constexpr variable 'bad' must be initialized by a constant expression}}\
                                            // expected-note {{in call to 'diff_bad()'}}}

constexpr int diff_bad_array() {            // expected-error {{constexpr function never produces a constant expression}}
  int a[1] = {0};
  int b[1] = {0};
  return &a[0] - &b[0];                     // expected-note 2{{subexpression not valid in a constant expression}}
}

constexpr int bad_array = diff_bad_array(); // expected-error {{constexpr variable 'bad_array' must be initialized by a constant expression}}\
                                            // expected-note {{in call to 'diff_bad_array()'}}}

constexpr int diff_pos() {
  int a[2] = {0, 1};
  return &a[1] - &a[0];
}

static_assert(diff_pos() == 1);

constexpr int diff_neg() {
  int a[2] = {0, 1};
  return &a[0] - &a[1];
}

static_assert(diff_neg() == -1);


struct S {
  int a[1] = {1};
  int b[1] = {2};
};
constexpr S s;
constexpr int diff_different_arrays() {   // expected-error {{constexpr function never produces a constant expression}}
  return &s.a[0] - &s.b[0];               // expected-note 2{{subtracted pointers are not elements of the same array}}
}

constexpr int different_arrays = diff_different_arrays(); // expected-error {{constexpr variable 'different_arrays' must be initialized by a constant expression}}\
                                                          // expected-note {{in call to 'diff_different_arrays()'}}
