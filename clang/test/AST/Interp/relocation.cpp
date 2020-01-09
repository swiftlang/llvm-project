// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int solved_callee();

constexpr int solved_caller() {
  return solved_callee();
}

constexpr int solved_callee() {
  return 5;
}

static_assert(solved_caller() == 5);

constexpr int unsolved_callee(); // expected-note {{declared here}}

constexpr int unsolved_caller() {
  return unsolved_callee(); // expected-note {{undefined function 'unsolved_callee' cannot be used in a constant expression}}
}

static_assert(unsolved_caller() == 5); // expected-error {{static_assert expression is not an integral constant expression}} \
                                       // expected-note {{in call to 'unsolved_caller()'}}
