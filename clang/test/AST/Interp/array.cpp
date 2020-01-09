// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int *invalid_offset() { // expected-error {{constexpr function never produces a constant expression}}
  int arr[3] = {1, 2};
  return 4 + arr; // expected-note {{cannot refer to element 4 of array of 3 elements in a constant expression}} \
                                                      // expected-note {{cannot refer to element 4 of array of 3 elements in a constant expression}} \
                                                      // expected-warning {{address of stack memory associated with local variable 'arr' returned}}
}

constexpr int *invalid_value = invalid_offset(); // expected-error {{constexpr variable 'invalid_value' must be initialized by a constant expression}}\
                                                      // expected-note {{in call to 'invalid_offset()'}}
